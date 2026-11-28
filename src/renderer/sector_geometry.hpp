// A map's sectors as triangles, worked out once when the map is set, each surface's triangles in a
// range of vertices of its own, and each sector's surfaces one after another.
#pragma once

#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sky.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace wulfhilde::detail {

/// The C++ twin of the sector shaders' `SectorVertex`: one corner of a triangle of a sector's
/// surface or sky, in the sector's coordinates, with where the material's textures are sampled
/// there, what shades it and the sector. What shades it is the index of the surface's material, or
/// of the sky among the geometry's skies after the map's materials.
///
/// The texture coordinates count repeats of the material from the top-left corner of its images,
/// as Vulkan samples them, so x runs along u and y against v.
struct SectorVertex {
    Vector3 position;
    Vector2 texture_coordinates;
    std::uint32_t material = 0;
    std::uint32_t sector = 0;
};

/// A run of items in a list: the index of the first, and how many there are.
struct Range {
    std::size_t first = 0;
    std::size_t count = 0;
};

/// The triangles of one surface: their vertices among those of the whole map, and the box that
/// holds them, along the axes of their sector's coordinates, from its lowest corner to its highest.
struct SurfaceTriangles {
    Range vertices;
    Vector3 lowest;
    Vector3 highest;
};

/// A map's sectors as lists of triangles, surface by surface: each wall's strips or the whole wall,
/// then the floor, then the ceiling. Each sector's surfaces are those in the range at its ID's
/// index. Each distinct sky that they show is listed in the order they are first met.
struct SectorGeometry {
    std::vector<SectorVertex> vertices;
    std::vector<SurfaceTriangles> surfaces;
    std::vector<Range> sectors;
    std::vector<Sky> skies;
};

/// Triangulates every sector of a map: each solid wall as a fan of triangles from the floor to the
/// ceiling, each side of a portal as a fan for each of its strips, below and above its opening,
/// and the floor and the ceiling across the sector's outer loop, concave or not, with holes where
/// its inner loops are, whether a surface or a sky, each corner lifted onto the floor's or the
/// ceiling's plane. Every triangle's corners run
/// anticlockwise seen from the side its surface faces: into the sector.
///
/// Each surface's material lies on it as the map says, a strip's as its wall's. On a wall, u runs
/// from the wall's start to the right, as the wall is seen from its sector, and v is height. On a
/// flat floor or ceiling, u and v are X and Y, and on a sloped one they run from its hinge wall's
/// start along the hinge and square to it, into the sector, measured along the slope. The
/// material is then moved by the surface's offset,
/// along u and v in metres, turned about the point it is moved to, from +u toward +v, by the
/// surface's rotation, and repeats at its size times the surface's scale, along its own turned
/// axes.
///
/// \param map The map.
/// \param materials The map's materials, each at the index of its ID.
[[nodiscard]] SectorGeometry triangulate(const Map& map, std::span<const Material> materials);

} // namespace wulfhilde::detail
