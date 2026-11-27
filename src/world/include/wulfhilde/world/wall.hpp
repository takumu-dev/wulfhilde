/// \file
/// A wall of a map, as the map holds it.

#pragma once

#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/portal_link.hpp>
#include <wulfhilde/world/surface.hpp>

#include <optional>
#include <string>

namespace wulfhilde {

/// A wall of a map, which runs between two corners of a sector's loop, seen from above, and stands
/// from the sector's floor to its ceiling.
///
/// It faces into its sector, which lies to its left as it runs from its start to its end. A wall
/// that is a side of a portal is open where the sector beyond overlaps its own, and solid only
/// above and below that, on its strips.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): it has no default, as its sector has none
struct Wall {
    /// The sector whose loop the wall is in.
    SectorId sector;

    /// The wall's name, unique among its map's walls, as the map's description gives it, or nothing
    /// if it gives none.
    std::optional<std::string> id{};

    /// Where the wall starts, seen from above, in metres.
    Vector2 start{};

    /// Where the wall ends, which is where the next wall in its loop starts.
    Vector2 end{};

    /// What covers the wall, with u along it from its start, to the right as the wall is seen from
    /// its sector, and v up.
    Surface surface;

    /// What covers the wall's upper strip, laid as `surface` is: its own, or else `surface`.
    Surface upper;

    /// What covers the wall's lower strip, laid as `surface` is: its own, or else `surface`.
    Surface lower;

    /// The way through the portal the wall is a side of, or nothing for a solid wall.
    std::optional<PortalLink> link{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

} // namespace wulfhilde
