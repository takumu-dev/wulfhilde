#include "map_checks.hpp"

#include "plan_geometry.hpp"
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/asset_path.hpp>
#include <wulfhilde/world/exposure.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
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

/// Returns a point seen from above as a message writes it, `(x, y)`.
[[nodiscard]] std::string point_text(const Vector2& point) {
    return std::format("({}, {})", point.x, point.y);
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

/// Checks that an ambient light's colour filters white, each component from 0 to 1, and that its
/// light level is a finite number, 0 or more.
void check_ambient(Problems& problems, std::string_view pointer,
                   const std::optional<AmbientLight>& ambient) {
    if (!ambient) {
        return;
    }
    const auto color_pointer = child(pointer, "color");
    for (std::size_t i = 0;
         const auto component : {ambient->color.r, ambient->color.g, ambient->color.b}) {
        if (std::isnan(component) || component < 0.0F || component > 1.0F) {
            problems.add(
                MapProblemCode::color_out_of_range, child(color_pointer, i),
                std::format("{} is outside a colour component's range, 0 to 1", component));
        }
        ++i;
    }
    if (!std::isfinite(ambient->lux) || ambient->lux < 0.0F) {
        problems.add(MapProblemCode::light_level_out_of_range, child(pointer, "lux"),
                     std::format("{} lux isn't a light level, which is 0 or more", ambient->lux));
    }
}

/// Checks that each of an exposure's values is a finite number, and that its range's minimum is no
/// higher than its maximum.
void check_exposure(Problems& problems, const Exposure& exposure) {
    bool finite = true;
    for (const auto& [name, value] : {
             std::pair{"min", exposure.min},
             std::pair{"max", exposure.max},
             std::pair{"compensation", exposure.compensation},
         }) {
        if (!std::isfinite(value)) {
            problems.add(MapProblemCode::exposure_out_of_range, child("/exposure", name),
                         std::format("{} isn't an exposure, which is a finite number", value));
            finite = false;
        }
    }
    if (finite && exposure.min > exposure.max) {
        problems.add(MapProblemCode::exposure_out_of_range, "/exposure/min",
                     std::format("the exposure range's minimum, {} EV100, is above its maximum, "
                                 "{} EV100",
                                 exposure.min, exposure.max));
    }
}

/// Checks that a floor's or ceiling's height lies within the map's bounds, and that its material is
/// an asset path, or its sky's backdrop, with nits that are a finite number, 0 or more.
void check_floor_or_ceiling(Problems& problems, std::string_view pointer,
                            const FloorOrCeilingDescription& floor_or_ceiling) {
    check_coordinate(problems, child(pointer, "height"), floor_or_ceiling.height);
    if (!floor_or_ceiling.sky) {
        check_asset_path(problems, child(pointer, "material"), floor_or_ceiling.material);
        return;
    }
    const auto sky_pointer = child(pointer, "sky");
    check_asset_path(problems, child(sky_pointer, "backdrop"), floor_or_ceiling.sky->backdrop);
    if (const auto nits = floor_or_ceiling.sky->nits;
        nits && (!std::isfinite(*nits) || *nits < 0.0F)) {
        problems.add(MapProblemCode::light_level_out_of_range, child(sky_pointer, "nits"),
                     std::format("{} nits isn't a light level, which is 0 or more", *nits));
    }
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

/// A wall that has an id, as the checks of portals look it up by it.
struct NamedWall {
    /// The wall's JSON pointer.
    std::string pointer;

    /// Where the wall starts and ends, seen from above.
    Vector2 start{};
    Vector2 end{};

    /// Whether both its ends lie within the map's bounds, so that they are numbers.
    bool in_bounds = false;
};

/// The walls that have ids, by their ids. Of walls with the same id, the first is kept.
using NamedWalls = std::unordered_map<std::string_view, NamedWall>;

void check_loop(Problems& problems, std::string_view pointer,
                const std::vector<WallDescription>& walls, NamedWalls& named_walls) {
    auto in_bounds = true;
    std::vector<Vector2> starts;
    std::vector<bool> starts_in_bounds;
    // The entry of each wall that is the first with its id, or null. The map's nodes stay put.
    std::vector<NamedWall*> named;
    for (std::size_t i = 0; const auto& wall : walls) {
        const auto wall_pointer = child(pointer, i++);
        named.push_back(nullptr);
        if (wall.id) {
            const auto [first, inserted] =
                named_walls.try_emplace(*wall.id, NamedWall{.pointer = wall_pointer});
            if (inserted) {
                named.back() = &first->second;
            } else {
                problems.add(MapProblemCode::duplicate_wall_id, child(wall_pointer, "id"),
                             std::format(R"(wall {} already has the id "{}")",
                                         first->second.pointer, *wall.id));
            }
        }
        const auto start_in_bounds =
            check_point(problems, child(wall_pointer, "start"), wall.start);
        in_bounds = start_in_bounds && in_bounds;
        starts_in_bounds.push_back(start_in_bounds);
        for (const auto& [name, surface] : {
                 std::pair{"surface", std::optional(wall.surface)},
                 std::pair{"upper", wall.upper},
                 std::pair{"lower", wall.lower},
             }) {
            if (surface) {
                check_asset_path(problems, child(child(wall_pointer, name), "material"),
                                 surface->material);
            }
        }
        starts.push_back(wall.start);
    }

    // Each wall ends where the next one starts.
    const auto ends = loop_ends(starts);
    for (std::size_t i = 0; i < walls.size(); ++i) {
        if (auto* const wall = named.at(i)) {
            wall->start = starts.at(i);
            wall->end = ends.at(i);
            wall->in_bounds = starts_in_bounds.at(i) && starts_in_bounds.at((i + 1) % walls.size());
        }
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

/// Finds the first wall with an id in any sector, returning its sector's index and its JSON
/// pointer.
[[nodiscard]] std::optional<std::pair<std::size_t, std::string>>
find_wall(const MapDescription& description, std::string_view id) {
    for (std::size_t s = 0; const auto& sector : description.sectors) {
        for (std::size_t l = 0; const auto& loop : sector.loops) {
            for (std::size_t w = 0; const auto& wall : loop) {
                if (wall.id == id) {
                    return std::pair{
                        s,
                        child(child(child(child("/sectors", s), "loops"), l), w),
                    };
                }
                ++w;
            }
            ++l;
        }
        ++s;
    }
    return std::nullopt;
}

/// Checks a floor's or ceiling's slope: that its hinge is a wall of its own sector, and that its
/// gradient keeps it within the map's bounds at each of the sector's corners. Corners beyond the
/// bounds may not be numbers, so a sector with any isn't checked for the second.
void check_slope(Problems& problems, std::string_view pointer, std::string_view noun,
                 const FloorOrCeilingDescription& floor_or_ceiling, std::size_t sector_index,
                 const MapDescription& description) {
    if (!floor_or_ceiling.slope) {
        return;
    }
    const auto& slope = *floor_or_ceiling.slope;
    const auto slope_pointer = child(pointer, "slope");
    const auto& sector = description.sectors.at(sector_index);
    std::optional<PlanSegment> hinge;
    for (const auto& loop : sector.loops) {
        const auto starts =
            loop | std::views::transform(&WallDescription::start) | std::ranges::to<std::vector>();
        for (const auto& [wall, segment] : std::views::zip(loop, loop_segments(starts))) {
            if (!hinge && wall.id == slope.hinge) {
                hinge = segment;
            }
        }
    }
    if (!hinge) {
        if (const auto found = find_wall(description, slope.hinge)) {
            problems.add(MapProblemCode::hinge_outside_sector, child(slope_pointer, "hinge"),
                         std::format("the hinge, wall {}, isn't a wall of this sector, but of {}",
                                     found->second, child("/sectors", found->first)));
        } else {
            problems.add(MapProblemCode::unknown_wall, child(slope_pointer, "hinge"),
                         std::format(R"(no wall has the id "{}")", slope.hinge));
        }
        return;
    }

    const auto gradient_pointer = child(slope_pointer, "gradient");
    if (!std::isfinite(slope.gradient)) {
        problems.add(MapProblemCode::coordinate_out_of_range, gradient_pointer,
                     std::format("{} isn't a gradient, which is a finite number", slope.gradient));
        return;
    }
    const auto in_bounds = [](float value) { return std::abs(value) <= coordinate_bound; };
    const auto corners =
        sector.loops | std::views::join | std::views::transform(&WallDescription::start);
    const auto length = std::hypot(hinge->end.x - hinge->start.x, hinge->end.y - hinge->start.y);
    if (!in_bounds(floor_or_ceiling.height) || length == 0.0 ||
        !std::ranges::all_of(corners, [&](const Vector2& corner) {
            return in_bounds(corner.x) && in_bounds(corner.y);
        })) {
        return;
    }
    for (const auto& corner : corners) {
        const auto rise = static_cast<double>(slope.gradient) *
                          turn(hinge->start, hinge->end, to_plan(corner)) / length;
        const auto height = static_cast<double>(floor_or_ceiling.height) + rise;
        if (std::abs(height) > static_cast<double>(coordinate_bound)) {
            problems.add(MapProblemCode::coordinate_out_of_range, gradient_pointer,
                         std::format("the slope takes the {} to {} m at the corner {}, beyond the "
                                     "map's bounds, 4,096 m either side of the origin",
                                     noun, height, point_text(corner)));
            return;
        }
    }
}

void check_sector(Problems& problems, std::string_view pointer, const MapDescription& description,
                  std::size_t index, NamedWalls& named_walls) {
    const auto& sector = description.sectors.at(index);
    check_floor_or_ceiling(problems, child(pointer, "floor"), sector.floor);
    check_floor_or_ceiling(problems, child(pointer, "ceiling"), sector.ceiling);
    check_ambient(problems, child(pointer, "ambient"), sector.ambient);
    const auto loops_pointer = child(pointer, "loops");
    if (sector.loops.empty()) {
        problems.add(MapProblemCode::missing_outer_loop, loops_pointer,
                     "the sector has no loops, but needs its outer loop");
        return;
    }
    check_loop(problems, child(loops_pointer, 0), sector.loops.front(), named_walls);
    for (const auto i : std::views::iota(std::size_t{1}, sector.loops.size())) {
        problems.add(MapProblemCode::unsupported_inner_loop, child(loops_pointer, i),
                     "the loop is an inner loop, which this version of the engine doesn't support");
    }
    check_slope(problems, child(pointer, "floor"), "floor", sector.floor, index, description);
    check_slope(problems, child(pointer, "ceiling"), "ceiling", sector.ceiling, index, description);
}

/// Returns the name of a kind of portal side, as the file writes it.
[[nodiscard]] std::string_view kind_name(PortalSideKind kind) noexcept {
    switch (kind) {
    case PortalSideKind::wall:
        return "wall";
    case PortalSideKind::floor:
        return "floor";
    case PortalSideKind::ceiling:
        return "ceiling";
    }
    return "wall";
}

/// Checks that each portal's sides name walls or sectors that exist, that it links like with like,
/// that no wall is a side of two portals, and that a wall portal's walls run between the same
/// corners in opposite directions.
void check_portals(Problems& problems, const std::vector<PortalDescription>& portals,
                   const NamedWalls& named_walls,
                   const std::unordered_map<std::string_view, std::size_t>& sectors_by_id) {
    // The JSON pointer of the portal that each wall is a side of, by the wall's id.
    std::unordered_map<std::string_view, std::size_t> portal_of_wall;
    for (std::size_t i = 0; const auto& portal : portals) {
        const auto pointer = child("/portals", i);
        std::array<const NamedWall*, 2> walls{};
        for (auto&& [name, side, wall] :
             std::views::zip(std::array{"a", "b"}, std::array{&portal.a, &portal.b}, walls)) {
            const auto side_pointer = child(child(pointer, name), kind_name(side->kind));
            if (side->kind != PortalSideKind::wall) {
                if (!sectors_by_id.contains(side->id)) {
                    problems.add(MapProblemCode::unknown_sector, side_pointer,
                                 std::format(R"(no sector has the id "{}")", side->id));
                }
                continue;
            }
            const auto found = named_walls.find(side->id);
            if (found == named_walls.end()) {
                problems.add(MapProblemCode::unknown_wall, side_pointer,
                             std::format(R"(no wall has the id "{}")", side->id));
                continue;
            }
            wall = &found->second;
            const auto [first, inserted] = portal_of_wall.try_emplace(side->id, i);
            if (!inserted && first->second != i) {
                problems.add(MapProblemCode::wall_in_two_portals, side_pointer,
                             std::format("the wall {} is already a side of {}", wall->pointer,
                                         child("/portals", first->second)));
            }
        }

        const auto a_is_wall = portal.a.kind == PortalSideKind::wall;
        const auto b_is_wall = portal.b.kind == PortalSideKind::wall;
        if (a_is_wall != b_is_wall) {
            problems.add(MapProblemCode::unlike_portal_sides, pointer,
                         std::format("the portal links a {} to a {}, but walls link only to walls",
                                     kind_name(portal.a.kind), kind_name(portal.b.kind)));
        } else if (!a_is_wall) {
            problems.add(MapProblemCode::unsupported_portal, pointer,
                         std::format("the portal links a {} to a {}, which this version of the "
                                     "engine doesn't support",
                                     kind_name(portal.a.kind), kind_name(portal.b.kind)));
        }

        const auto [a, b] = walls;
        // Corners beyond the bounds may not be numbers, so comparing them would mean nothing.
        if (a != nullptr && b != nullptr && a->in_bounds && b->in_bounds &&
            (a->start != b->end || a->end != b->start)) {
            problems.add(MapProblemCode::mismatched_portal_walls, pointer,
                         std::format("the portal's walls must run between the same corners in "
                                     "opposite directions, but {} runs from {} to {}, and {} "
                                     "from {} to {}",
                                     a->pointer, point_text(a->start), point_text(a->end),
                                     b->pointer, point_text(b->start), point_text(b->end)));
        }
        ++i;
    }
}

} // namespace

std::vector<MapProblem> find_problems(const MapDescription& description) {
    Problems problems;
    check_ambient(problems, "/ambient", description.ambient);
    check_exposure(problems, description.exposure);
    std::unordered_map<std::string_view, std::size_t> sectors_by_id;
    NamedWalls named_walls;
    for (std::size_t i = 0; const auto& sector : description.sectors) {
        const auto pointer = child("/sectors", i);
        const auto [first, inserted] = sectors_by_id.try_emplace(sector.id, i);
        if (!inserted) {
            problems.add(MapProblemCode::duplicate_sector_id, child(pointer, "id"),
                         std::format(R"(sector {} already has the id "{}")",
                                     child("/sectors", first->second), sector.id));
        }
        check_sector(problems, pointer, description, i, named_walls);
        ++i;
    }
    check_portals(problems, description.portals, named_walls, sectors_by_id);

    if (!sectors_by_id.contains(description.start.sector)) {
        problems.add(MapProblemCode::unknown_sector, "/start/sector",
                     std::format(R"(no sector has the id "{}")", description.start.sector));
    }
    check_point(problems, "/start/point", description.start.point);
    return std::move(problems).take();
}

} // namespace wulfhilde::detail
