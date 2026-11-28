#include "overlap.hpp"

#include "plan_geometry.hpp"
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/wall.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

namespace wulfhilde::detail {

namespace {

// A sector as the search for overlaps sees it: its walls seen from above, each with the sector to
// its left, and the box that holds it.
struct Volume {
    std::vector<PlanSegment> walls{};
    PlanPoint lowest{
        .x = std::numeric_limits<double>::infinity(),
        .y = std::numeric_limits<double>::infinity(),
    };
    PlanPoint highest{
        .x = -std::numeric_limits<double>::infinity(),
        .y = -std::numeric_limits<double>::infinity(),
    };
    double floor = 0.0;
    double ceiling = 0.0;
};

Volume volume_of(const Sector& sector, std::span<const Wall> walls) {
    Volume volume{
        .floor = static_cast<double>(sector.floor.height),
        .ceiling = static_cast<double>(sector.ceiling.height),
    };
    for (const auto& loop : sector.loops) {
        for (const auto id : loop) {
            const auto& wall = walls.subspan(id.index(), 1).front();
            const PlanSegment segment{.start = to_plan(wall.start), .end = to_plan(wall.end)};
            volume.walls.push_back(segment);
            volume.lowest = {
                .x = std::min(volume.lowest.x, segment.start.x),
                .y = std::min(volume.lowest.y, segment.start.y),
            };
            volume.highest = {
                .x = std::max(volume.highest.x, segment.start.x),
                .y = std::max(volume.highest.y, segment.start.y),
            };
        }
    }
    return volume;
}

// Whether two of a sector's walls cross at a point inside each, where each passes from one side
// of the other to the other side.
bool cross(const PlanSegment& a, const PlanSegment& b) {
    const auto b_start = turn(a.start, a.end, b.start);
    const auto b_end = turn(a.start, a.end, b.end);
    const auto a_start = turn(b.start, b.end, a.start);
    const auto a_end = turn(b.start, b.end, a.end);
    return ((b_start > 0.0 && b_end < 0.0) || (b_start < 0.0 && b_end > 0.0)) &&
           ((a_start > 0.0 && a_end < 0.0) || (a_start < 0.0 && a_end > 0.0));
}

// Whether a point is inside a sector's walls seen from above, and not on one of them.
bool strictly_inside(std::span<const PlanSegment> walls, const PlanPoint& point) {
    // The winding number of the walls about the point, as `Map::contains` counts it.
    auto winding = 0;
    for (const auto& [a, b] : walls) {
        const auto side = turn(a, b, point);
        if (side == 0.0 && within(a, b, point)) {
            return false;
        }
        if (a.y <= point.y && point.y < b.y && side > 0.0) {
            ++winding;
        } else if (b.y <= point.y && point.y < a.y && side < 0.0) {
            --winding;
        }
    }
    return winding != 0;
}

// How far along a wall a point in line with it lies: 0 at its start and 1 at its end.
double along(const PlanSegment& wall, const PlanPoint& point) {
    const auto dx = wall.end.x - wall.start.x;
    const auto dy = wall.end.y - wall.start.y;
    return (((point.x - wall.start.x) * dx) + ((point.y - wall.start.y) * dy)) /
           ((dx * dx) + (dy * dy));
}

// How a piece of a wall, around a point along it, lies on the other sector's walls.
enum class Alongside : std::uint8_t {
    none,     // It lies on none of them.
    same_way, // It runs along one the same way, so both sectors lie to its left.
    opposite, // It runs along one the opposite way, where the sectors meet side by side.
};

// How the piece of a wall around a point along it lies on another sector's walls, given that the
// wall is cut wherever the other's corners lie on it.
Alongside alongside(const PlanSegment& wall, double middle, std::span<const PlanSegment> other) {
    auto found = Alongside::none;
    for (const auto& [start, end] : other) {
        if (turn(wall.start, wall.end, start) != 0.0 || turn(wall.start, wall.end, end) != 0.0) {
            continue;
        }
        const auto start_along = along(wall, start);
        const auto end_along = along(wall, end);
        if (std::min(start_along, end_along) < middle &&
            middle < std::max(start_along, end_along)) {
            if (start_along < end_along) {
                return Alongside::same_way;
            }
            found = Alongside::opposite;
        }
    }
    return found;
}

// Where along a wall another sector's corners lie on it, from 0 at its start to 1 at its end, both
// of which are among them, in order.
std::vector<double> cuts_of(const PlanSegment& wall, std::span<const PlanSegment> other) {
    std::vector<double> cuts{0.0, 1.0};
    for (const auto& [start, end] : other) {
        if (turn(wall.start, wall.end, start) == 0.0 && within(wall.start, wall.end, start)) {
            cuts.push_back(along(wall, start));
        }
    }
    std::ranges::sort(cuts);
    return cuts;
}

// Whether any part of one sector's walls, seen from above, lies inside the other's, or runs along
// one of the other's walls with both sectors to the same side, so that their insides meet there.
// Their walls mustn't cross.
//
// Each wall is cut where the other's corners lie on it. A piece that runs along one of the other's
// walls the opposite way is where the sectors meet side by side, and any other piece lies inside
// the other sector or outside it all along, as its middle does.
bool reaches_inside(std::span<const PlanSegment> walls, std::span<const PlanSegment> other) {
    for (const auto& wall : walls) {
        const auto cuts = cuts_of(wall, other);
        for (const auto [from, to] : std::views::zip(cuts, cuts | std::views::drop(1))) {
            if (from == to) {
                continue;
            }
            const auto middle = (from + to) / 2.0;
            const auto lies = alongside(wall, middle, other);
            const PlanPoint point{
                .x = wall.start.x + ((wall.end.x - wall.start.x) * middle),
                .y = wall.start.y + ((wall.end.y - wall.start.y) * middle),
            };
            if (lies == Alongside::same_way ||
                (lies == Alongside::none && strictly_inside(other, point))) {
                return true;
            }
        }
    }
    return false;
}

// Whether two sectors share space: some height, and some area within their walls seen from above.
bool overlap(const Volume& a, const Volume& b) {
    if (a.floor >= b.ceiling || b.floor >= a.ceiling) {
        return false;
    }
    for (const auto& wall : a.walls) {
        for (const auto& other : b.walls) {
            if (cross(wall, other)) {
                return true;
            }
        }
    }
    return reaches_inside(a.walls, b.walls) || reaches_inside(b.walls, a.walls);
}

} // namespace

std::vector<std::vector<SectorId>> find_overlaps(std::span<const Sector> sectors,
                                                 std::span<const Wall> walls) {
    std::vector<Volume> volumes;
    volumes.reserve(sectors.size());
    for (const auto& sector : sectors) {
        volumes.push_back(volume_of(sector, walls));
    }

    // Sweeping across X in the order the boxes start, each box is compared with those that start
    // before it ends, and of those, only the ones whose boxes overlap in Y and in height too.
    std::vector<std::size_t> order(volumes.size());
    std::ranges::iota(order, std::size_t{0});
    std::ranges::sort(order, {},
                      [&volumes](std::size_t index) { return volumes.at(index).lowest.x; });
    std::vector<std::vector<SectorId>> overlapping(sectors.size());
    for (auto first = order.begin(); first != order.end(); ++first) {
        const auto& a = volumes.at(*first);
        for (auto second = std::next(first);
             second != order.end() && volumes.at(*second).lowest.x < a.highest.x; ++second) {
            const auto& b = volumes.at(*second);
            if (b.lowest.y >= a.highest.y || a.lowest.y >= b.highest.y || !overlap(a, b)) {
                continue;
            }
            overlapping.at(*first).emplace_back(narrow<std::uint32_t>(*second));
            overlapping.at(*second).emplace_back(narrow<std::uint32_t>(*first));
        }
    }
    for (auto& found : overlapping) {
        std::ranges::sort(found);
    }
    return overlapping;
}

} // namespace wulfhilde::detail
