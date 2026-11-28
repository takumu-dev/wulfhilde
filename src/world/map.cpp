#include <wulfhilde/world/map.hpp>

#include "height_plane.hpp"
#include "map_checks.hpp"
#include "overlap.hpp"
#include "plan_geometry.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
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
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wulfhilde {
namespace {

/// How many transformed portals a trace crosses at most, beyond one crossing of each side of each
/// portal: enough for any trace a map's bounds allow through portals a metre apart.
constexpr std::size_t most_transformed_crossings = 16384;

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
        .nits = description.nits,
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
                                  .nits = description.nits,
                              }),
    };
}

/// Returns a floor or ceiling of a map as heights over the plan.
[[nodiscard]] detail::HeightPlane plane_of(const Map& map, const FloorOrCeiling& floor_or_ceiling) {
    return detail::plane_of(floor_or_ceiling, floor_or_ceiling.slope
                                                  ? &map.wall(floor_or_ceiling.slope->hinge)
                                                  : nullptr);
}

/// Returns the direction a floor or ceiling faces, of unit length: up for a floor and down for a
/// ceiling, tilted by its slope.
[[nodiscard]] Vector3 normal_of(const detail::HeightPlane& plane, double up) {
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

/// Tells whether a corner of a polygon lies on the line through the corners either side of it, or
/// on one of them, as the floats it is given in.
[[nodiscard]] bool on_line_between(const Vector3& before, const Vector3& corner,
                                   const Vector3& after) {
    const auto to_corner = corner - before;
    const auto to_after = after - before;
    const auto x = (static_cast<double>(to_corner.y) * static_cast<double>(to_after.z)) -
                   (static_cast<double>(to_corner.z) * static_cast<double>(to_after.y));
    const auto y = (static_cast<double>(to_corner.z) * static_cast<double>(to_after.x)) -
                   (static_cast<double>(to_corner.x) * static_cast<double>(to_after.z));
    const auto z = (static_cast<double>(to_corner.x) * static_cast<double>(to_after.y)) -
                   (static_cast<double>(to_corner.y) * static_cast<double>(to_after.x));
    return x == 0.0 && y == 0.0 && z == 0.0;
}

/// Returns a polygon of a wall's plane as corners in its sector's coordinates, or none if it has
/// no area. A corner that single precision puts on the line between its neighbours is left out,
/// so every triangle of a fan from any corner has area, and a polygon too thin to keep three
/// corners has none.
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
    for (std::size_t i = 0; corners.size() >= 3 && i < corners.size();) {
        const auto& before = corners.at((i + corners.size() - 1) % corners.size());
        const auto& after = corners.at((i + 1) % corners.size());
        if (on_line_between(before, corners.at(i), after)) {
            corners.erase(corners.begin() + static_cast<std::ptrdiff_t>(i));
            i = 0;
        } else {
            ++i;
        }
    }
    if (corners.size() < 3) {
        return {};
    }
    return corners;
}

/// A point in three dimensions, in double.
struct PrecisePoint {
    double x = 0.0; ///< Its X.
    double y = 0.0; ///< Its Y.
    double z = 0.0; ///< Its height.
};

/// A portal transform as a matrix applied in double, to carry points through the portal.
struct PreciseTransform {
    /// The transform's matrix.
    Matrix4 matrix;

    /// Returns where the transform takes a point.
    [[nodiscard]] PrecisePoint apply(const PrecisePoint& p) const noexcept {
        const auto& [x_axis, y_axis, z_axis, moved] = matrix.columns;
        const auto row = [&p](float x, float y, float z, float w) {
            return (static_cast<double>(x) * p.x) + (static_cast<double>(y) * p.y) +
                   (static_cast<double>(z) * p.z) + static_cast<double>(w);
        };
        return {
            .x = row(x_axis.x, y_axis.x, z_axis.x, moved.x),
            .y = row(x_axis.y, y_axis.y, z_axis.y, moved.y),
            .z = row(x_axis.z, y_axis.z, z_axis.z, moved.z),
        };
    }
};

/// The span from the floor to the ceiling of the sector beyond a side of a portal, carried back
/// into the side's own sector's coordinates, at a point of the side's wall: from its bottom to its
/// top there.
struct FarSpan {
    double bottom = 0.0; ///< The height of its bottom.
    double top = 0.0;    ///< The height of its top.
};

/// Returns the span of the sector beyond a side of a portal at a point of the side's wall, given
/// the side's link.
///
/// The checks have made sure that a portal's transform keeps the wall's corners upright, so each
/// point of the wall lands on one upright line beyond, and the far floor and ceiling there come
/// back as heights over the point, turned over if the transform turns the wall upside down. An
/// ordinary portal's transform is the identity, which carries every height back as it is.
[[nodiscard]] FarSpan far_span_at(const Map& map, const PortalLink& link,
                                  const detail::PlanPoint& on_wall) {
    const auto& far = map.sector(map.wall(link.wall).sector);
    const auto beyond =
        PreciseTransform{to_matrix(link.transform)}.apply({.x = on_wall.x, .y = on_wall.y});
    const detail::PlanPoint far_point{.x = beyond.x, .y = beyond.y};
    const PreciseTransform back{to_matrix(inverse(link.transform))};
    const auto height_back = [&](const FloorOrCeiling& floor_or_ceiling) {
        return back
            .apply({
                .x = far_point.x,
                .y = far_point.y,
                .z = plane_of(map, floor_or_ceiling).at(far_point),
            })
            .z;
    };
    const auto floor = height_back(far.floor);
    const auto ceiling = height_back(far.ceiling);
    return {.bottom = std::min(floor, ceiling), .top = std::max(floor, ceiling)};
}

