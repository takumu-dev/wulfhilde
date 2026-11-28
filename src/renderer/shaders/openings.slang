// Draws the opening a view is seen through, as the view it is seen from sees it, into the stencil
// and the depth that confine the view to it: where the opening shows, where nothing yet lies in
// front of it, and back where it lies once the view is drawn.
#language slang 2026
import views;

struct OpeningConstants {
    // The view seen through the opening.
    ViewShading* view;
    // The view it is seen from.
    ViewShading* seen_from;
};

[vk::push_constant] ConstantBuffer<OpeningConstants> openings;

struct Placed {
    float4 position : SV_Position;
    float clip : SV_ClipDistance;
};

// The opening's triangles, a fan from its first corner, by its corners. Those past its last
// corner have no area.
static const uint corners[12] = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5};

// One corner of the opening's triangles, clipped by the plane of the view it is seen from. An eye
// that stands in the opening sees it nearer than the near plane, which would take away what it
// shows there, so such an opening is drawn as near as depth goes, all of it alike. Nothing of the
// view it is seen from can lie between it and the eye there.
Placed place_corner(uint vertex) {
    let view = openings.view;
    let seen_from = openings.seen_from;
    let corner = view.opening[corners[vertex]];
    var position = mul(seen_from.view_projection, float4(corner, 1.0));
    if (view.opening_at_eye != 0) {
        position.z = position.w;
    }
    Placed placed;
    placed.position = position;
    placed.clip = beyond_clip(*seen_from, corner);
    return placed;
}

[shader("vertex")]
Placed place_opening(uint vertex : SV_VulkanVertexID) {
    return place_corner(vertex);
}

// The same corner infinitely far away, which takes away the depth of what lies within the opening.
[shader("vertex")]
Placed place_opening_far(uint vertex : SV_VulkanVertexID) {
    var placed = place_corner(vertex);
    placed.position.z = 0.0;
    return placed;
}

[shader("fragment")]
void cover() {}
