/// \file
/// What a surface of a map carries.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ids.hpp>

namespace wulfhilde {

/// What a surface carries, as a map holds it: the material that covers it, and how the material
/// lies on it.
///
/// It mirrors `SurfaceDescription`, with the material's asset path interned as an ID, which
/// `Map::materials` turns back into the path.
struct Surface {
    /// The material that covers the surface.
    MaterialId material;

    /// How far the material is moved along u and v, in metres.
    Vector2 offset{};

    /// What the material's size is multiplied by, along u and v.
    Vector2 scale{.x = 1.0F, .y = 1.0F};

    /// How far the material is turned in the surface's plane.
    Angle rotation{};

    /// Compares two surfaces exactly.
    constexpr bool operator==(const Surface&) const noexcept = default;
};

} // namespace wulfhilde
