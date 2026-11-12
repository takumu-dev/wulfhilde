// A map's sectors as triangles, worked out once when the map is set, each sector's triangles in a
// range of vertices of its own.
#pragma once

#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wulfhilde::detail {

/// The C++ twin of the sector shaders' `SectorVertex`: one corner of a triangle of a sector's
/// surface, in the sector's coordinates, with the material that covers the surface and the sector.
struct SectorVertex {
    Vector3 position;
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
[[nodiscard]] SectorGeometry triangulate(const Map& map);

} // namespace wulfhilde::detail
