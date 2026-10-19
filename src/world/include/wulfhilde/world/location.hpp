/// \file
/// A location: a sector together with a point in it.

#pragma once

#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>

namespace wulfhilde {

/// Where something is: a sector together with a point in that sector's coordinates.
///
/// Sectors can overlap, so a point alone doesn't say where something is. The type doesn't hold
/// that the point lies in the sector, since a body pushed out of a wall can end a hair outside it;
/// `Map::contains` tells whether it does.
struct Location {
    /// The sector.
    SectorId sector;

    /// The point, in the sector's coordinates, in metres.
    Vector3 point{};

    /// Compares two locations exactly.
    constexpr bool operator==(const Location&) const noexcept = default;
};

} // namespace wulfhilde
