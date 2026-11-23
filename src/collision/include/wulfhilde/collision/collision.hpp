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
/// Walls stop the body and the floor and ceiling hold it between them. The move runs in substeps
/// shorter than the body's radius, each followed by a push-out of whatever the body overlaps, so
/// it can't pass through a wall, however thin. What it meets clips its velocity, so it slides
/// along a wall and stops in a corner. It holds no state: everything it needs is passed in, and
/// the game keeps the result for the next call.
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
