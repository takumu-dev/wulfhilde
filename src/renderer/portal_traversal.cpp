#include "portal_traversal.hpp"

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/plane.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/wall.hpp>
#include <wulfhilde/world/wall_face.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

// How near the eye a surface can be and still be drawn, in metres. Depth is reversed, so a fixed
// near plane loses no precision far away, and there is no far plane at all.
constexpr float near_plane = 0.05F;

// The steps that windows are rounded out to, in normalised device coordinates: about a pixel of an
// image 2048 pixels across. A window that grows grows by whole steps, so the walk ends.
constexpr float window_step = 1.0F / 1024.0F;

// How a camera projects: from its sector's coordinates into Vulkan's clip space, and how far the
// screen reaches to either side and up and down, as the tangents of the angles to its edges.
struct Projection {
    Matrix4 view_projection;
    float half_width = 0.0F;
    float half_height = 0.0F;
};

// From a camera's sector's coordinates into Vulkan's clip space: x to the right, y down, and depth
// from 1 at the near plane to 0 infinitely far away.
Projection projection_of(const Camera& camera, Extent extent) {
    // Into the eye's own axes, as the camera's orientation has them: forward, left and up.
    const auto view = to_matrix(inverse(RigidTransform{
        .rotation = camera.orientation,
        .translation = camera.location.point,
    }));
    const auto vertical =
        1.0F / tan(Angle::radians(camera.vertical_field_of_view.in_radians() / 2.0F));
    const auto horizontal =
        vertical * static_cast<float>(extent.height) / static_cast<float>(extent.width);
    // Left is to the left and up is up, so each turns negative in the image, and depth is the near
    // plane over the distance forward, which divides through as w.
    const Matrix4 projection{
        .columns =
            {
                Vector4{.w = 1.0F},
                Vector4{.x = -horizontal},
                Vector4{.y = -vertical},
                Vector4{.z = near_plane},
            },
    };
    return {
        .view_projection = projection * view,
        .half_width = 1.0F / horizontal,
        .half_height = 1.0F / vertical,
    };
}

// How much farther from the eye than straight ahead of it a point on the screen can be at most: the
// distance to a corner of the screen at a distance of 1 ahead.
float spread_of(const Projection& projection) {
    return std::sqrt(1.0F + (projection.half_width * projection.half_width) +
                     (projection.half_height * projection.half_height));
}

// A polygon in clip space.
using ClipPolygon = std::vector<Vector4>;

// Keeps the part of a polygon where a linear function of its points is 0 or more.
template <typename Distance> ClipPolygon clip(const ClipPolygon& polygon, Distance distance) {
    ClipPolygon kept;
    if (polygon.empty()) {
        return kept;
    }
    auto from = polygon.back();
    auto from_distance = distance(from);
    for (const auto& to : polygon) {
        const auto to_distance = distance(to);
        if ((from_distance >= 0.0F) != (to_distance >= 0.0F)) {
            kept.push_back(lerp(from, to, from_distance / (from_distance - to_distance)));
        }
        if (to_distance >= 0.0F) {
            kept.push_back(to);
        }
        from = to;
        from_distance = to_distance;
    }
    return kept;
}

