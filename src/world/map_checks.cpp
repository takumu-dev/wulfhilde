#include "map_checks.hpp"

#include "plan_geometry.hpp"
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wulfhilde::detail {
namespace {

/// How far from the origin a coordinate may lie on each axis, in metres. Within it, a float's
/// spacing is at most half a millimetre.
constexpr float coordinate_bound = 4096.0F;

/// The problems found so far, each at the JSON pointer of the value it lies in.
class Problems {
  public:
    /// Adds a problem.
    void add(MapProblemCode code, std::string pointer, std::string message) {
        problems_.push_back(
            {.code = code, .message = std::move(message), .pointer = std::move(pointer)});
    }

    /// Returns the problems, in the order they were found.
    [[nodiscard]] std::vector<MapProblem> take() && { return std::move(problems_); }

  private:
    std::vector<MapProblem> problems_;
};

/// Returns the JSON pointer of a value within another: an object's member or an array's element.
template <typename Key> [[nodiscard]] std::string child(std::string_view pointer, const Key& key) {
    return std::format("{}/{}", pointer, key);
}

/// Checks that a coordinate lies within the map's bounds, which a value that isn't a number
/// doesn't.
///
/// \return Whether it does.
bool check_coordinate(Problems& problems, std::string pointer, float value) {
    if (std::abs(value) <= coordinate_bound) {
        return true;
    }
    problems.add(
        MapProblemCode::coordinate_out_of_range, std::move(pointer),
        std::format("{} is beyond the map's bounds, 4,096 m either side of the origin", value));
    return false;
}

/// Checks that each coordinate of a point seen from above lies within the map's bounds.
///
/// \return Whether they all do.
bool check_point(Problems& problems, std::string_view pointer, const Vector2& point) {
    const auto x = check_coordinate(problems, child(pointer, 0), point.x);
    const auto y = check_coordinate(problems, child(pointer, 1), point.y);
    return x && y;
}

/// Checks that each coordinate of a point lies within the map's bounds.
void check_point(Problems& problems, std::string_view pointer, const Vector3& point) {
    check_coordinate(problems, child(pointer, 0), point.x);
    check_coordinate(problems, child(pointer, 1), point.y);
    check_coordinate(problems, child(pointer, 2), point.z);
}

/// Tells whether a string is an asset path: `/`-separated segments of lowercase `a`-`z`, `0`-`9`,
/// `_` and `-`, none of them empty. A path so has no extension and can't leave its content root.
[[nodiscard]] bool is_asset_path(std::string_view path) {
    // Splitting an empty string gives no segments at all, rather than one empty segment.
    if (path.empty()) {
        return false;
    }
    for (const auto segment : std::views::split(path, '/')) {
        if (segment.empty() || !std::ranges::all_of(segment, [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            })) {
            return false;
        }
    }
    return true;
}

/// Checks that a string is an asset path.
void check_asset_path(Problems& problems, std::string pointer, std::string_view path) {
    if (!is_asset_path(path)) {
        problems.add(MapProblemCode::invalid_asset_path, std::move(pointer),
                     std::format(R"("{}" isn't an asset path, which is segments of a-z, 0-9, _ )"
                                 "and - separated by / and without an extension",
                                 path));
    }
}

void check_floor_or_ceiling(Problems& problems, std::string_view pointer,
                            const FloorOrCeilingDescription& floor_or_ceiling) {
    check_coordinate(problems, child(pointer, "height"), floor_or_ceiling.height);
    check_asset_path(problems, child(pointer, "material"), floor_or_ceiling.material);
}

/// Checks that a sector's loop of at least three walls is a shape it can be bounded by: its walls
/// each of some length, not crossing or touching each other, and running anticlockwise.
void check_loop_shape(Problems& problems, std::string_view pointer,
                      std::span<const Vector2> starts) {
    const auto walls = loop_segments(starts);
    auto simple = true;
    for (std::size_t i = 0; const auto& wall : walls) {
        if (wall.start.x == wall.end.x && wall.start.y == wall.end.y) {
            problems.add(MapProblemCode::zero_length_wall, child(pointer, i),
                         "the wall has no length, since it starts where the next wall starts");
            simple = false;
        }
        ++i;
    }
    // Walls of no length would meet both their neighbours.
    if (!simple) {
        return;
    }

    // Each wall shares a corner with the walls on either side of it, which it meets only by
    // folding back over one of them. The last wall's corners are with the one before it and the
    // first.
    const auto last = walls.size() - 1;
    for (std::size_t j = 0; const auto& later : walls) {
        for (std::size_t i = 0; const auto& earlier : walls) {
            if (i == j) {
                break;
            }
            bool meet = false;
            if (j == i + 1) {
                meet = folds_back(earlier.start, earlier.end, later.end);
            } else if (i == 0 && j == last) {
                meet = folds_back(later.start, later.end, earlier.end);
            } else {
                meet = segments_meet(earlier.start, earlier.end, later.start, later.end);
            }
            if (meet) {
                problems.add(MapProblemCode::self_intersecting_loop, child(pointer, j),
                             std::format("the wall crosses or touches {}, another wall of its loop",
                                         child(pointer, i)));
                simple = false;
            }
            ++i;
        }
        ++j;
    }
    // A loop that crosses itself runs both ways at once.
    if (simple && twice_signed_area(walls) < 0.0) {
        problems.add(MapProblemCode::wrong_winding, std::string{pointer},
                     "the outer loop runs clockwise seen from above, but must run anticlockwise");
    }
}

void check_loop(Problems& problems, std::string_view pointer,
                const std::vector<WallDescription>& walls) {
    auto in_bounds = true;
    std::vector<Vector2> starts;
    for (std::size_t i = 0; const auto& wall : walls) {
        const auto wall_pointer = child(pointer, i++);
        in_bounds = check_point(problems, child(wall_pointer, "start"), wall.start) && in_bounds;
        check_asset_path(problems, child(child(wall_pointer, "surface"), "material"),
                         wall.surface.material);
        starts.push_back(wall.start);
    }
    if (walls.size() < 3) {
        problems.add(MapProblemCode::too_few_walls, std::string{pointer},
                     std::format("the loop has {} wall{}, but a loop needs at least 3",
                                 walls.size(), walls.size() == 1 ? "" : "s"));
    } else if (in_bounds) {
        // A point beyond the bounds may not be a number, so its shape would mean nothing.
        check_loop_shape(problems, pointer, starts);
    }
}

void check_sector(Problems& problems, std::string_view pointer, const SectorDescription& sector) {
    check_floor_or_ceiling(problems, child(pointer, "floor"), sector.floor);
    check_floor_or_ceiling(problems, child(pointer, "ceiling"), sector.ceiling);
    const auto loops_pointer = child(pointer, "loops");
    if (sector.loops.empty()) {
        problems.add(MapProblemCode::missing_outer_loop, loops_pointer,
                     "the sector has no loops, but needs its outer loop");
        return;
    }
    check_loop(problems, child(loops_pointer, 0), sector.loops.front());
    for (const auto i : std::views::iota(std::size_t{1}, sector.loops.size())) {
        problems.add(MapProblemCode::unsupported_inner_loop, child(loops_pointer, i),
                     "the loop is an inner loop, which this version of the engine doesn't support");
    }
}

} // namespace

std::vector<MapProblem> find_problems(const MapDescription& description) {
    Problems problems;
    std::unordered_map<std::string_view, std::size_t> sectors_by_id;
    for (std::size_t i = 0; const auto& sector : description.sectors) {
        const auto pointer = child("/sectors", i);
        const auto [first, inserted] = sectors_by_id.try_emplace(sector.id, i);
        if (!inserted) {
            problems.add(MapProblemCode::duplicate_sector_id, child(pointer, "id"),
                         std::format(R"(sector {} already has the id "{}")",
                                     child("/sectors", first->second), sector.id));
        }
        check_sector(problems, pointer, sector);
        ++i;
    }

    if (!sectors_by_id.contains(description.start.sector)) {
        problems.add(MapProblemCode::unknown_sector, "/start/sector",
                     std::format(R"(no sector has the id "{}")", description.start.sector));
    }
    check_point(problems, "/start/point", description.start.point);
    return std::move(problems).take();
}

} // namespace wulfhilde::detail
