/// \file
/// The face of a wall, in the parts it is drawn and met as.

#pragma once

#include <wulfhilde/core/vector3.hpp>

#include <vector>

namespace wulfhilde {

/// The face of a wall, which stands from its sector's floor to its ceiling, and the parts of it
/// that a side of a portal is split into.
///
/// Each part is a convex polygon in the wall's upright plane, given by its corners in the wall's
/// sector's coordinates, which run anticlockwise as the wall is seen from its sector. A part that
/// has no area has no corners. A floor or ceiling that slopes makes the edges along it slope, and
/// where two floors or two ceilings cross along a side of a portal, a strip ends in a point there.
struct WallFace {
    /// The whole wall, from the floor to the ceiling, where the ceiling is above the floor.
    std::vector<Vector3> whole{};

    /// A side of a portal's lower strip: the part of the whole wall below the floor beyond. A solid
    /// wall has none.
    std::vector<Vector3> lower{};

    /// A side of a portal's opening: the part of the whole wall from the floor beyond to the
    /// ceiling beyond. A solid wall has none.
    std::vector<Vector3> opening{};

    /// A side of a portal's upper strip: the part of the whole wall above both the ceiling and the
    /// floor beyond. A solid wall has none.
    std::vector<Vector3> upper{};
};

} // namespace wulfhilde