// The window in which a camera sees a wall's opening within the window of the wall's sector, or
// nothing if it sees none of it.
std::optional<ScreenWindow> window_through(const Wall& wall, const std::vector<Vector3>& opening,
                                           const ScreenWindow& window, const Camera& camera,
                                           const Projection& projection) {
    // The wall faces its sector, to its left, and an eye behind it sees it from the back, through
    // which nothing is seen.
    const auto along = wall.end - wall.start;
    const Vector2 eye{.x = camera.location.point.x, .y = camera.location.point.y};
    const auto in_front = perp_dot(along, eye - wall.start);
    if (in_front < -near_plane * length(along)) {
        return std::nullopt;
    }
    if (in_front < near_plane * length(along)) {
        return window;
    }

    ClipPolygon polygon;
    for (const auto& corner : opening) {
        polygon.push_back(projection.view_projection *
                          Vector4{.x = corner.x, .y = corner.y, .z = corner.z, .w = 1.0F});
    }
    // Every point of the opening that the screen shows is at least the near plane's distance from
    // the eye, and the farthest a direction on the screen strays from straight ahead is less than
    // `spread` times its distance ahead. Half that depth keeps every such point, and takes away
    // what is behind the eye, which the planes through the window's edges would otherwise keep.
    const auto least_depth = 0.5F * near_plane / spread_of(projection);
    polygon = clip(polygon, [&](const Vector4& p) { return p.w - least_depth; });
    polygon = clip(polygon, [&](const Vector4& p) { return p.x - (window.min.x * p.w); });
    polygon = clip(polygon, [&](const Vector4& p) { return (window.max.x * p.w) - p.x; });
    polygon = clip(polygon, [&](const Vector4& p) { return p.y - (window.min.y * p.w); });
    polygon = clip(polygon, [&](const Vector4& p) { return (window.max.y * p.w) - p.y; });
    if (polygon.empty()) {
        return std::nullopt;
    }

    ScreenWindow seen{.min = window.max, .max = window.min};
    for (const auto& point : polygon) {
        const Vector2 screen{.x = point.x / point.w, .y = point.y / point.w};
        seen.min = {.x = std::min(seen.min.x, screen.x), .y = std::min(seen.min.y, screen.y)};
        seen.max = {.x = std::max(seen.max.x, screen.x), .y = std::max(seen.max.y, screen.y)};
    }
    // Rounding out leaves the window on the steps, as its sector's window is, and within it.
    const auto down = [](float value) { return std::floor(value / window_step) * window_step; };
    const auto up = [](float value) { return std::ceil(value / window_step) * window_step; };
    return ScreenWindow{
        .min =
            {
                .x = std::max(down(seen.min.x), window.min.x),
                .y = std::max(down(seen.min.y), window.min.y),
            },
        .max =
            {
                .x = std::min(up(seen.max.x), window.max.x),
                .y = std::min(up(seen.max.y), window.max.y),
            },
    };
}

// Grows a sector's window, if it has one, to take in another, or gives it that one, and returns
// what it changed to, or nothing if it didn't change.
std::optional<ScreenWindow> take_in(std::optional<ScreenWindow>& window, const ScreenWindow& seen) {
    if (!window) {
        window = seen;
        return seen;
    }
    const ScreenWindow grown{
        .min =
            {
                .x = std::min(window->min.x, seen.min.x),
                .y = std::min(window->min.y, seen.min.y),
            },
        .max =
            {
                .x = std::max(window->max.x, seen.max.x),
                .y = std::max(window->max.y, seen.max.y),
            },
    };
    if (grown == *window) {
        return std::nullopt;
    }
    window = grown;
    return grown;
}

// Where a view starts: the sector it starts in, seen within a window, from a camera in that
// sector's coordinates.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): it has no default, as its sector has none
struct ViewStart {
    SectorId sector;
    ScreenWindow window;
    Camera camera;

    // The wall of the sector that the view entered it through, which leads back into the view it is
    // seen from, or nothing for the main view.
    std::optional<WallId> entrance{};

    // The view it is seen from, with its depth, the plane it is clipped at, and the opening it is
    // seen through, as the view's record has them.
    std::optional<std::size_t> parent{};
    std::size_t depth = 0;
    std::optional<Plane> clip{};
    std::array<Vector3, 6> opening{};
    bool opening_at_eye = false;
};

// An opening seen in a view that starts a view nested in it: the wall it is in, the wall on the
// portal's other side and the sector beyond, the transform into it, and the window it is seen in.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): it has no default, as its IDs have none
struct NestedStart {
    WallId wall;
    WallId entrance;
    SectorId sector;
    RigidTransform transform;
    std::vector<Vector3> opening;
    ScreenWindow window;
};

