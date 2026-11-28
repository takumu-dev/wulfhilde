#include "sector_geometry.hpp"

#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>
#include <wulfhilde/world/surface.hpp>
#include <wulfhilde/world/wall.hpp>
#include <wulfhilde/world/wall_face.hpp>

// GCC's optimiser takes earcut's sorting of a list that it never calls empty for one that might be,
// and warns of a null dereference there, in a header no other warning reaches.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
#include <mapbox/earcut.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace wulfhilde::detail {

namespace {

// Twice the area of a triangle seen from above, positive when its corners run anticlockwise.
float twice_area(const Vector2& a, const Vector2& b, const Vector2& c) {
    return ((b.x - a.x) * (c.y - a.y)) - ((b.y - a.y) * (c.x - a.x));
}

// How a surface's material lies on it: from a point's u and v on the surface, in metres, to where
// the material's textures are sampled there. Textures repeat, so whole repeats are taken away to
// leave the coordinates near a point of the surface within one repeat of zero. They are worked
// out in double, so that a surface far from the origin keeps every bit of their precision.
class MaterialPlacement {
  public:
    MaterialPlacement(const Surface& surface, std::span<const Material> materials, Vector2 near)
        : offset_x_(static_cast<double>(surface.offset.x)),
          offset_y_(static_cast<double>(surface.offset.y)),
          cosine_(std::cos(static_cast<double>(surface.rotation.in_radians()))),
          sine_(std::sin(static_cast<double>(surface.rotation.in_radians()))) {
        WULFHILDE_DEBUG_ASSERT(surface.material.index() < materials.size(),
                               "the map's assets hold the surface's material");
        // A material without a texture has no size, and is the same wherever it is sampled.
        const auto size = materials.subspan(surface.material.index(), 1)
                              .front()
                              .size.value_or(Vector2{.x = 1.0F, .y = 1.0F});
        repeat_x_ = static_cast<double>(size.x) * static_cast<double>(surface.scale.x);
        repeat_y_ = static_cast<double>(size.y) * static_cast<double>(surface.scale.y);
        const auto [x, y] = exact(near);
        whole_x_ = std::floor(x);
        whole_y_ = std::floor(y);
    }

    [[nodiscard]] Vector2 operator()(Vector2 point) const {
        const auto [x, y] = exact(point);
        return {.x = static_cast<float>(x - whole_x_), .y = static_cast<float>(y - whole_y_)};
    }

  private:
    [[nodiscard]] std::array<double, 2> exact(Vector2 point) const {
        const auto moved_x = static_cast<double>(point.x) - offset_x_;
        const auto moved_y = static_cast<double>(point.y) - offset_y_;
        // Turning the material one way turns the point the other way in the material's axes.
        const auto turned_x = (cosine_ * moved_x) + (sine_ * moved_y);
        const auto turned_y = (cosine_ * moved_y) - (sine_ * moved_x);
        // An image's rows run down from its top, and v runs up from its bottom.
        return {turned_x / repeat_x_, -turned_y / repeat_y_};
    }

