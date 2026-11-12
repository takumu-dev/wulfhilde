#include "sector_geometry.hpp"

#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/wall.hpp>

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

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

// Twice the area of a triangle seen from above, positive when its corners run anticlockwise.
float twice_area(const Vector2& a, const Vector2& b, const Vector2& c) {
    return ((b.x - a.x) * (c.y - a.y)) - ((b.y - a.y) * (c.x - a.x));
}

void add_triangle(std::vector<SectorVertex>& vertices, const std::array<Vector3, 3>& corners,
                  MaterialId material, SectorId sector) {
    for (const auto& corner : corners) {
        vertices.push_back({
            .position = corner,
            .material = material.index(),
            .sector = sector.index(),
        });
    }
}

// A wall faces its sector, to its left as it runs from its start to its end, so seen from there
// its start is on the right.
void add_wall(std::vector<SectorVertex>& vertices, const Wall& wall, const Sector& sector) {
    const auto low = sector.floor.height;
    const auto high = sector.ceiling.height;
    const Vector3 start_low{.x = wall.start.x, .y = wall.start.y, .z = low};
    const Vector3 start_high{.x = wall.start.x, .y = wall.start.y, .z = high};
    const Vector3 end_low{.x = wall.end.x, .y = wall.end.y, .z = low};
    const Vector3 end_high{.x = wall.end.x, .y = wall.end.y, .z = high};
    add_triangle(vertices, {start_low, start_high, end_high}, wall.surface.material, wall.sector);
    add_triangle(vertices, {start_low, end_high, end_low}, wall.surface.material, wall.sector);
}

// The floor faces up and the ceiling down, so the floor's triangles run anticlockwise seen from
// above and the ceiling's clockwise. Each triangle is three indices into the sector's corners.
void add_floor_or_ceiling(std::vector<SectorVertex>& vertices, SectorId sector,
                          const std::vector<Vector2>& corners,
                          const std::vector<std::uint32_t>& triangles,
                          const FloorOrCeiling& surface, bool faces_up) {
    const auto lift = [&surface](const Vector2& point) {
        return Vector3{.x = point.x, .y = point.y, .z = surface.height};
    };
    for (std::size_t first = 0; first + 2 < triangles.size(); first += 3) {
        auto a = corners.at(triangles.at(first));
        auto b = corners.at(triangles.at(first + 1));
        const auto c = corners.at(triangles.at(first + 2));
        if ((twice_area(a, b, c) > 0.0F) != faces_up) {
            std::swap(a, b);
        }
        add_triangle(vertices, {lift(a), lift(b), lift(c)}, surface.surface.material, sector);
    }
}

} // namespace

SectorGeometry triangulate(const Map& map) {
    SectorGeometry geometry;
    for (std::size_t index = 0; const auto& sector : map.sectors()) {
        const SectorId id(narrow<std::uint32_t>(index));
        const auto first = geometry.vertices.size();

        // earcut takes the outer loop and then any inner loops, and numbers their corners in that
        // order.
        std::vector<std::vector<std::array<float, 2>>> loops;
        std::vector<Vector2> corners;
        for (const auto& loop : sector.loops) {
            auto& outline = loops.emplace_back();
            for (const auto wall_id : loop) {
                const auto& wall = map.wall(wall_id);
                add_wall(geometry.vertices, wall, sector);
                outline.push_back({wall.start.x, wall.start.y});
                corners.push_back(wall.start);
            }
        }
        const auto flat_triangles = mapbox::earcut<std::uint32_t>(loops);
        add_floor_or_ceiling(geometry.vertices, id, corners, flat_triangles, sector.floor, true);
        add_floor_or_ceiling(geometry.vertices, id, corners, flat_triangles, sector.ceiling, false);

        geometry.sectors.push_back({.first = first, .count = geometry.vertices.size() - first});
        ++index;
    }
    return geometry;
}

} // namespace wulfhilde::detail
