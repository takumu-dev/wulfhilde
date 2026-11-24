// Draws the surfaces of sectors into the scene, in linear light: each in its material's base colour,
// its colour texture times its factor, under its sector's ambient light, a Lambertian surface lit
// equally from every direction.
#language slang 2026
import bindless;

// One corner of a triangle of a sector's surface, in the sector's coordinates, with what shades it.
struct SectorVertex {
    float3 position;
    // Where the material's textures are sampled, in repeats from an image's top-left corner.
    float2 texture_coordinates;
    uint material;
    uint sector;
};

struct MaterialShading {
    // What the colour texture is multiplied by, in linear Rec.709.
    float3 base_color;
    // The colour texture, by its index among the bindless set's sampled images: one of white for a
    // material without one.
    uint2 color_texture;
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
    // The sampler that every texture is read with, by its index among the bindless set's samplers.
    uint2 texture_sampler;
};

[vk::push_constant] ConstantBuffer<SceneConstants> scene;

static const float pi = 3.14159265358979;

struct Placed {
    float4 position : SV_Position;
    float2 texture_coordinates : TEXCOORD;
    nointerpolation uint material : MATERIAL;
    nointerpolation uint sector : SECTOR;
};

[shader("vertex")]
Placed place(uint vertex : SV_VulkanVertexID) {
    let corner = scene.vertices[vertex];
    Placed placed;
    placed.position = mul(scene.view_projection, float4(corner.position, 1.0));
    placed.texture_coordinates = corner.texture_coordinates;
    placed.material = corner.material;
    placed.sector = corner.sector;
    return placed;
}

// The luminance leaving the surface toward the eye, in nits in each channel.
[shader("fragment")]
float4 shade(Placed placed) : SV_Target {
    let material = scene.materials[placed.material];
    // Neighbouring pixels may show different materials, so the texture's index may differ across
    // the pixels that shade together.
    let texture = DescriptorHandle<Texture2D<float4>>(material.color_texture);
    let texture_sampler = DescriptorHandle<SamplerState>(scene.texture_sampler);
    let texel = nonuniform(texture).Sample(texture_sampler, placed.texture_coordinates).rgb;
    let albedo = material.base_color * texel;
    let illuminance = scene.sectors[placed.sector].ambient;
    return float4(albedo * illuminance / pi, 1.0);
}
