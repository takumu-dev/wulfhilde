#include <wulfhilde/world/map.hpp>

#include "map_checks.hpp"
#include "plan_geometry.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_load_error.hpp>
#include <wulfhilde/world/opening.hpp>
#include <wulfhilde/world/portal.hpp>
#include <wulfhilde/world/portal_link.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/sky.hpp>
#include <wulfhilde/world/surface.hpp>
#include <wulfhilde/world/surface_ref.hpp>
#include <wulfhilde/world/trace.hpp>
#include <wulfhilde/world/wall.hpp>
#include <wulfhilde/world/wall_face.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wulfhilde {
namespace {

/// Gives each distinct asset path an ID, in the order they are first seen.
template <typename AssetId> class Interner {
  public:
    /// Returns a path's ID, giving it the next one if it has none yet.
    AssetId intern(std::string path) {
        const auto [found, inserted] =
            ids_.try_emplace(std::move(path), narrow<std::uint32_t>(ids_.size()));
        if (inserted) {
            paths_.push_back(found->first);
        }
        return found->second;
    }

    /// Returns each path, by ID.
    [[nodiscard]] std::vector<std::string> take_paths() && { return std::move(paths_); }

  private:
    std::unordered_map<std::string, AssetId> ids_;
    std::vector<std::string> paths_;
};

using MaterialInterner = Interner<MaterialId>;
using BackdropInterner = Interner<BackdropId>;

/// Returns a surface as a map holds it.
[[nodiscard]] Surface make_surface(MaterialInterner& materials, SurfaceDescription description) {
    return {
        .material = materials.intern(std::move(description.material)),
        .offset = description.offset,
        .scale = description.scale,
        .rotation = description.rotation,
    };
}

/// Returns the ID a wall of a sector's loops will have, by its id, given the ID of the sector's
/// first wall. The checks have made sure that the loops have it.
[[nodiscard]] WallId wall_of_loops(const std::vector<std::vector<WallDescription>>& loops,
                                   std::size_t first, const std::string& id) {
    const auto walls = loops | std::views::join;
    const auto found = std::ranges::find(walls, std::optional(id), &WallDescription::id);
    return WallId{narrow<std::uint32_t>(
        first + static_cast<std::size_t>(std::ranges::distance(walls.begin(), found)))};
}

/// Returns a floor or ceiling of a sector as a map holds it, given the sector's loops and the ID
/// of its first wall.
[[nodiscard]] FloorOrCeiling
make_floor_or_ceiling(MaterialInterner& materials, BackdropInterner& backdrops,
                      const std::vector<std::vector<WallDescription>>& loops,
                      std::size_t first_wall, FloorOrCeilingDescription description) {
    std::optional<Slope> slope;
    if (description.slope) {
        slope = Slope{
            .hinge = wall_of_loops(loops, first_wall, description.slope->hinge),
            .gradient = description.slope->gradient,
        };
    }
    if (description.sky) {
        return {
            .height = description.height,
            .slope = slope,
            .cover =
                Sky{
                    .backdrop = backdrops.intern(std::move(description.sky->backdrop)),
                    .nits = description.sky->nits,
                },
        };
    }
    return {
        .height = description.height,
        .slope = slope,
        .cover = make_surface(materials,
                              {
                                  .material = std::move(description.material),
                                  .offset = description.offset,
                                  .scale = description.scale,
                                  .rotation = description.rotation,
                              }),
    };
}

/// A floor or ceiling as heights over the plan, in double, so that a slope keeps every bit of a
/// float's precision far from the origin: its height at a point is its height along its hinge, plus
/// its gradient times the point's distance from the hinge's line, into the sector.
struct HeightPlane {
    double height = 0.0;        ///< The height along the hinge.
    double gradient = 0.0;      ///< The rise for each metre into the sector.
    detail::PlanPoint origin{}; ///< A point of the hinge's line.
    detail::PlanPoint inward{}; ///< The direction into the sector, square to the hinge.

