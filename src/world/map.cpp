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
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/surface.hpp>
#include <wulfhilde/world/surface_ref.hpp>
#include <wulfhilde/world/trace.hpp>
#include <wulfhilde/world/wall.hpp>

#include <algorithm>
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

/// Gives each distinct material path an ID, in the order they are first seen.
class MaterialInterner {
  public:
    /// Returns a path's ID, giving it the next one if it has none yet.
    MaterialId intern(std::string path) {
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
    std::unordered_map<std::string, MaterialId> ids_;
    std::vector<std::string> paths_;
};

/// Returns a surface as a map holds it.
[[nodiscard]] Surface make_surface(MaterialInterner& materials, SurfaceDescription description) {
    return {
        .material = materials.intern(std::move(description.material)),
        .offset = description.offset,
        .scale = description.scale,
        .rotation = description.rotation,
    };
}

/// Returns a floor or ceiling as a map holds it.
[[nodiscard]] FloorOrCeiling make_floor_or_ceiling(MaterialInterner& materials,
                                                   FloorOrCeilingDescription description) {
    return {
        .height = description.height,
        .surface = make_surface(materials,
                                {
                                    .material = std::move(description.material),
                                    .offset = description.offset,
                                    .scale = description.scale,
                                    .rotation = description.rotation,
                                }),
    };
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
    map.properties_ = std::move(description.properties);
    const auto ambient = description.ambient.value_or(AmbientLight{});
    MaterialInterner materials;
    for (auto& sector : description.sectors) {
        const SectorId id{narrow<std::uint32_t>(map.sectors_.size())};
        const auto floor = make_floor_or_ceiling(materials, std::move(sector.floor));
        const auto ceiling = make_floor_or_ceiling(materials, std::move(sector.ceiling));

        std::vector<std::vector<WallId>> loops;
        for (auto& walls : sector.loops) {
            const auto starts = walls | std::views::transform(&WallDescription::start) |
                                std::ranges::to<std::vector>();
            const auto ends = detail::loop_ends(starts);

            auto& loop = loops.emplace_back();
            for (auto&& [wall, end] : std::views::zip(walls, ends)) {
                loop.emplace_back(narrow<std::uint32_t>(map.walls_.size()));
                map.walls_.push_back({
                    .sector = id,
                    .start = wall.start,
                    .end = end,
                    .surface = make_surface(materials, std::move(wall.surface)),
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
    map.materials_ = std::move(materials).take_paths();
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
    const auto& here = sector(from.sector);

    // How far along the motion the trace stops, from 0 to 1, and what stops it there. Of two
    // surfaces met at once, the first found stops it.
    auto reached = 1.0;
    std::optional<SurfaceRef> surface;
    const auto meet = [&](double at, const SurfaceRef& met) {
        if (surface ? at < reached : at <= reached) {
            reached = at;
            surface = met;
        }
    };

    const auto z = static_cast<double>(from.point.z);
    const auto rise = static_cast<double>(motion.z);
    if (rise < 0.0 && z >= static_cast<double>(here.floor.height)) {
        meet((static_cast<double>(here.floor.height) - z) / rise, SectorFloor{from.sector});
    }
    if (rise > 0.0 && z <= static_cast<double>(here.ceiling.height)) {
        meet((static_cast<double>(here.ceiling.height) - z) / rise, SectorCeiling{from.sector});
    }

    // A wall stops the trace where it reaches the wall's line from the sector's side, between the
    // wall's ends. The ends are judged by the side of the trace's line they lie on, so
    // two walls that share a corner can't both miss a trace through it.
    const auto start = detail::to_plan({.x = from.point.x, .y = from.point.y});
    const detail::PlanPoint finish{
        .x = start.x + static_cast<double>(motion.x),
        .y = start.y + static_cast<double>(motion.y),
    };
    for (const auto& loop : here.loops) {
        for (const auto id : loop) {
            const auto a = detail::to_plan(wall(id).start);
            const auto b = detail::to_plan(wall(id).end);
            const auto start_side = detail::turn(a, b, start);
            const auto finish_side = detail::turn(a, b, finish);
            // A trace may start or end on the wall's line, but not run along it.
            if (start_side < 0.0 || finish_side > 0.0 ||
                (start_side == 0.0 && finish_side == 0.0)) {
                continue;
            }
            const auto a_side = detail::turn(start, finish, a);
            const auto b_side = detail::turn(start, finish, b);
            if ((a_side >= 0.0 && b_side <= 0.0) || (a_side <= 0.0 && b_side >= 0.0)) {
                meet(start_side / (start_side - finish_side), id);
            }
        }
    }

    const auto along = [reached](float origin, float distance) {
        return static_cast<float>(static_cast<double>(origin) +
                                  (static_cast<double>(distance) * reached));
    };
    return {
        .end =
            {
                .sector = from.sector,
                .point =
                    {
                        .x = along(from.point.x, motion.x),
                        .y = along(from.point.y, motion.y),
                        .z = along(from.point.z, motion.z),
                    },
            },
        .surface = surface,
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
