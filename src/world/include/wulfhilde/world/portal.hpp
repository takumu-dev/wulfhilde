/// \file
/// A portal of a map, as the map holds it.

#pragma once

#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <optional>
#include <string>

namespace wulfhilde {

/// A portal of a map, which links two of its surfaces so that each side's sector is seen and
/// reached through it from the other's.
///
/// A map holds it, and every reference in it is to that map's objects. This version of the engine
/// makes wall portals only, so both sides are walls. An ordinary portal's run between the same
/// corners in opposite directions, and a transformed portal's meet once its transform carries side
/// a onto side b. Each wall's `link` leads through it.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): it has no default, as its sides have none
struct Portal {
    /// One side of the portal, as the map's description names it first.
    SurfaceRef a;

    /// The other side of the portal.
    SurfaceRef b;

    /// The portal transform, from side a's sector's coordinates into side b's, as the map's
    /// description gives it, or nothing for an ordinary portal.
    std::optional<RigidTransform> transform{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

} // namespace wulfhilde