    /// Returns the height at a point seen from above.
    [[nodiscard]] double at(const detail::PlanPoint& point) const noexcept {
        return height +
               (gradient * (((point.x - origin.x) * inward.x) + ((point.y - origin.y) * inward.y)));
    }
};

/// Returns a floor or ceiling of a map as heights over the plan.
[[nodiscard]] HeightPlane plane_of(const Map& map, const FloorOrCeiling& floor_or_ceiling) {
    HeightPlane plane{.height = static_cast<double>(floor_or_ceiling.height)};
    if (!floor_or_ceiling.slope) {
        return plane;
    }
    const auto& hinge = map.wall(floor_or_ceiling.slope->hinge);
    const auto start = detail::to_plan(hinge.start);
    const auto end = detail::to_plan(hinge.end);
    const auto run_x = end.x - start.x;
    const auto run_y = end.y - start.y;
    const auto length = std::hypot(run_x, run_y);
    plane.gradient = static_cast<double>(floor_or_ceiling.slope->gradient);
    plane.origin = start;
    // A wall faces into its sector, to its left.
    plane.inward = {.x = -run_y / length, .y = run_x / length};
    return plane;
}

/// Returns the direction a floor or ceiling faces, of unit length: up for a floor and down for a
/// ceiling, tilted by its slope.
[[nodiscard]] Vector3 normal_of(const HeightPlane& plane, double up) {
    const auto x = -up * plane.gradient * plane.inward.x;
    const auto y = -up * plane.gradient * plane.inward.y;
    const auto length = std::sqrt((x * x) + (y * y) + 1.0);
    return {
        .x = static_cast<float>(x / length),
        .y = static_cast<float>(y / length),
        .z = static_cast<float>(up / length),
    };
}

/// A point of a wall's upright plane: how far along the wall, from 0 at its start to 1 at its end,
/// and its height.
struct FacePoint {
    double along = 0.0; ///< How far along the wall.
    double z = 0.0;     ///< The height.
};

/// A floor or ceiling along a wall, as heights that run straight from the wall's start to its end.
struct FaceLine {
    double start = 0.0; ///< The height at the wall's start.
    double end = 0.0;   ///< The height at the wall's end.

    /// Returns how far a point lies above the line.
    [[nodiscard]] double above(const FacePoint& point) const noexcept {
        return point.z - (start + ((end - start) * point.along));
    }
};

/// Returns the part of a convex polygon of a wall's plane above a line, or below it, keeping the
/// order of its corners.
[[nodiscard]] std::vector<FacePoint> clip_face(const std::vector<FacePoint>& polygon,
                                               const FaceLine& line, bool keep_above) {
    std::vector<FacePoint> kept;
    const auto side = [&](const FacePoint& point) {
        return keep_above ? line.above(point) : -line.above(point);
    };
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto& from = polygon.at(i);
        const auto& to = polygon.at((i + 1) % polygon.size());
        const auto from_side = side(from);
        const auto to_side = side(to);
        if (from_side >= 0.0) {
            kept.push_back(from);
        }
        if ((from_side > 0.0 && to_side < 0.0) || (from_side < 0.0 && to_side > 0.0)) {
            const auto t = from_side / (from_side - to_side);
            kept.push_back({
                .along = from.along + ((to.along - from.along) * t),
                .z = from.z + ((to.z - from.z) * t),
            });
        }
    }
    return kept;
}

/// Returns a polygon of a wall's plane as corners in its sector's coordinates, or none if it has
/// no area.
[[nodiscard]] std::vector<Vector3> face_corners(const std::vector<FacePoint>& polygon,
                                                const detail::PlanPoint& start,
                                                const detail::PlanPoint& end) {
    auto twice_area = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto& a = polygon.at(i);
        const auto& b = polygon.at((i + 1) % polygon.size());
        twice_area += (a.along * b.z) - (b.along * a.z);
    }
    // The corners run clockwise as along and z run, which is anticlockwise seen from the sector,
    // where the wall's start is on the right.
    if (polygon.size() < 3 || twice_area >= 0.0) {
        return {};
    }
    std::vector<Vector3> corners;
    corners.reserve(polygon.size());
    for (const auto& point : polygon) {
        corners.push_back({
            .x = static_cast<float>(start.x + ((end.x - start.x) * point.along)),
            .y = static_cast<float>(start.y + ((end.y - start.y) * point.along)),
            .z = static_cast<float>(point.z),
        });
    }
    return corners;
}

