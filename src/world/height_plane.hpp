// A sector's floor or ceiling as heights over the plan, which the map's queries and the search for
// sectors that share space both read.
#pragma once

#include "plan_geometry.hpp"
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/wall.hpp>

#include <cmath>

namespace wulfhilde::detail {

/// A floor or ceiling as heights over the plan, in double, so that a slope keeps every bit of a
/// float's precision far from the origin: its height at a point is its height along its hinge, plus
/// its gradient times the point's distance from the hinge's line, into the sector.
struct HeightPlane {
    double height = 0.0;   ///< The height along the hinge.
    double gradient = 0.0; ///< The rise for each metre into the sector.
    PlanPoint origin{};    ///< A point of the hinge's line.
    PlanPoint inward{};    ///< The direction into the sector, square to the hinge.

    /// Returns the height at a point seen from above.
    [[nodiscard]] double at(const PlanPoint& point) const noexcept {
        return height +
               (gradient * (((point.x - origin.x) * inward.x) + ((point.y - origin.y) * inward.y)));
    }
};

/// Returns a floor or ceiling as heights over the plan.
///
/// \param floor_or_ceiling The floor or ceiling.
/// \param hinge The wall its slope is hinged on, or null if it is flat.
[[nodiscard]] inline HeightPlane plane_of(const FloorOrCeiling& floor_or_ceiling,
                                          const Wall* hinge) {
    HeightPlane plane{.height = static_cast<double>(floor_or_ceiling.height)};
    if (!floor_or_ceiling.slope || hinge == nullptr) {
        return plane;
    }
    const auto start = to_plan(hinge->start);
    const auto end = to_plan(hinge->end);
    const auto run_x = end.x - start.x;
    const auto run_y = end.y - start.y;
    const auto length = std::hypot(run_x, run_y);
    plane.gradient = static_cast<double>(floor_or_ceiling.slope->gradient);
    plane.origin = start;
    // A wall faces into its sector, to its left.
    plane.inward = {.x = -run_y / length, .y = run_x / length};
    return plane;
}

} // namespace wulfhilde::detail
