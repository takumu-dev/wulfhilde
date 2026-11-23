/// \file
/// What a body stands on.

#pragma once

#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/surface_ref.hpp>

namespace wulfhilde {

/// What a body stands on: a surface it touches that is no steeper, measured from its gravity up,
/// than the steepest slope it can stand on.
struct Ground {
    /// The surface the body stands on.
    SurfaceRef surface;

    /// The direction that pushes the body off the surface where they touch, of unit length, in the
    /// coordinates of the sector the body's centre is in.
    Vector3 normal{};

    /// Compares two grounds exactly.
    constexpr bool operator==(const Ground&) const = default;
};

} // namespace wulfhilde