/// The straight line a trace runs along, in double. Ordinary portals keep every sector in the same
/// coordinates, so it is one line through them all, measured from 0 at its start to 1 at its
/// finish, and each wall's line is met at the same point of it from whichever sector.
struct TraceLine {
    detail::PlanPoint start{};  ///< Where it starts, seen from above.
    detail::PlanPoint finish{}; ///< Where it finishes, seen from above.
    double z = 0.0;             ///< The height it starts at.
    double rise = 0.0;          ///< How far it rises from its start to its finish.
};

/// How far along a trace's line it reaches a wall's line from the wall's sector, between the
/// wall's ends, having entered the sector at a point of the line.
///
/// The ends are judged by the side of the line they lie on, so two walls that share a corner can't
/// both miss a line through it.
[[nodiscard]] std::optional<double> reaches_wall(const TraceLine& line,
                                                 const detail::PlanPoint& entry,
                                                 const detail::PlanPoint& a,
                                                 const detail::PlanPoint& b) {
    const auto entry_side = detail::turn(a, b, entry);
    const auto finish_side = detail::turn(a, b, line.finish);
    // A trace may enter or end on the wall's line, but not run along it.
    if (entry_side < 0.0 || finish_side > 0.0 || (entry_side == 0.0 && finish_side == 0.0)) {
        return std::nullopt;
    }
    const auto a_side = detail::turn(line.start, line.finish, a);
    const auto b_side = detail::turn(line.start, line.finish, b);
    if ((a_side < 0.0 || b_side > 0.0) && (a_side > 0.0 || b_side < 0.0)) {
        return std::nullopt;
    }
    const auto start_side = detail::turn(a, b, line.start);
    return start_side / (start_side - finish_side);
}

/// Returns the sector beyond a wall that a trace reaching it at a point of its line crosses into:
/// the far sector of a side of a portal, if the point is within its opening, its edges included.
[[nodiscard]] std::optional<SectorId> beyond_opening(const Map& map, WallId id,
                                                     const TraceLine& line, double at) {
    const auto& link = map.wall(id).link;
    if (!link) {
        return std::nullopt;
    }
    const Vector2 reached{
        .x = static_cast<float>(line.start.x + ((line.finish.x - line.start.x) * at)),
        .y = static_cast<float>(line.start.y + ((line.finish.y - line.start.y) * at)),
    };
    const auto opening = map.opening(id, reached);
    const auto height = line.z + (line.rise * at);
    if (!opening || height < static_cast<double>(opening->bottom) ||
        height > static_cast<double>(opening->top)) {
        return std::nullopt;
    }
    return map.wall(link->wall).sector;
}

/// Where a trace leaves one sector: at the surface that stops it, through an opening, or at the
/// finish of its line.
struct SectorTrace {
    /// How far along the line, from 0 to 1.
    double at = 1.0;

    /// The surface that stops the trace there, if one does.
    std::optional<SurfaceRef> surface{};

    /// The sector beyond the opening the trace crosses there, if it crosses one.
    std::optional<SectorId> beyond{};
};

