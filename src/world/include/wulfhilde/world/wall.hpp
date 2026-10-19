/// \file
/// A wall of a map, as the map holds it.

#pragma once

#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/surface.hpp>

namespace wulfhilde {

/// A wall of a map, which runs between two corners of a sector's loop, seen from above, and stands
/// from the sector's floor to its ceiling.
///
/// It faces into its sector, which lies to its left as it runs from its start to its end.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): it has no default, as its sector has none
struct Wall {
    /// The sector whose loop the wall is in.
    SectorId sector;

    /// Where the wall starts, seen from above, in metres.
    Vector2 start{};

    /// Where the wall ends, which is where the next wall in its loop starts.
    Vector2 end{};

    /// What covers the wall, with u along it from its start and v up.
    Surface surface;
};

} // namespace wulfhilde
