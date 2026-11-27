// Culls the surfaces of the sectors each view sees against that view's frustum, narrowed to the
// window of the screen each sector is seen through, and writes a draw for each surface left, in the
// order of the surfaces, for one multi-draw per view.
#language slang 2026

// One of a sector's surfaces: its triangles' vertices among the map's, and the box that holds them,
// along the axes of the sector's coordinates.
struct SurfaceDraw {
    uint first_vertex;
    uint vertex_count;
    float3 lowest;
    float3 highest;
};

// A sector that a view sees: the window of the screen it is seen through, in normalised device
// coordinates, its surfaces among the map's, and where they start among the view's candidates.
struct SectorInView {
    float2 window_min;
    float2 window_max;
    uint first_surface;
    uint surface_count;
    uint first_candidate;
};

// A view: from its sector's coordinates into clip space, its sectors among the frame's, and its
// draws among the frame's, with room for a draw for each candidate: each surface of its sectors.
struct ViewCulling {
    float4x4 view_projection;
    uint first_sector;
    uint sector_count;
    uint first_draw;
    uint candidate_count;
};

// A draw of a range of vertices, as Vulkan reads it from a buffer.
struct DrawCommand {
    uint vertex_count;
    uint instance_count;
    uint first_vertex;
    uint first_instance;
};

struct CullingConstants {
    ViewCulling* views;
    SectorInView* sectors;
    SurfaceDraw* surfaces;
    DrawCommand* draws;
    // How many draws each view's multi-draw makes, by the view's index.
    uint* draw_counts;
};

[vk::push_constant] ConstantBuffer<CullingConstants> culling;

static const uint group_size = 64;

// Which of the group's candidates are in sight, so that each thread can count those before its own.
groupshared uint group_seen[group_size];

// Whether any of a box lies within what a view sees through a window. It doesn't when every corner
// lies beyond one plane of those that bound it: the near plane, nearer than which depth passes 1,
// or one of the four through the eye and the window's edges.
bool in_sight(float4x4 view_projection, float3 lowest, float3 highest, float2 window_min,
              float2 window_max) {
    bool all_near = true;
    bool all_left = true;
    bool all_right = true;
    bool all_above = true;
    bool all_below = true;
    for (uint corner = 0; corner < 8; ++corner) {
        let point = float3((corner & 1) != 0 ? highest.x : lowest.x,
                           (corner & 2) != 0 ? highest.y : lowest.y,
                           (corner & 4) != 0 ? highest.z : lowest.z);
        let clip = mul(view_projection, float4(point, 1.0));
        all_near = all_near && clip.z > clip.w;
        all_left = all_left && clip.x < window_min.x * clip.w;
        all_right = all_right && clip.x > window_max.x * clip.w;
        all_above = all_above && clip.y < window_min.y * clip.w;
        all_below = all_below && clip.y > window_max.y * clip.w;
    }
    return !(all_near || all_left || all_right || all_above || all_below);
}

// The index among the frame's of the view's sector that a candidate belongs to: the last whose
// candidates start at or before it.
uint sector_of(ViewCulling view, uint candidate) {
    uint low = 0;
    uint high = view.sector_count;
    while (high - low > 1) {
        let middle = (low + high) / 2;
        if (culling.sectors[view.first_sector + middle].first_candidate <= candidate) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return view.first_sector + low;
}

// One group for each view, which takes its candidates a group's worth at a time, and writes the
// draws of those in sight after the draws before them, so that they keep the surfaces' order.
[shader("compute")]
[numthreads(group_size, 1, 1)]
void cull(uint3 group : SV_GroupID, uint3 thread : SV_GroupThreadID) {
    let view = culling.views[group.x];
    uint drawn = 0;
    for (uint start = 0; start < view.candidate_count; start += group_size) {
        let candidate = start + thread.x;
        bool seen = false;
        DrawCommand command = {};
        if (candidate < view.candidate_count) {
            let sector = culling.sectors[sector_of(view, candidate)];
            let surface =
                culling.surfaces[sector.first_surface + candidate - sector.first_candidate];
            seen = in_sight(view.view_projection, surface.lowest, surface.highest,
                            sector.window_min, sector.window_max);
            command.vertex_count = surface.vertex_count;
            command.instance_count = 1;
            command.first_vertex = surface.first_vertex;
        }
        group_seen[thread.x] = seen ? 1 : 0;
        GroupMemoryBarrierWithGroupSync();
        uint before = 0;
        uint all = 0;
        for (uint other = 0; other < group_size; ++other) {
            before += other < thread.x ? group_seen[other] : 0;
            all += group_seen[other];
        }
        if (seen) {
            culling.draws[view.first_draw + drawn + before] = command;
        }
        drawn += all;
        // Every thread has counted before the next candidates take the group's places.
        GroupMemoryBarrierWithGroupSync();
    }
    if (thread.x == 0) {
        culling.draw_counts[group.x] = drawn;
    }
}
