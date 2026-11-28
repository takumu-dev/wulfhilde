#include "portal_traversal.hpp"

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/narrow.hpp>
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
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
    // The wall faces its sector, to its left, which is where the eye is.
    const auto along = wall.end - wall.start;
    const Vector2 eye{.x = camera.location.point.x, .y = camera.location.point.y};
    if (perp_dot(along, eye - wall.start) < near_plane * length(along)) {
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
    const auto spread = std::sqrt(1.0F + (projection.half_width * projection.half_width) +
                                  (projection.half_height * projection.half_height));
    const auto least_depth = 0.5F * near_plane / spread;
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

} // namespace

PortalTraversal traverse_portals(const Map& map, const Camera& camera, Extent extent) {
    const auto projection = projection_of(camera, extent);
    // The window each sector is seen in so far, by its ID's index, and each sector whose window has
    // grown since the walk last went on from it, with the window it grew to.
    std::vector<std::optional<ScreenWindow>> windows(map.sectors().size());
    std::vector<std::pair<SectorId, ScreenWindow>> grown{{camera.location.sector, ScreenWindow{}}};
    windows.at(camera.location.sector.index()) = ScreenWindow{};
    while (!grown.empty()) {
        const auto [sector_id, window] = grown.back();
        grown.pop_back();
        // A window that has grown again since is taken up from its later place on the list.
        if (windows.at(sector_id.index()) != window) {
            continue;
        }
        for (const auto& loop : map.sector(sector_id).loops) {
            for (const auto wall_id : loop) {
                const auto& wall = map.wall(wall_id);
                if (!wall.link) {
                    continue;
                }
                const auto opening = map.face(wall_id).opening;
                if (opening.empty()) {
                    continue;
                }
                const auto seen = window_through(wall, opening, window, camera, projection);
                if (!seen) {
                    continue;
                }
                const auto beyond = map.wall(wall.link->wall).sector;
                if (const auto taken = take_in(windows.at(beyond.index()), *seen)) {
                    grown.emplace_back(beyond, *taken);
                }
            }
        }
    }

    PortalTraversal traversal;
    traversal.views.push_back({
        .view_projection = projection.view_projection,
        .eye = camera.location.point,
    });
    for (std::size_t index = 0; const auto& window : windows) {
        if (window) {
            traversal.sectors.push_back({
                .view = 0,
                .sector = SectorId(narrow<std::uint32_t>(index)),
                .window = *window,
            });
        }
        ++index;
    }
    return traversal;
}

} // namespace wulfhilde::detail
