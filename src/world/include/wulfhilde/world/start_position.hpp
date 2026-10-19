/// \file
/// Where the player starts in a map.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/world/location.hpp>

namespace wulfhilde {

/// The location and facing where the player is placed when a map loads. Each map has exactly one.
///
/// The facing is turned from +X by `yaw`, then `pitch`, as everywhere in the engine.
struct StartPosition {
    /// Where the player's feet are.
    Location location;

    /// How far the facing turns left from +X, anticlockwise seen from above.
    Angle yaw{};

    /// How far the facing tilts up from the horizontal.
    Angle pitch{};
};

} // namespace wulfhilde
