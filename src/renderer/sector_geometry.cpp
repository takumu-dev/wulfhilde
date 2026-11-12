#include "sector_geometry.hpp"

#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
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

class SectorTriangles {
  public:
    SectorTriangles(std::vector<SectorVertex>& vertices, std::uint32_t sector)
        : vertices_(&vertices), sector_(sector) {}

    void add(const std::array<Vector3, 3>& corners, std::uint32_t material) {
        for (const auto& corner : corners) {
            vertices_->push_back({.position = corner, .material = material, .sector = sector_});
        }
    }

    // A wall faces its sector, to its left as it runs from its start to its end, so seen from
    // there its start is on the right.
    void add_wall(const Wall& wall, float floor, float ceiling) {
        const auto material = wall.surface.material.index();
        const Vector3 start_low{.x = wall.start.x, .y = wall.start.y, .z = floor};
        const Vector3 start_high{.x = wall.start.x, .y = wall.start.y, .z = ceiling};
        const Vector3 end_low{.x = wall.end.x, .y = wall.end.y, .z = floor};
        const Vector3 end_high{.x = wall.end.x, .y = wall.end.y, .z = ceiling};
        add({start_low, start_high, end_high}, material);
        add({start_low, end_high, end_low}, material);
    }

    // The floor faces up and the ceiling down, so the floor's triangles run anticlockwise seen
    // from above and the ceiling's clockwise.
    void add_flat(const std::vector<Vector2>& points, const std::vector<std::uint32_t>& triangles,
                  const FloorOrCeiling& surface, bool faces_up) {
        for (std::size_t first = 0; first + 2 < triangles.size(); first += 3) {
            auto a = points.at(triangles.at(first));
            auto b = points.at(triangles.at(first + 1));
            const auto c = points.at(triangles.at(first + 2));
            if ((twice_area(a, b, c) > 0.0F) != faces_up) {
                std::swap(a, b);
            }
            const auto lift = [&surface](const Vector2& point) {
                return Vector3{.x = point.x, .y = point.y, .z = surface.height};
            };
            add({lift(a), lift(b), lift(c)}, surface.surface.material.index());
        }
    }

  private:
    std::vector<SectorVertex>* vertices_;
    std::uint32_t sector_;
};

} // namespace

SectorGeometry triangulate(const Map& map) {
    SectorGeometry geometry;
    for (std::size_t index = 0; const auto& sector : map.sectors()) {
        const auto first = geometry.vertices.size();
        SectorTriangles triangles(geometry.vertices, narrow<std::uint32_t>(index));

        // earcut takes the outer loop and then any inner loops, and numbers their corners in that
        // order.
        std::vector<std::vector<std::array<float, 2>>> loops;
        std::vector<Vector2> corners;
        for (const auto& loop : sector.loops) {
            auto& outline = loops.emplace_back();
            for (const auto id : loop) {
                const auto& wall = map.wall(id);
                triangles.add_wall(wall, sector.floor.height, sector.ceiling.height);
                outline.push_back({wall.start.x, wall.start.y});
                corners.push_back(wall.start);
            }
        }
        const auto flat = mapbox::earcut<std::uint32_t>(loops);
        triangles.add_flat(corners, flat, sector.floor, true);
        triangles.add_flat(corners, flat, sector.ceiling, false);

        geometry.sectors.push_back({.first = first, .count = geometry.vertices.size() - first});
        ++index;
    }
    return geometry;
}

} // namespace wulfhilde::detail
