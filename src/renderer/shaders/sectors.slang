// Draws the surfaces of sectors into the scene, in linear light: each in its material's base colour
// under its sector's ambient light, a Lambertian surface lit equally from every direction.
#language slang 2026

// One corner of a triangle of a sector's surface, in the sector's coordinates, with what shades it.
struct SectorVertex {
    float3 position;
    uint material;
    uint sector;
};

struct MaterialShading {
    // The base colour, in linear Rec.709.
    float3 base_color;
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
};

[vk::push_constant] ConstantBuffer<SceneConstants> scene;

static const float pi = 3.14159265358979;

struct Placed {
    float4 position : SV_Position;
    nointerpolation uint material : MATERIAL;
    nointerpolation uint sector : SECTOR;
};

[shader("vertex")]
Placed place(uint vertex : SV_VulkanVertexID) {
    let corner = scene.vertices[vertex];
    Placed placed;
    placed.position = mul(scene.view_projection, float4(corner.position, 1.0));
    placed.material = corner.material;
    placed.sector = corner.sector;
    return placed;
}

// The luminance leaving the surface toward the eye, in nits in each channel.
[shader("fragment")]
float4 shade(Placed placed) : SV_Target {
    let albedo = scene.materials[placed.material].base_color;
    let illuminance = scene.sectors[placed.sector].ambient;
    return float4(albedo * illuminance / pi, 1.0);
}