// The plane of a wall that is a side of a portal, facing the sector beyond it, moved a millimetre
// the other way so that rounding never clips what the sector beyond holds on the plane itself.
Plane plane_beyond(const Wall& wall) {
    constexpr float margin = 0.001F;
    const auto along = normalize(wall.end - wall.start);
    const Vector3 normal{.x = along.y, .y = -along.x};
    return {
        .normal = normal,
        .distance = dot(normal, Vector3{.x = wall.start.x, .y = wall.start.y}) - margin,
    };
}

// Whether a wall's plane lies so near the eye that the near plane would take away some of what the
// screen shows of it: a point on the screen lies less than `spread` times as far from the eye as
// it does ahead of it.
bool at_eye(const Wall& wall, const Camera& camera, const Projection& projection) {
    const auto along = wall.end - wall.start;
    const Vector2 eye{.x = camera.location.point.x, .y = camera.location.point.y};
    const auto distance = std::abs(perp_dot(along, eye - wall.start)) / length(along);
    return distance < near_plane * spread_of(projection);
}

// The corners of an opening, a convex polygon of at most six corners bounded by the two floors,
// the two ceilings and the wall's ends, with its last corner repeated after them.
std::array<Vector3, 6> corners_of(const std::vector<Vector3>& opening) {
    WULFHILDE_DEBUG_ASSERT(!opening.empty() && opening.size() <= 6,
                           "an opening has from one to six corners");
    std::array<Vector3, 6> corners{};
    std::ranges::fill(corners, opening.back());
    std::ranges::copy(opening | std::views::take(corners.size()), corners.begin());
    return corners;
}

// What a walk through one view finds: the window each sector is seen in, by its ID's index, each
// sector whose window has grown since the walk last went on from it, with the window it grew to,
// and each opening that starts a view nested in it, in the order the walk first sees them.
struct ViewWalk {
    std::vector<std::optional<ScreenWindow>> windows{};
    std::vector<std::pair<SectorId, ScreenWindow>> grown{};
    std::vector<NestedStart> starts{};
};

// Notes an opening that starts a nested view, or grows the window of one noted already to take in
// the window it is seen in now.
void note_start(std::vector<NestedStart>& starts, const NestedStart& start) {
    const auto found = std::ranges::find(starts, start.wall, &NestedStart::wall);
    if (found == starts.end()) {
        starts.push_back(start);
        return;
    }
    std::optional<ScreenWindow> window = found->window;
    take_in(window, start.window);
    found->window = window.value_or(start.window);
}

// Goes on through the openings of a sector seen in a view within a window: into the sector beyond
// each, or to the view nested there if the portal is transformed or the sector beyond shares space
// with another.
void walk_through(const Map& map, const Projection& projection, const ViewStart& start,
                  SectorId sector, const ScreenWindow& window, ViewWalk& walk) {
    const auto& camera = start.camera;
    for (const auto& loop : map.sector(sector).loops) {
        for (const auto wall_id : loop) {
            const auto& wall = map.wall(wall_id);
            const auto opening = map.face(wall_id).opening;
            if (!wall.link || opening.empty() || wall_id == start.entrance) {
                continue;
            }
            const auto seen = window_through(wall, opening, window, camera, projection);
            if (!seen) {
                continue;
            }
            const auto beyond = map.wall(wall.link->wall).sector;
            if (wall.link->transform != RigidTransform::identity() ||
                !map.overlapping(beyond).empty()) {
                note_start(walk.starts, {
                                            .wall = wall_id,
                                            .entrance = wall.link->wall,
                                            .sector = beyond,
                                            .transform = wall.link->transform,
                                            .opening = opening,
                                            .window = *seen,
                                        });
            } else if (const auto taken = take_in(walk.windows.at(beyond.index()), *seen)) {
                walk.grown.emplace_back(beyond, *taken);
            }
        }
    }
}

