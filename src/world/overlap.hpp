// Which of a map's sectors share space: those whose volumes overlap, which the renderer can't tell
// apart by depth alone.
#pragma once

#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/wall.hpp>

#include <span>
#include <vector>

namespace wulfhilde::detail {

/// Finds the sectors whose volumes overlap each sector's: those within whose walls, seen from
/// above, it shares some area, between heights that it shares some of too. Sectors that only meet
/// at a wall, a corner, a floor or a ceiling share no space.
///
/// Only the sectors whose bounding boxes overlap are compared, wall by wall, so a map of many
/// sectors that each touch a few others takes little longer than reading their walls.
///
/// \param sectors The map's sectors, by ID.
/// \param walls The map's walls, by ID, which the sectors' loops name.
/// \return For each sector, by its ID's index, the sectors that share space with it, in order of
///         their IDs.
[[nodiscard]] std::vector<std::vector<SectorId>> find_overlaps(std::span<const Sector> sectors,
                                                               std::span<const Wall> walls);

} // namespace wulfhilde::detail
