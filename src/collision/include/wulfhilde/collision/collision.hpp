/// \file
/// Moving bodies through a map, and placing them in it.

#pragma once

#include <wulfhilde/collision/body_shape.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>

#include <optional>

namespace wulfhilde {

/// Moves a body through a map at a velocity for a time, stopping it at what it meets and sliding it
/// along.
///
/// Walls stop the body and the floor and ceiling hold it between them. Where a wall is a side of a
/// portal, the part of the body that comes through its opening is in the sector beyond, whose
/// surfaces stop it there, and the strips above and below the opening stop it as a wall does. The
/// body's sector changes only when its centre crosses an opening. The move runs in substeps
/// shorter than the body's radius, each followed by a push-out of whatever the body overlaps, so
/// it can't pass through a wall, however thin. What it meets clips its velocity, so it slides
/// along a wall and stops in a corner. It holds no state: everything it needs is passed in, and
/// the game keeps the result for the next call.
///
/// Steps follow Build's rule on a capsule. A strip below an opening, or the floor beyond, doesn't
/// stop the body where it is at most the body's step height above its feet, measured along its
/// gravity up, if the body fits in the opening: an opening as tall as the body passes it, and a
/// lower one stops it. The sector under the body's centre decides its height, so it is lifted
/// onto the step at once as its centre crosses the opening. A body that stood on ground and walks
/// off it, rather than leaving it upward, snaps down onto ground up to its step height below. Both
/// are reported as instant steps.
///
/// \param map The map.
/// \param shape The body's shape.
/// \param pose Where the body starts, whose sector `map` issued.
/// \param velocity How fast the body moves and which way, in the coordinates of its start's
///                 sector, in metres per second.
/// \param seconds How long it moves for, zero or more. The move, its speed times this, is at most
///                16,384 m, twice the width of the largest map.
/// \return Where the body ended, its clipped velocity, and what it stands on.
[[nodiscard]] Movement move(const Map& map, const BodyShape& shape, const Pose& pose,
                            const Vector3& velocity, float seconds);

/// Places a body in a map if it fits there, pushing it out of what it overlaps by a little.
///
/// A game asks this before it places a body, and before each step of righting a tilted body, which
/// waits while the step doesn't fit.
///
/// \param map The map.
/// \param shape The body's shape.
/// \param pose Where the body would be, whose sector `map` issued.
/// \return The pose, pushed out of what it overlapped, if the body fits after a push-out of at most
///         a third of its radius; or nothing if it doesn't.
[[nodiscard]] std::optional<Pose> settle(const Map& map, const BodyShape& shape, const Pose& pose);

} // namespace wulfhilde
