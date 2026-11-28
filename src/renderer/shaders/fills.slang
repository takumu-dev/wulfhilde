// Fills an opening past the view limits flat, as the view it is seen in sees it: with the light that
// a grey surface reflecting 18% of it would give back of the ambient light of the sector beyond.
#language slang 2026
import views;

// An opening filled flat.
struct FillShading {
    // The corners of the opening, in order round it, in the coordinates of the view it is seen in.
    float3 opening[6];
    // The light it gives toward the eye, in each channel, in nits.
    float3 radiance;
    // Whether the opening's plane lies so near the eye that the near plane would take away some of
    // what the screen shows of it, which is then drawn as near as depth goes.
    uint opening_at_eye;
};

struct FillConstants {
    // The view the opening is seen in.
    ViewShading* view;
    // The opening.
    FillShading* fill;
};

[vk::push_constant] ConstantBuffer<FillConstants> fills;

struct Placed {
    float4 position : SV_Position;
    float clip : SV_ClipDistance;
};

// The opening's triangles, a fan from its first corner, by its corners. Those past its last corner
// have no area.
static const uint corners[12] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5};

// One corner of the opening's triangles, clipped by the plane of the view it is seen in.
[shader("vertex")]
Placed place_fill(uint vertex : SV_VulkanVertexID) {
    let view = fills.view;
    let fill = fills.fill;
    let corner = fill.opening[corners[vertex]];
    var position = mul(view.view_projection, float4(corner, 1.0));
    if (fill.opening_at_eye != 0) {
        position.z = position.w;
    }
    Placed placed;
    placed.position = position;
    placed.clip = beyond_clip(*view, corner);
    return placed;
}

[shader("fragment")]
float4 fill_flat() : SV_Target {
    return float4(fills.fill.radiance, 1.0);
}
