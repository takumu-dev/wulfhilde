/// \file
/// Which surface of a map something is.

#pragma once

#include <wulfhilde/world/ids.hpp>

#include <variant>

namespace wulfhilde {

/// A sector's floor, as one of a map's surfaces.
struct SectorFloor {
    /// The sector whose floor it is.
    SectorId sector;

    /// Compares two floors by their sectors.
    constexpr bool operator==(const SectorFloor&) const noexcept = default;
};

/// A sector's ceiling, as one of a map's surfaces.
struct SectorCeiling {
    /// The sector whose ceiling it is.
    SectorId sector;

    /// Compares two ceilings by their sectors.
    constexpr bool operator==(const SectorCeiling&) const noexcept = default;
};

/// Which of a map's surfaces something is: a sector's floor or ceiling, or a wall, by its ID.
using SurfaceRef = std::variant<SectorFloor, SectorCeiling, WallId>;

} // namespace wulfhilde
