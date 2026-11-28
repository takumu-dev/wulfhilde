/// \file
/// The way through a portal from one of its sides.

#pragma once

#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/world/ids.hpp>

namespace wulfhilde {

/// The way through a portal from one of its walls: the portal, the wall on its other side, and the
/// transform into the sector beyond.
///
/// A map works out both of a portal's links when it is made, one for each of its sides. The wall on
/// the other side faces into the sector beyond. The link from side b holds the inverse of the link
/// from side a's transform, so crossing either way is a lookup.
struct PortalLink {
    /// The portal.
    PortalId portal;

    /// The wall on the portal's other side.
    WallId wall;

    /// The transform from the coordinates of this side's sector into the sector beyond's: the
    /// identity, exactly, through an ordinary portal.
    RigidTransform transform = RigidTransform::identity();

    /// Compares two links exactly.
    constexpr bool operator==(const PortalLink&) const noexcept = default;
};

} // namespace wulfhilde
