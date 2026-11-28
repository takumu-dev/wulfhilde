#include <wulfhilde/world/map.hpp>

#include "map_checks.hpp"
#include "plan_geometry.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
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
        .nits = description.nits,
    };
}

/// Returns a floor or ceiling as a map holds it.
[[nodiscard]] FloorOrCeiling make_floor_or_ceiling(MaterialInterner& materials,
                                                   BackdropInterner& backdrops,
                                                   FloorOrCeilingDescription description) {
    if (description.sky) {
        return {
            .height = description.height,
            .cover =
                Sky{
                    .backdrop = backdrops.intern(std::move(description.sky->backdrop)),
                    .nits = description.sky->nits,
                },
        };
    }
    return {
        .height = description.height,
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

/// Returns the sector beyond a wall that a trace reaching it at a height crosses into: the far
/// sector of a side of a portal, if the height is within its opening, its edges included.
[[nodiscard]] std::optional<SectorId> beyond_opening(const Map& map, WallId id, double height) {
    const auto& link = map.wall(id).link;
    const auto opening = map.opening(id);
    if (!link || !opening || height < static_cast<double>(opening->bottom) ||
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
    const auto floor = static_cast<double>(here.floor.height);
    const auto ceiling = static_cast<double>(here.ceiling.height);
    const auto z_entered = line.z + (line.rise * entered);
    if (line.rise < 0.0 && z_entered >= floor) {
        meet((floor - line.z) / line.rise, SectorFloor{id});
    }
    if (line.rise > 0.0 && z_entered <= ceiling) {
        meet((ceiling - line.z) / line.rise, SectorCeiling{id});
    }

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
            const auto beyond = beyond_opening(map, wall_id, line.z + (line.rise * *at));
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
        const auto floor = make_floor_or_ceiling(materials, backdrops, std::move(sector.floor));
        const auto ceiling = make_floor_or_ceiling(materials, backdrops, std::move(sector.ceiling));

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

std::optional<Opening> Map::opening(WallId wall) const {
    const auto& near_wall = this->wall(wall);
    if (!near_wall.link) {
        return std::nullopt;
    }
    const auto& near = sector(near_wall.sector);
    const auto& far = sector(this->wall(near_wall.link->wall).sector);
    // Written with min and max rather than clamp, a sector whose floor is above its ceiling gives
    // an opening that means nothing rather than breaking clamp's precondition.
    const auto bottom =
        std::min(std::max(near.floor.height, far.floor.height), near.ceiling.height);
    const auto top = std::max(std::min(near.ceiling.height, far.ceiling.height), bottom);
    return Opening{.bottom = bottom, .top = top};
}

bool Map::contains(const Location& location) const {
    const auto& here = sector(location.sector);
    const auto& point = location.point;
    // So written, a height that isn't a number is between neither.
    const auto between = here.floor.height <= point.z && point.z <= here.ceiling.height;
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
    return sector(location.sector).floor.height;
}

float Map::ceiling_height(const Location& location) const {
    return sector(location.sector).ceiling.height;
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