/// Follows a trace's line through one sector, from where it entered the sector.
///
/// Of two surfaces met at once, the first found stops it, and a surface met where an opening is
/// reached stops it there.
[[nodiscard]] SectorTrace trace_sector(const Map& map, SectorId id, const TraceLine& line,
                                       double entered) {
    const auto& here = map.sector(id);
    SectorTrace stop;
    const auto meet = [&stop](double at, const SurfaceRef& met) {
        if (stop.surface ? at < stop.at : at <= stop.at) {
            stop.at = at;
            stop.surface = met;
        }
    };
    // Along the line, a floor's or a ceiling's height runs straight too, and the trace meets it
    // where the gap between them, closing, reaches nothing.
    const auto meet_plane = [&](const FloorOrCeiling& floor_or_ceiling, double up,
                                const SurfaceRef& met) {
        const auto plane = plane_of(map, floor_or_ceiling);
        const auto at_start = plane.at(line.start);
        const auto gap = up * (line.z - at_start);
        const auto widening = up * (line.rise - (plane.at(line.finish) - at_start));
        if (widening < 0.0 && gap + (widening * entered) >= 0.0) {
            meet(-gap / widening, met);
        }
    };
    meet_plane(here.floor, 1.0, SectorFloor{id});
    meet_plane(here.ceiling, -1.0, SectorCeiling{id});

    const detail::PlanPoint entry{
        .x = line.start.x + ((line.finish.x - line.start.x) * entered),
        .y = line.start.y + ((line.finish.y - line.start.y) * entered),
    };
    std::optional<SectorTrace> crossing;
    for (const auto& loop : here.loops) {
        for (const auto wall_id : loop) {
            const auto& wall = map.wall(wall_id);
            const auto at =
                reaches_wall(line, entry, detail::to_plan(wall.start), detail::to_plan(wall.end));
            if (!at) {
                continue;
            }
            const auto beyond = beyond_opening(map, wall_id, line, *at);
            if (!beyond) {
                meet(*at, wall_id);
            } else if (!crossing || *at < crossing->at) {
                crossing = SectorTrace{.at = *at, .beyond = beyond};
            }
        }
    }
    if (crossing && (!stop.surface || crossing->at < stop.at)) {
        return *crossing;
    }
    return stop;
}

} // namespace

std::expected<Map, MapLoadError> Map::create(MapDescription description) {
    if (auto problems = detail::find_problems(description); !problems.empty()) {
        return std::unexpected(MapLoadError{.problems = std::move(problems)});
    }

    const auto start_sector =
        std::ranges::find(description.sectors, description.start.sector, &SectorDescription::id);
    Map map{{
        .location =
            {
                .sector =
                    SectorId{narrow<std::uint32_t>(start_sector - description.sectors.begin())},
                .point = description.start.point,
            },
        .yaw = description.start.yaw,
        .pitch = description.start.pitch,
    }};
    map.exposure_ = description.exposure;
    map.properties_ = std::move(description.properties);
    const auto ambient = description.ambient.value_or(AmbientLight{});
    MaterialInterner materials;
    BackdropInterner backdrops;
    std::unordered_map<std::string, WallId> walls_by_name;
    for (auto& sector : description.sectors) {
        const SectorId id{narrow<std::uint32_t>(map.sectors_.size())};
        const auto floor = make_floor_or_ceiling(materials, backdrops, sector.loops,
                                                 map.walls_.size(), std::move(sector.floor));
        const auto ceiling = make_floor_or_ceiling(materials, backdrops, sector.loops,
                                                   map.walls_.size(), std::move(sector.ceiling));

        std::vector<std::vector<WallId>> loops;
        for (auto& walls : sector.loops) {
            const auto starts = walls | std::views::transform(&WallDescription::start) |
                                std::ranges::to<std::vector>();
            const auto ends = detail::loop_ends(starts);

            auto& loop = loops.emplace_back();
            for (auto&& [wall, end] : std::views::zip(walls, ends)) {
                const WallId wall_id{narrow<std::uint32_t>(map.walls_.size())};
                loop.push_back(wall_id);
                if (wall.id) {
                    walls_by_name.try_emplace(*wall.id, wall_id);
                }
                auto surface = make_surface(materials, wall.surface);
                const auto strip = [&](std::optional<SurfaceDescription>& own) {
                    return own ? make_surface(materials, std::move(*own)) : surface;
                };
                const auto upper = strip(wall.upper);
                const auto lower = strip(wall.lower);
                map.walls_.push_back({
                    .sector = id,
                    .id = std::move(wall.id),
                    .start = wall.start,
                    .end = end,
                    .surface = surface,
                    .upper = upper,
                    .lower = lower,
                    .properties = std::move(wall.properties),
                });
            }
        }

        map.sectors_.push_back({
            .id = std::move(sector.id),
            .floor = floor,
            .ceiling = ceiling,
            .loops = std::move(loops),
            .ambient = sector.ambient.value_or(ambient),
            .properties = std::move(sector.properties),
        });
        map.sectors_by_name_.push_back(id);
    }
    // The checks have made sure that every portal links two walls, each a side of it alone.
    for (auto& portal : description.portals) {
        const PortalId id{narrow<std::uint32_t>(map.portals_.size())};
        const auto a = walls_by_name.at(portal.a.id);
        const auto b = walls_by_name.at(portal.b.id);
        map.wall_at(a).link = PortalLink{.portal = id, .wall = b};
        map.wall_at(b).link = PortalLink{.portal = id, .wall = a};
        map.portals_.push_back({.a = a, .b = b, .properties = std::move(portal.properties)});
    }
    map.materials_ = std::move(materials).take_paths();
    map.backdrops_ = std::move(backdrops).take_paths();
    std::ranges::sort(map.sectors_by_name_, {}, [&map](SectorId sector) -> std::string_view {
        return map.sector(sector).id;
    });
    return map;
}

