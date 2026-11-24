// A map's sectors as triangles, worked out once when the map is set, each sector's triangles in a
// range of vertices of its own.
#pragma once

#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace wulfhilde::detail {

/// The C++ twin of the sector shaders' `SectorVertex`: one corner of a triangle of a sector's
/// surface, in the sector's coordinates, with where the material's textures are sampled there, the
/// material that covers the surface and the sector.
///
/// The texture coordinates count repeats of the material from the top-left corner of its images,
/// as Vulkan samples them, so x runs along u and y against v.
struct SectorVertex {
    Vector3 position;
    Vector2 texture_coordinates;
    std::uint32_t material = 0;
    std::uint32_t sector = 0;
};

/// The vertices of one sector's triangles, among those of the whole map.
struct VertexRange {
    std::size_t first = 0;
    std::size_t count = 0;
};

/// A map's sectors as lists of triangles, each sector's in the range at its ID's index.
struct SectorGeometry {
    std::vector<SectorVertex> vertices;
    std::vector<VertexRange> sectors;
};

/// Triangulates every sector of a map: each wall as two triangles from the floor to the ceiling,
/// and the floor and the ceiling across the sector's loops, concave or not. Every triangle's
/// corners run anticlockwise seen from the side its surface faces: into the sector.
///
/// Each surface's material lies on it as the map says. On a wall, u runs from the wall's start to
/// the right, as the wall is seen from its sector, and v is height. On a floor or a ceiling, u and
/// v are X and Y. The material is then moved by the surface's offset, along u and v in metres,
/// turned about the point it is moved to, from +u toward +v, by the surface's rotation, and
/// repeats at its size times the surface's scale, along its own turned axes.
///
/// \param map The map.
/// \param materials The map's materials, each at the index of its ID.
[[nodiscard]] SectorGeometry triangulate(const Map& map, std::span<const Material> materials);

} // namespace wulfhilde::detail