    double offset_x_;
    double offset_y_;
    double cosine_;
    double sine_;
    double repeat_x_ = 1.0;
    double repeat_y_ = 1.0;
    double whole_x_ = 0.0;
    double whole_y_ = 0.0;
};

// Takes the vertices added since the first given as the triangles of one surface, unless there are
// none.
void end_surface(SectorGeometry& geometry, std::size_t first) {
    const auto added = std::span(geometry.vertices).subspan(first);
    if (added.empty()) {
        return;
    }
    SurfaceTriangles surface{
        .vertices = {.first = first, .count = added.size()},
        .lowest = added.front().position,
        .highest = added.front().position,
    };
    for (const auto& vertex : added) {
        surface.lowest = {
            .x = std::min(surface.lowest.x, vertex.position.x),
            .y = std::min(surface.lowest.y, vertex.position.y),
            .z = std::min(surface.lowest.z, vertex.position.z),
        };
        surface.highest = {
            .x = std::max(surface.highest.x, vertex.position.x),
            .y = std::max(surface.highest.y, vertex.position.y),
            .z = std::max(surface.highest.z, vertex.position.z),
        };
    }
    geometry.surfaces.push_back(surface);
}

// Each corner of a triangle carries the index of what shades it: its surface's material, or its
// sky, numbered after the map's materials.
void add_triangle(std::vector<SectorVertex>& vertices, const std::array<Vector3, 3>& corners,
                  const std::array<Vector2, 3>& texture_coordinates, std::uint32_t shading,
                  SectorId sector) {
    for (std::size_t index = 0; const auto& corner : corners) {
        vertices.push_back({
            .position = corner,
            .texture_coordinates = texture_coordinates.at(index),
            .material = shading,
            .sector = sector.index(),
        });
        ++index;
    }
}

// A wall faces its sector, to its left as it runs from its start to its end, so seen from there
// its start is on the right, and u, which runs to the right from the start, is negative across it.
// Each part of a wall is a convex polygon whose corners run anticlockwise seen from the sector, so
// a fan of triangles from its first corner covers it.
void add_wall_part(SectorGeometry& geometry, const Wall& wall, const Surface& surface,
                   const std::vector<Vector3>& corners, std::span<const Material> materials) {
    if (corners.empty()) {
        return;
    }
    const auto along = normalize(wall.end - wall.start);
    const auto u_v = [&](const Vector3& corner) {
        return Vector2{
            .x = -dot(Vector2{.x = corner.x, .y = corner.y} - wall.start, along),
            .y = corner.z,
        };
    };
    const MaterialPlacement placement(surface, materials, u_v(corners.front()));

    const auto first = geometry.vertices.size();
    for (std::size_t next = 1; next + 1 < corners.size(); ++next) {
        const std::array triangle{corners.front(), corners.at(next), corners.at(next + 1)};
        add_triangle(geometry.vertices, triangle,
                     {
                         placement(u_v(triangle.at(0))),
                         placement(u_v(triangle.at(1))),
                         placement(u_v(triangle.at(2))),
                     },
                     surface.material.index(), wall.sector);
    }
    end_surface(geometry, first);
}

// A solid wall stands from the floor to the ceiling. A side of a portal is open between, and only
// its strips below and above the opening are drawn.
void add_wall(SectorGeometry& geometry, const Map& map, WallId id,
              std::span<const Material> materials) {
    const auto& wall = map.wall(id);
    const auto face = map.face(id);
    if (!wall.link) {
        add_wall_part(geometry, wall, wall.surface, face.whole, materials);
        return;
    }
    add_wall_part(geometry, wall, wall.lower, face.lower, materials);
    add_wall_part(geometry, wall, wall.upper, face.upper, materials);
}

// Returns where a point of a floor or ceiling lies in its surface's u and v, before its material
// is placed: along X and Y on a flat one, and on a sloped one from its hinge wall's start, along
// the hinge and square to it, into the sector, measured along the slope.
Vector2 floor_or_ceiling_u_v(const Map& map, const FloorOrCeiling& floor_or_ceiling,
                             const Vector2& point) {
    if (!floor_or_ceiling.slope) {
        return point;
    }
    const auto& hinge = map.wall(floor_or_ceiling.slope->hinge);
    const auto run_x = static_cast<double>(hinge.end.x) - static_cast<double>(hinge.start.x);
    const auto run_y = static_cast<double>(hinge.end.y) - static_cast<double>(hinge.start.y);
    const auto length = std::hypot(run_x, run_y);
    const auto x = static_cast<double>(point.x) - static_cast<double>(hinge.start.x);
    const auto y = static_cast<double>(point.y) - static_cast<double>(hinge.start.y);
    const auto gradient = static_cast<double>(floor_or_ceiling.slope->gradient);
    const auto stretch = std::sqrt(1.0 + (gradient * gradient));
    return {
        .x = static_cast<float>(((x * run_x) + (y * run_y)) / length),
        .y = static_cast<float>(((y * run_x) - (x * run_y)) / length * stretch),
    };
}

// Returns the index of what shades a sky: the map's materials come first, then each distinct sky,
// which this adds to the geometry's skies the first time it is met.
std::uint32_t sky_shading(SectorGeometry& geometry, const Sky& sky,
                          std::span<const Material> materials) {
    auto found = std::ranges::find(geometry.skies, sky);
    if (found == geometry.skies.end()) {
        geometry.skies.push_back(sky);
        found = std::prev(geometry.skies.end());
    }
    return narrow<std::uint32_t>(materials.size() +
                                 static_cast<std::size_t>(found - geometry.skies.begin()));
}

// The floor faces up and the ceiling down, so the floor's triangles run anticlockwise seen from
// above and the ceiling's clockwise. Each triangle is three indices into the sector's corners. A
// sky samples its backdrop by direction, not by texture coordinates, so it has none.
void add_floor_or_ceiling(SectorGeometry& geometry, const Map& map, SectorId sector,
                          const std::vector<Vector2>& corners,
                          const std::vector<std::uint32_t>& triangles, bool faces_up,
                          std::span<const Material> materials) {
    const auto& floor_or_ceiling = faces_up ? map.sector(sector).floor : map.sector(sector).ceiling;
    std::vector<Vector2> coordinates(corners.size());
    std::uint32_t shading = 0;
    if (const auto* const sky = std::get_if<Sky>(&floor_or_ceiling.cover)) {
        shading = sky_shading(geometry, *sky, materials);
    } else {
        const auto& surface = std::get<Surface>(floor_or_ceiling.cover);
        const auto u_v = [&](const Vector2& point) {
            return floor_or_ceiling_u_v(map, floor_or_ceiling, point);
        };
        const MaterialPlacement placement(surface, materials, u_v(corners.front()));
        std::ranges::transform(corners, coordinates.begin(),
                               [&](const Vector2& point) { return placement(u_v(point)); });
        shading = surface.material.index();
    }

    // A sloped floor or ceiling lies on its plane, so each corner is lifted to its height there.
    const auto lift = [&](const Vector2& point) {
        const Location at{.sector = sector, .point = {.x = point.x, .y = point.y}};
        return Vector3{
            .x = point.x,
            .y = point.y,
            .z = faces_up ? map.floor_height(at) : map.ceiling_height(at),
        };
    };
    const auto first_vertex = geometry.vertices.size();
    for (std::size_t first = 0; first + 2 < triangles.size(); first += 3) {
        auto a = triangles.at(first);
        auto b = triangles.at(first + 1);
        const auto c = triangles.at(first + 2);
        if ((twice_area(corners.at(a), corners.at(b), corners.at(c)) > 0.0F) != faces_up) {
            std::swap(a, b);
        }
        add_triangle(geometry.vertices,
                     {lift(corners.at(a)), lift(corners.at(b)), lift(corners.at(c))},
                     {coordinates.at(a), coordinates.at(b), coordinates.at(c)}, shading, sector);
    }
    end_surface(geometry, first_vertex);
}

} // namespace

SectorGeometry triangulate(const Map& map, std::span<const Material> materials) {
    SectorGeometry geometry;
    for (std::size_t index = 0; const auto& sector : map.sectors()) {
        const SectorId id(narrow<std::uint32_t>(index));
        const auto first = geometry.surfaces.size();

        // earcut takes the outer loop and then any inner loops, and numbers their corners in that
        // order.
        std::vector<std::vector<std::array<float, 2>>> loops;
        std::vector<Vector2> corners;
        for (const auto& loop : sector.loops) {
            auto& outline = loops.emplace_back();
            for (const auto wall_id : loop) {
                const auto& wall = map.wall(wall_id);
                add_wall(geometry, map, wall_id, materials);
                outline.push_back({wall.start.x, wall.start.y});
                corners.push_back(wall.start);
            }
        }
        const auto flat_triangles = mapbox::earcut<std::uint32_t>(loops);
        add_floor_or_ceiling(geometry, map, id, corners, flat_triangles, true, materials);
        add_floor_or_ceiling(geometry, map, id, corners, flat_triangles, false, materials);

        geometry.sectors.push_back({.first = first, .count = geometry.surfaces.size() - first});
        ++index;
    }
    return geometry;
}

} // namespace wulfhilde::detail
