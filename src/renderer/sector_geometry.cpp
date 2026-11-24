#include "sector_geometry.hpp"

#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/surface.hpp>
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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
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

void add_triangle(std::vector<SectorVertex>& vertices, const std::array<Vector3, 3>& corners,
                  const std::array<Vector2, 3>& texture_coordinates, MaterialId material,
                  SectorId sector) {
    for (std::size_t index = 0; const auto& corner : corners) {
        vertices.push_back({
            .position = corner,
            .texture_coordinates = texture_coordinates.at(index),
            .material = material.index(),
            .sector = sector.index(),
        });
        ++index;
    }
}

// A wall faces its sector, to its left as it runs from its start to its end, so seen from there
// its start is on the right, and u, which runs to the right from the start, is negative across it.
void add_wall(std::vector<SectorVertex>& vertices, const Wall& wall, const Sector& sector,
              std::span<const Material> materials) {
    const auto low = sector.floor.height;
    const auto high = sector.ceiling.height;
    const Vector3 start_low{.x = wall.start.x, .y = wall.start.y, .z = low};
    const Vector3 start_high{.x = wall.start.x, .y = wall.start.y, .z = high};
    const Vector3 end_low{.x = wall.end.x, .y = wall.end.y, .z = low};
    const Vector3 end_high{.x = wall.end.x, .y = wall.end.y, .z = high};

    const MaterialPlacement placement(wall.surface, materials, {.x = 0.0F, .y = low});
    const auto end_u = -distance(wall.start, wall.end);
    const auto at_start_low = placement({.x = 0.0F, .y = low});
    const auto at_start_high = placement({.x = 0.0F, .y = high});
    const auto at_end_low = placement({.x = end_u, .y = low});
    const auto at_end_high = placement({.x = end_u, .y = high});

    add_triangle(vertices, {start_low, start_high, end_high},
                 {at_start_low, at_start_high, at_end_high}, wall.surface.material, wall.sector);
    add_triangle(vertices, {start_low, end_high, end_low}, {at_start_low, at_end_high, at_end_low},
                 wall.surface.material, wall.sector);
}

// The floor faces up and the ceiling down, so the floor's triangles run anticlockwise seen from
// above and the ceiling's clockwise. Each triangle is three indices into the sector's corners.
void add_floor_or_ceiling(std::vector<SectorVertex>& vertices, SectorId sector,
                          const std::vector<Vector2>& corners,
                          const std::vector<std::uint32_t>& triangles,
                          const FloorOrCeiling& surface, bool faces_up,
                          std::span<const Material> materials) {
    const MaterialPlacement placement(surface.surface, materials, corners.front());
    std::vector<Vector2> coordinates;
    coordinates.reserve(corners.size());
    for (const auto& corner : corners) {
        coordinates.push_back(placement(corner));
    }

    const auto lift = [&surface](const Vector2& point) {
        return Vector3{.x = point.x, .y = point.y, .z = surface.height};
    };
    for (std::size_t first = 0; first + 2 < triangles.size(); first += 3) {
        auto a = triangles.at(first);
        auto b = triangles.at(first + 1);
        const auto c = triangles.at(first + 2);
        if ((twice_area(corners.at(a), corners.at(b), corners.at(c)) > 0.0F) != faces_up) {
            std::swap(a, b);
        }
        add_triangle(vertices, {lift(corners.at(a)), lift(corners.at(b)), lift(corners.at(c))},
                     {coordinates.at(a), coordinates.at(b), coordinates.at(c)},
                     surface.surface.material, sector);
    }
}

} // namespace

SectorGeometry triangulate(const Map& map, std::span<const Material> materials) {
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
                add_wall(geometry.vertices, wall, sector, materials);
                outline.push_back({wall.start.x, wall.start.y});
                corners.push_back(wall.start);
            }
        }
        const auto flat_triangles = mapbox::earcut<std::uint32_t>(loops);
        add_floor_or_ceiling(geometry.vertices, id, corners, flat_triangles, sector.floor, true,
                             materials);
        add_floor_or_ceiling(geometry.vertices, id, corners, flat_triangles, sector.ceiling, false,
                             materials);

        geometry.sectors.push_back({.first = first, .count = geometry.vertices.size() - first});
        ++index;
    }
    return geometry;
}

} // namespace wulfhilde::detail
