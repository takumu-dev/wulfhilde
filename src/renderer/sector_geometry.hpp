// A map's sectors as triangles, worked out once when the map is set, each surface's triangles in a
// range of vertices of its own, and each sector's surfaces one after another.
#pragma once

#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/sky.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace wulfhilde::detail {

/// The C++ twin of the sector shaders' `SectorVertex`: one corner of a triangle of a sector's
/// surface or sky, in the sector's coordinates, with where the material's textures are sampled
/// there, the surface's normal and tangent, what shades it and the sector. What shades it is the
/// index of the surface's material, or of one of the geometry's other shadings after the map's
/// materials.
///
/// The texture coordinates count repeats of the material from the top-left corner of its images,
/// as Vulkan samples them, so x runs along u and y against v. The normal points out of the surface,
/// into its sector. The tangent's x, y and z are the way u runs on the surface, and its w is 1
/// where v runs the way of the normal's cross product with it, and -1 where it runs the other way,
/// as in glTF, so that a normal texture tilts the normal as the material lies.
struct SectorVertex {
    Vector3 position;
    Vector2 texture_coordinates;
    Vector3 normal;
    Vector4 tangent;
    std::uint32_t material = 0;
    std::uint32_t sector = 0;
};

/// A material shining at a surface's own nits, in place of its emission's.
struct MaterialAtNits {
    MaterialId material;
    float nits = 0.0F;

    bool operator==(const MaterialAtNits&) const = default;
};

/// What shades a surface besides a map's materials as they are: a sky, or a material at a surface's
/// own nits.
using Shading = std::variant<Sky, MaterialAtNits>;

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
/// index. Each distinct sky that they show, and each distinct material at a surface's own nits, is
/// a shading, listed in the order they are first met.
struct SectorGeometry {
    std::vector<SectorVertex> vertices;
    std::vector<SurfaceTriangles> surfaces;
    std::vector<Range> sectors;
    std::vector<Shading> shadings;
};

/// Triangulates every sector of a map: each solid wall as two triangles from the floor to the
/// ceiling, each side of a portal as two triangles for each of its strips, below and above its
/// opening, and the floor and the ceiling across the sector's outer loop, concave or not, with
/// holes where its inner loops are, whether a surface or a sky. Every triangle's corners run
/// anticlockwise seen from the side its surface faces: into the sector. A surface with nits of its
/// own is shaded by its material at those nits.
///
/// Each surface's material lies on it as the map says, a strip's as its wall's. On a wall, u runs
/// from the wall's start to the right, as the wall is seen from its sector, and v is height. On a
/// floor or a ceiling, u and v are X and Y. The material is then moved by the surface's offset,
/// along u and v in metres, turned about the point it is moved to, from +u toward +v, by the
/// surface's rotation, and repeats at its size times the surface's scale, along its own turned
/// axes.
///
/// \param map The map.
/// \param materials The map's materials, each at the index of its ID.
[[nodiscard]] SectorGeometry triangulate(const Map& map, std::span<const Material> materials);

} // namespace wulfhilde::detail