const Sector& Map::sector(SectorId sector) const {
    WULFHILDE_ASSERT(sector.index() < sectors_.size(), "the sector is one of this map's");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): checked above
    return sectors_[sector.index()];
}

const Wall& Map::wall(WallId wall) const {
    WULFHILDE_ASSERT(wall.index() < walls_.size(), "the wall is one of this map's");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): checked above
    return walls_[wall.index()];
}

const Portal& Map::portal(PortalId portal) const {
    WULFHILDE_ASSERT(portal.index() < portals_.size(), "the portal is one of this map's");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): checked above
    return portals_[portal.index()];
}

Wall& Map::wall_at(WallId wall) {
    WULFHILDE_DEBUG_ASSERT(wall.index() < walls_.size(), "the wall is one of this map's");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): checked above
    return walls_[wall.index()];
}

std::optional<Opening> Map::opening(WallId wall, const Vector2& point) const {
    const auto& near_wall = this->wall(wall);
    if (!near_wall.link) {
        return std::nullopt;
    }
    const auto& near = sector(near_wall.sector);
    const auto& far = sector(this->wall(near_wall.link->wall).sector);
    const auto start = detail::to_plan(near_wall.start);
    const auto end = detail::to_plan(near_wall.end);
    const auto p = detail::to_plan(point);
    const auto run_x = end.x - start.x;
    const auto run_y = end.y - start.y;
    const auto along = std::clamp((((p.x - start.x) * run_x) + ((p.y - start.y) * run_y)) /
                                      ((run_x * run_x) + (run_y * run_y)),
                                  0.0, 1.0);
    const detail::PlanPoint on_wall{.x = start.x + (run_x * along), .y = start.y + (run_y * along)};
    const auto near_floor = plane_of(*this, near.floor).at(on_wall);
    const auto near_ceiling = plane_of(*this, near.ceiling).at(on_wall);
    const auto far_floor = plane_of(*this, far.floor).at(on_wall);
    const auto far_ceiling = plane_of(*this, far.ceiling).at(on_wall);
    // Written with min and max rather than clamp, a sector whose floor is above its ceiling gives
    // an opening that means nothing rather than breaking clamp's precondition.
    const auto bottom = std::min(std::max(near_floor, far_floor), near_ceiling);
    const auto top = std::max(std::min(near_ceiling, far_ceiling), bottom);
    return Opening{.bottom = static_cast<float>(bottom), .top = static_cast<float>(top)};
}

WallFace Map::face(WallId wall) const {
    const auto& near_wall = this->wall(wall);
    const auto start = detail::to_plan(near_wall.start);
    const auto end = detail::to_plan(near_wall.end);
    const auto line_of = [&](const FloorOrCeiling& floor_or_ceiling) {
        const auto plane = plane_of(*this, floor_or_ceiling);
        return FaceLine{.start = plane.at(start), .end = plane.at(end)};
    };
    const auto& near = sector(near_wall.sector);
    const auto floor = line_of(near.floor);
    const auto ceiling = line_of(near.ceiling);

    // A box taller than the wall, cut down to the whole wall and then into its parts.
    const auto low = std::min({floor.start, floor.end, ceiling.start, ceiling.end}) - 1.0;
    const auto high = std::max({floor.start, floor.end, ceiling.start, ceiling.end}) + 1.0;
    const auto whole = clip_face(clip_face(
                                     {
                                         {.along = 0.0, .z = low},
                                         {.along = 0.0, .z = high},
                                         {.along = 1.0, .z = high},
                                         {.along = 1.0, .z = low},
                                     },
                                     floor, true),
                                 ceiling, false);
    WallFace face{.whole = face_corners(whole, start, end)};
    if (!near_wall.link) {
        return face;
    }
    const auto& far = sector(this->wall(near_wall.link->wall).sector);
    const auto far_floor = line_of(far.floor);
    const auto far_ceiling = line_of(far.ceiling);
    const auto above_far_floor = clip_face(whole, far_floor, true);
    face.lower = face_corners(clip_face(whole, far_floor, false), start, end);
    face.opening = face_corners(clip_face(above_far_floor, far_ceiling, false), start, end);
    face.upper = face_corners(clip_face(above_far_floor, far_ceiling, true), start, end);
    return face;
}

