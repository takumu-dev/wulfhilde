/// \file
/// Where a body is, how it is tilted and which way its gravity pulls.

#pragma once

#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/location.hpp>

namespace wulfhilde {

/// Where a body is, how it is tilted and which way its gravity pulls, all in the coordinates of the
/// sector its centre is in.
///
/// A body is upright when its tilt's up is its gravity up. Righting turns it toward that, and is
/// left to the game, which asks `settle` whether each step fits.
struct Pose {
    /// Where the body's centre is: the midpoint of its capsule.
    Location centre;

    /// How the body is turned from its sector's upright, which stands along +Z. The capsule stands
    /// along the tilt's up.
    Rotation tilt = Rotation::identity();

    /// The direction the body's gravity pulls it away from, of unit length. It is +Z, the sector's
    /// up, until a portal changes it.
    Vector3 gravity_up{.x = 0.0F, .y = 0.0F, .z = 1.0F};
};

} // namespace wulfhilde
