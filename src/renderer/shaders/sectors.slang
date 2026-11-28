// Draws the surfaces of sectors into the scene, in linear light: each in its material, read from
// its factors and textures, reflecting its sector's ambient light, a uniform environment of light
// from every direction, as glTF's metallic-roughness BRDF does, darkened by its ambient occlusion,
// and glowing with its emission, which lights nothing else. A sky shows its backdrop instead,
// infinitely far away, in the direction it is seen in its sector's coordinates.
#language slang 2026
import bindless;
import brdf;

// One corner of a triangle of a sector's surface or sky, in the sector's coordinates, with what
// shades it.
struct SectorVertex {
    float3 position;
    // Where the material's textures are sampled, in repeats from an image's top-left corner.
    float2 texture_coordinates;
    // Out of the surface, into the sector.
    float3 normal;
    // The way u runs on the surface, and in w, 1 where v runs the way of the normal's cross product
    // with it and -1 where it runs the other way.
    float4 tangent;
    // What shades it, by its index among the materials: the surface's material, or the sky.
    uint material;
    uint sector;
};

// A property of one value: a factor times one channel of a texture.
struct ValueShading {
    float factor;
    // By its index among the bindless set's sampled images: white for a material without one.
    uint2 texture;
    // Which of the texel's red, green, blue and alpha it reads, from 0 to 3.
    uint channel;
};

// A material, or a sky.
struct MaterialShading {
    // What the colour texture is multiplied by, in linear Rec.709: for a sky, its luminance of
    // white in nits.
    float3 base_color;
    // The colour texture, by its index among the bindless set's sampled images: white for a
    // material without one, and the backdrop's image for a sky. Each texture below is white when
    // the material lacks it.
    uint2 color_texture;
    ValueShading roughness;
    ValueShading metallic;
    ValueShading occlusion;
    // How strongly the occlusion applies: 0 for a material without one.
    float occlusion_strength;
    uint2 normal_texture;
    // What the normal texture's u and v components are multiplied by: 0 for a material without
    // one, which flattens white into the surface's own normal.
    float normal_scale;
    // What the emission texture is multiplied by, in nits in each channel: 0 for a material that
    // doesn't emit.
    float3 emission;
    uint2 emission_texture;
    // Whether it shades a sky, whose texture is sampled by the direction it is seen in.
    uint sky;
};

struct SectorShading {
    // The ambient light's illuminance in each channel, in lux.
    float3 ambient;
};

struct SceneConstants {
    // From the sector's coordinates into clip space, with depth reversed and no far plane.
    float4x4 view_projection;
    SectorVertex* vertices;
    MaterialShading* materials;
    SectorShading* sectors;
    // The sampler that every material's texture is read with, by its index among the bindless
    // set's samplers.
    uint2 texture_sampler;
    // Where the eye is, in the sector's coordinates.
    float3 eye;
    // The sampler that every backdrop's image is read with, which doesn't repeat it over the top.
    uint2 backdrop_sampler;
    // Rounds the size up to a whole number of the pointers, as C++ lays the constants out.
    float unused;
};

[vk::push_constant] ConstantBuffer<SceneConstants> scene;

static const float pi = 3.14159265358979;

struct Placed {
    float4 position : SV_Position;
    // Where the fragment is, in the sector's coordinates.
    float3 point : POINT;
    float2 texture_coordinates : TEXCOORD;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    nointerpolation uint material : MATERIAL;
    nointerpolation uint sector : SECTOR;
};

[shader("vertex")]
Placed place(uint vertex : SV_VulkanVertexID) {
    let corner = scene.vertices[vertex];
    Placed placed;
    placed.position = mul(scene.view_projection, float4(corner.position, 1.0));
    placed.point = corner.position;
    placed.texture_coordinates = corner.texture_coordinates;
    placed.normal = corner.normal;
    placed.tangent = corner.tangent;
    placed.material = corner.material;
    placed.sector = corner.sector;
    return placed;
}