/// The straight line a trace runs along, in double, measured from 0 at its start to 1 at its
/// finish, in the coordinates of the sector it is in. Ordinary portals keep every sector in the
/// same coordinates, so it is one line through them, and each wall's line is met at the same point
/// of it from whichever sector. A transformed portal carries it into the coordinates beyond,
/// keeping how far along it each point is.
struct TraceLine {
    detail::PlanPoint start{};  ///< Where it starts, seen from above.
    detail::PlanPoint finish{}; ///< Where it finishes, seen from above.
    double z = 0.0;             ///< The height it starts at.
    double rise = 0.0;          ///< How far it rises from its start to its finish.
};

/// Returns a trace's line carried through a portal transform.
[[nodiscard]] TraceLine carried(const TraceLine& line, const RigidTransform& transform) {
    const PreciseTransform onto{to_matrix(transform)};
    const auto start = onto.apply({.x = line.start.x, .y = line.start.y, .z = line.z});
    const auto finish =
        onto.apply({.x = line.finish.x, .y = line.finish.y, .z = line.z + line.rise});
    return {
        .start = {.x = start.x, .y = start.y},
        .finish = {.x = finish.x, .y = finish.y},
        .z = start.z,
        .rise = finish.z - start.z,
    };
}

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

/// Returns the way through a wall that a trace reaching it at a point of its line takes: the link
/// of a side of a portal, if the point is within its opening, its edges included.
[[nodiscard]] std::optional<PortalLink> beyond_opening(const Map& map, WallId id,
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
    return link;
}

/// Where a trace leaves one sector: at the surface that stops it, through an opening, or at the
/// finish of its line.
struct SectorTrace {
    /// How far along the line, from 0 to 1.
    double at = 1.0;

    /// The surface that stops the trace there, if one does.
    std::optional<SurfaceRef> surface{};

    /// The way through the opening the trace crosses there, if it crosses one.
    std::optional<PortalLink> beyond{};
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
        const auto transform =
            portal.transform.transform([](const PortalTransformDescription& written) {
                const auto& [yaw, pitch, roll] = written.rotation;
                return RigidTransform{
                    .rotation = Rotation::from_yaw_pitch_roll(yaw, pitch, roll),
                    .translation = written.translation,
                };
            });
        const auto onto_b = transform.value_or(RigidTransform::identity());
        map.wall_at(a).link = PortalLink{.portal = id, .wall = b, .transform = onto_b};
        map.wall_at(b).link = PortalLink{.portal = id, .wall = a, .transform = inverse(onto_b)};
        map.portals_.push_back({
            .a = a,
            .b = b,
            .transform = transform,
            .properties = std::move(portal.properties),
        });
    }
    map.overlapping_ = detail::find_overlaps(map.sectors_, map.walls_);
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

std::span<const SectorId> Map::overlapping(SectorId sector) const {
    WULFHILDE_ASSERT(sector.index() < overlapping_.size(), "the sector is one of this map's");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): checked above
    return overlapping_[sector.index()];
}

std::optional<Opening> Map::opening(WallId wall, const Vector2& point) const {
    const auto& near_wall = this->wall(wall);
    if (!near_wall.link) {
        return std::nullopt;
    }
    const auto& near = sector(near_wall.sector);
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
    const auto far = far_span_at(*this, *near_wall.link, on_wall);
    // Written with min and max rather than clamp, a sector whose floor is above its ceiling gives
    // an opening that means nothing rather than breaking clamp's precondition.
    const auto bottom = std::min(std::max(near_floor, far.bottom), near_ceiling);
    const auto top = std::max(std::min(near_ceiling, far.top), bottom);
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
    const auto far_at_start = far_span_at(*this, *near_wall.link, start);
    const auto far_at_end = far_span_at(*this, *near_wall.link, end);
    const FaceLine far_floor{.start = far_at_start.bottom, .end = far_at_end.bottom};
    const FaceLine far_ceiling{.start = far_at_start.top, .end = far_at_end.top};
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
    TraceLine line{
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
    auto transform = RigidTransform::identity();
    SectorTrace step;
    // Through ordinary portals, a straight line crosses each wall's line once at most, so it
    // crosses each side of each portal once at most. Transformed portals may bring it back to
    // the same portal, as two facing each other do, so it crosses as many as its length takes it
    // through, up to a bound that keeps a trace along the edge of such a portal from looping.
    for (std::size_t crossings = 0; crossings <= (2 * portals_.size()) + most_transformed_crossings;
         ++crossings) {
        step = trace_sector(*this, sector_id, line, step.beyond ? step.at : 0.0);
        if (!step.beyond) {
            break;
        }
        sector_id = wall(step.beyond->wall).sector;
        if (step.beyond->transform != RigidTransform::identity()) {
            line = carried(line, step.beyond->transform);
            transform = step.beyond->transform * transform;
        }
    }

    const auto along = [reached = step.at](double origin, double distance) {
        return static_cast<float>(origin + (distance * reached));
    };
    return {
        .end =
            {
                .sector = sector_id,
                .point =
                    {
                        .x = along(line.start.x, line.finish.x - line.start.x),
                        .y = along(line.start.y, line.finish.y - line.start.y),
                        .z = along(line.z, line.rise),
                    },
            },
        .transform = transform,
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
