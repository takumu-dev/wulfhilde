/// \file
/// The way through a portal from one of its sides.

#pragma once

#include <wulfhilde/world/ids.hpp>

namespace wulfhilde {

/// The way through a portal from one of its walls: the portal, and the wall on its other side.
///
/// A map works out both of a portal's links when it is made, one for each of its sides. The wall on
/// the other side faces into the sector beyond.
struct PortalLink {
    /// The portal.
    PortalId portal;

    /// The wall on the portal's other side.
    WallId wall;

    /// Compares two links exactly.
    constexpr bool operator==(const PortalLink&) const noexcept = default;
};

} // namespace wulfhilde
