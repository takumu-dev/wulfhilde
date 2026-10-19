#pragma once

#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <vector>

namespace wulfhilde::detail {

/// Checks a description against every rule a map must keep.
///
/// \param description The description.
/// \return Every problem in it, sector by sector in the description's order and then the start
///         position's, or nothing if a map can be made from it.
[[nodiscard]] std::vector<MapProblem> find_problems(const MapDescription& description);

} // namespace wulfhilde::detail
