/// \file
/// Where a trace through a map ended, and what stopped it.

#pragma once

#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <optional>

namespace wulfhilde {

/// Where a trace, a straight move of a location through a map, ended, and what stopped it.
struct Trace {
    /// Where the trace ended: at the surface that stopped it, or where its whole motion took it.
    Location end;

    /// The combined transform of the portals the trace crossed, from its start's sector's
    /// coordinates into its end's. It is the identity when it crossed none.
    RigidTransform transform = RigidTransform::identity();

    /// The surface that stopped the trace, or nothing if it made its whole motion.
    std::optional<SurfaceRef> surface{};
};

} // namespace wulfhilde