bool Map::contains(const Location& location) const {
    const auto& here = sector(location.sector);
    const auto& point = location.point;
    // So written, a height that isn't a number is between neither.
    const auto between = floor_height(location) <= point.z && point.z <= ceiling_height(location);
    if (!between) {
        return false;
    }

    // The winding number of the walls about the point, counting each wall that passes it on its
    // left going up and on its right going down. A point on a wall is in the sector.
    const auto p = detail::to_plan({.x = point.x, .y = point.y});
    auto winding = 0;
    for (const auto& loop : here.loops) {
        for (const auto id : loop) {
            const auto a = detail::to_plan(wall(id).start);
            const auto b = detail::to_plan(wall(id).end);
            const auto side = detail::turn(a, b, p);
            if (side == 0.0 && detail::within(a, b, p)) {
                return true;
            }
            if (a.y <= p.y && p.y < b.y && side > 0.0) {
                ++winding;
            } else if (b.y <= p.y && p.y < a.y && side < 0.0) {
                --winding;
            }
        }
    }
    return winding != 0;
}

float Map::floor_height(const Location& location) const {
    const auto point = detail::to_plan({.x = location.point.x, .y = location.point.y});
    return static_cast<float>(plane_of(*this, sector(location.sector).floor).at(point));
}

float Map::ceiling_height(const Location& location) const {
    const auto point = detail::to_plan({.x = location.point.x, .y = location.point.y});
    return static_cast<float>(plane_of(*this, sector(location.sector).ceiling).at(point));
}

Vector3 Map::floor_normal(SectorId sector) const {
    return normal_of(plane_of(*this, this->sector(sector).floor), 1.0);
}

Vector3 Map::ceiling_normal(SectorId sector) const {
    return normal_of(plane_of(*this, this->sector(sector).ceiling), -1.0);
}

Trace Map::trace(const Location& from, const Vector3& motion) const {
    const auto start = detail::to_plan({.x = from.point.x, .y = from.point.y});
    const TraceLine line{
        .start = start,
        .finish =
            {
                .x = start.x + static_cast<double>(motion.x),
                .y = start.y + static_cast<double>(motion.y),
            },
        .z = static_cast<double>(from.point.z),
        .rise = static_cast<double>(motion.z),
    };
    auto sector_id = from.sector;
    SectorTrace step;
    // A straight line crosses each wall's line once at most, so it crosses each side of each
    // portal once at most.
    for (std::size_t crossings = 0; crossings <= 2 * portals_.size(); ++crossings) {
        step = trace_sector(*this, sector_id, line, step.beyond ? step.at : 0.0);
        if (!step.beyond) {
            break;
        }
        sector_id = *step.beyond;
    }

    const auto along = [reached = step.at](float origin, float distance) {
        return static_cast<float>(static_cast<double>(origin) +
                                  (static_cast<double>(distance) * reached));
    };
    return {
        .end =
            {
                .sector = sector_id,
                .point =
                    {
                        .x = along(from.point.x, motion.x),
                        .y = along(from.point.y, motion.y),
                        .z = along(from.point.z, motion.z),
                    },
            },
        .surface = step.surface,
    };
}

std::optional<SectorId> Map::find_sector(std::string_view id) const noexcept {
    const auto name = [this](SectorId sector) -> std::string_view {
        return this->sector(sector).id;
    };
    const auto found = std::ranges::lower_bound(sectors_by_name_, id, {}, name);
    if (found == sectors_by_name_.end() || name(*found) != id) {
        return std::nullopt;
    }
    return *found;
}

} // namespace wulfhilde
