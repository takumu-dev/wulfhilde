/// \file
/// The shape of a body, and the limits of what it can walk on.

#pragma once

#include <wulfhilde/core/angle.hpp>

namespace wulfhilde {

/// The shape of a body that collision moves: a capsule, with the limits of what it can step onto
/// and stand on.
///
/// A capsule is a cylinder capped by two half spheres. It stands along its tilt's up, centred on
/// its pose's centre, and its height runs from the bottom of one cap to the top of the other.
struct BodyShape {
    /// The radius of the capsule's cylinder and caps, in metres, at least 0.001.
    float radius = 0.0F;

    /// The capsule's height from end to end, in metres, at least twice its radius.
    float height = 0.0F;

    /// How high a step the body walks up without stopping, measured along its gravity up, in
    /// metres, zero or more.
    float step_height = 0.0F;

    /// The steepest surface the body stands on, measured from its gravity up, from 0 to 90
    /// degrees. A surface exactly this steep is ground.
    Angle steepest_slope{};
};

} // namespace wulfhilde