// Walks through one view from where it starts.
ViewWalk walk_view(const Map& map, const Projection& projection, const ViewStart& start) {
    ViewWalk walk{
        .windows = std::vector<std::optional<ScreenWindow>>(map.sectors().size()),
        .grown = {{start.sector, start.window}},
        .starts = {},
    };
    walk.windows.at(start.sector.index()) = start.window;
    while (!walk.grown.empty()) {
        const auto [sector, window] = walk.grown.back();
        walk.grown.pop_back();
        // A window that has grown again since is taken up from its later place on the list.
        if (walk.windows.at(sector.index()) == window) {
            walk_through(map, projection, start, sector, window, walk);
        }
    }
    return walk;
}

// A camera carried through a portal into the sector beyond it.
Camera carried(const Camera& camera, SectorId beyond, const RigidTransform& transform) {
    return {
        .location = {.sector = beyond, .point = transform_point(transform, camera.location.point)},
        .orientation = transform.rotation * camera.orientation,
        .vertical_field_of_view = camera.vertical_field_of_view,
    };
}

} // namespace

PortalTraversal traverse_portals(const Map& map, const Camera& camera, Extent extent,
                                 const ViewLimits& limits) {
    WULFHILDE_DEBUG_ASSERT(limits.views >= 1, "a frame has its main view");
    PortalTraversal traversal;
    // An opening that would start a view past the limits is filled flat in the view it is seen in.
    const auto fill = [&traversal](const ViewStart& start) {
        traversal.fills.push_back({
            .view = start.parent.value_or(0),
            .sector = start.sector,
            .opening = start.opening,
            .opening_at_eye = start.opening_at_eye,
        });
    };
    // The views still to walk through, the next last, so that each view's nested views follow it,
    // each with the views nested in it, before the next view nested where it is.
    constexpr ScreenWindow whole_screen{};
    const ViewStart main_view{
        .sector = camera.location.sector,
        .window = whole_screen,
        .camera = camera,
    };
    std::vector<ViewStart> starts{main_view};
    while (!starts.empty() && traversal.views.size() < limits.views) {
        const auto start = starts.back();
        starts.pop_back();
        const auto projection = projection_of(start.camera, extent);
        const auto view = traversal.views.size();
        traversal.views.push_back({
            .view_projection = projection.view_projection,
            .eye = start.camera.location.point,
            .parent = start.parent,
            .depth = start.depth,
            .clip = start.clip,
            .opening = start.opening,
            .opening_at_eye = start.opening_at_eye,
        });
        const auto walk = walk_view(map, projection, start);
        for (std::size_t index = 0; const auto& window : walk.windows) {
            if (window) {
                traversal.sectors.push_back({
                    .view = view,
                    .sector = SectorId(narrow<std::uint32_t>(index)),
                    .window = *window,
                });
            }
            ++index;
        }
        for (const auto& nested : walk.starts | std::views::reverse) {
            const auto& wall = map.wall(nested.wall);
            const ViewStart nested_start{
                .sector = nested.sector,
                .window = nested.window,
                .camera = carried(start.camera, nested.sector, nested.transform),
                .entrance = nested.entrance,
                .parent = view,
                .depth = start.depth + 1,
                .clip = transform(nested.transform, plane_beyond(wall)),
                .opening = corners_of(nested.opening),
                .opening_at_eye = at_eye(wall, start.camera, projection),
            };
            if (start.depth == limits.nested) {
                fill(nested_start);
            } else {
                starts.push_back(nested_start);
            }
        }
    }
    std::ranges::for_each(starts, fill);
    // Each view's fills are drawn once the view is, so they are kept in the order of the views.
    std::ranges::stable_sort(traversal.fills, {}, &FillRecord::view);
    return traversal;
}

} // namespace wulfhilde::detail