// The texel of an equirectangular backdrop's image seen in a direction: its centre column faces +X,
// moving left in it turns anticlockwise toward +Y, and its top row is straight up.
float3 backdrop_texel(Texture2D<float4> image, SamplerState image_sampler, float3 direction) {
    let toward = normalize(direction);
    let u = 0.5 - (atan2(toward.y, toward.x) / (2.0 * pi));
    let v = acos(clamp(toward.z, -1.0, 1.0)) / pi;
    // u wraps from 1 to 0 toward -X, where its derivatives would be nearly a whole turn and pick the
    // coarsest mip level. Away from there u changes across neighbouring pixels by far less than half
    // a turn, so its derivatives are taken as the nearest change, less any whole turns.
    let u_x = ddx(u);
    let u_y = ddy(u);
    return image
        .SampleGrad(image_sampler, float2(u, v), float2(u_x - round(u_x), ddx(v)),
                    float2(u_y - round(u_y), ddy(v)))
        .rgb;
}

// Reads a material's colour, normal and emission textures, and its single values. Neighbouring
// pixels may show different materials, so a texture's index may differ across the pixels that
// shade together.
float4 texel_of(uint2 texture, SamplerState texture_sampler, float2 coordinates) {
    let image = DescriptorHandle<Texture2D<float4>>(texture);
    return nonuniform(image).Sample(texture_sampler, coordinates);
}

float value_of(ValueShading value, SamplerState texture_sampler, float2 coordinates) {
    return value.factor * texel_of(value.texture, texture_sampler, coordinates)[value.channel];
}

// The luminance leaving the surface or the sky toward the eye, in nits in each channel.
[shader("fragment")]
float4 shade(Placed placed) : SV_Target {
    let material = scene.materials[placed.material];
    // A material is the same across a triangle, and the pixels that shade together are of one
    // triangle, so they take the same branch and their derivatives hold.
    if (material.sky != 0) {
        let texture = DescriptorHandle<Texture2D<float4>>(material.color_texture);
        let backdrop_sampler = DescriptorHandle<SamplerState>(scene.backdrop_sampler);
        let texel = backdrop_texel(nonuniform(texture), backdrop_sampler, placed.point - scene.eye);
        return float4(material.base_color * texel, 1.0);
    }
    let texture_sampler = DescriptorHandle<SamplerState>(scene.texture_sampler);
    let at = placed.texture_coordinates;

    // The normal texture tilts the surface's normal in the frame of its material's u and v, as
    // glTF has it, with +v green.
    let surface_normal = normalize(placed.normal);
    let along_u = placed.tangent.xyz;
    let tangent = normalize(along_u - (surface_normal * dot(surface_normal, along_u)));
    let bitangent = placed.tangent.w * cross(surface_normal, tangent);
    var tilt = (texel_of(material.normal_texture, texture_sampler, at).xyz * 2.0) - 1.0;
    tilt.xy *= material.normal_scale;
    let normal = normalize((tangent * tilt.x) + (bitangent * tilt.y) + (surface_normal * tilt.z));

    let roughness = saturate(value_of(material.roughness, texture_sampler, at));
    let response = Response(
        material.base_color * texel_of(material.color_texture, texture_sampler, at).rgb,
        saturate(value_of(material.metallic, texture_sampler, at)),
        widened_roughness(roughness, ddx(normal), ddy(normal)));
    let toward_eye = normalize(scene.eye - placed.point);
    let occlusion = 1.0 + (material.occlusion_strength
                           * (saturate(value_of(material.occlusion, texture_sampler, at)) - 1.0));
    // A uniform environment that gives an illuminance to a surface facing any way has a radiance of
    // that illuminance over pi.
    let ambient = scene.sectors[placed.sector].ambient / pi;
    let reflected = ambient_response(response, dot(normal, toward_eye)) * ambient * occlusion;
    let emitted =
        material.emission * texel_of(material.emission_texture, texture_sampler, at).rgb;
    return float4(reflected + emitted, 1.0);
}
