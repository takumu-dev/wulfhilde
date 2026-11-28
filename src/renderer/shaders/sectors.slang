// Draws the surfaces of sectors into the scene, in linear light: each in its material's base colour,
// its colour texture times its factor, under its sector's ambient light, a Lambertian surface lit
// equally from every direction. A sky shows its backdrop instead, infinitely far away, in the
// direction it is seen in its sector's coordinates.
#language slang 2026
import bindless;

// One corner of a triangle of a sector's surface or sky, in the sector's coordinates, with what
// shades it.
struct SectorVertex {
    float3 position;
    // Where the material's textures are sampled, in repeats from an image's top-left corner.
    float2 texture_coordinates;
    // What shades it, by its index among the materials: the surface's material, or the sky.
    uint material;
    uint sector;
};

// A material, or a sky.
struct MaterialShading {
    // What the colour texture is multiplied by, in linear Rec.709: for a sky, its luminance of
    // white in nits.
    float3 base_color;
    // The colour texture, by its index among the bindless set's sampled images: one of white for a
    // material without one, and the backdrop's image for a sky.
    uint2 color_texture;
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

// The luminance leaving the surface or the sky toward the eye, in nits in each channel.
[shader("fragment")]
float4 shade(Placed placed) : SV_Target {
    let material = scene.materials[placed.material];
    // Neighbouring pixels may show different materials, so the texture's index may differ across
    // the pixels that shade together.
    let texture = DescriptorHandle<Texture2D<float4>>(material.color_texture);
    // A material is the same across a triangle, and the pixels that shade together are of one
    // triangle, so they take the same branch and their derivatives hold.
    if (material.sky != 0) {
        let backdrop_sampler = DescriptorHandle<SamplerState>(scene.backdrop_sampler);
        let texel = backdrop_texel(nonuniform(texture), backdrop_sampler, placed.point - scene.eye);
        return float4(material.base_color * texel, 1.0);
    }
    let texture_sampler = DescriptorHandle<SamplerState>(scene.texture_sampler);
    let texel = nonuniform(texture).Sample(texture_sampler, placed.texture_coordinates).rgb;
    let albedo = material.base_color * texel;
    let illuminance = scene.sectors[placed.sector].ambient;
    return float4(albedo * illuminance / pi, 1.0);
}
