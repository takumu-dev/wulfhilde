#include "map_decoding.hpp"

#include "file_format.hpp"
#include "json_file.hpp"
#include "object_reader.hpp"
#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/exposure.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <pedantjson/pedantjson.hpp>

#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace wulfhilde::detail {
namespace {

/// What a map's file begins with.
constexpr FileFormat map_format{.name = "wulfhilde-map", .noun = "map", .version = 1};

std::optional<AmbientLight> read_ambient(const Field& field, JsonFile& file) {
    auto ambient = ReadObject{.noun = "ambient light"}(field, file);
    if (!ambient) {
        return std::nullopt;
    }
    const AmbientLight light{
        .color = ambient->required("color", read_color).value_or(Color{}),
        .lux = ambient->required("lux", read_number).value_or(0.0F),
    };
    ambient->finish();
    return light;
}

std::optional<Exposure> read_exposure(const Field& field, JsonFile& file) {
    auto object = ReadObject{.noun = "exposure"}(field, file);
    if (!object) {
        return std::nullopt;
    }
    Exposure exposure;
    exposure.min = object->optional("min", read_number).value_or(exposure.min);
    exposure.max = object->optional("max", read_number).value_or(exposure.max);
    exposure.compensation =
        object->optional("compensation", read_number).value_or(exposure.compensation);
    object->finish();
    return exposure;
}

/// Reads the members that say how a material lies on a surface, which a wall's surface and a floor
/// or a ceiling share, into a description that holds their defaults.
template <typename Description>
void read_placement_members(ObjectReader& object, Description& description) {
    description.offset = object.optional("offset", read_vector2).value_or(description.offset);
    description.scale = object.optional("scale", read_vector2).value_or(description.scale);
    description.rotation = object.optional("rotation", read_angle).value_or(description.rotation);
}

std::optional<SurfaceDescription> read_surface(const Field& field, JsonFile& file) {
    auto surface = ReadObject{.noun = "surface"}(field, file);
    if (!surface) {
        return std::nullopt;
    }
    SurfaceDescription description{
        .material = surface->required("material", read_string).value_or(""),
    };
    read_placement_members(*surface, description);
    surface->finish();
    return description;
}

std::optional<SkyDescription> read_sky(const Field& field, JsonFile& file) {
    auto sky = ReadObject{.noun = "sky"}(field, file);
    if (!sky) {
        return std::nullopt;
    }
    SkyDescription description{
        .backdrop = sky->required("backdrop", read_string).value_or(""),
        .nits = sky->optional("nits", read_number),
    };
    sky->finish();
    return description;
}

std::optional<SlopeDescription> read_slope(const Field& field, JsonFile& file) {
    auto slope = ReadObject{.noun = "slope"}(field, file);
    if (!slope) {
        return std::nullopt;
    }
    SlopeDescription description{
        .hinge = slope->required("hinge", read_string).value_or(""),
        .gradient = slope->required("gradient", read_number).value_or(0.0F),
    };
    slope->finish();
    return description;
}

/// The members of a floor or a ceiling that say which material covers it and how, none of which
/// a sky may have.
constexpr std::array surface_members{"material", "offset", "scale", "rotation"};

/// Reads a floor or a ceiling, whose surface's members, or else its sky, sit beside its height and
/// its slope.
struct ReadFloorOrCeiling {
    /// Which of the two it is, for messages.
    std::string_view noun;

    std::optional<FloorOrCeilingDescription> operator()(const Field& field, JsonFile& file) const {
        auto object = ReadObject{.noun = noun}(field, file);
        if (!object) {
            return std::nullopt;
        }
        FloorOrCeilingDescription description{
            .height = object->required("height", read_number).value_or(0.0F),
            .slope = object->optional("slope", read_slope),
        };
        if (object->has("sky")) {
            description.sky = object->required("sky", read_sky).value_or(SkyDescription{});
            for (const auto* const name : surface_members) {
                std::ignore = object->optional(
                    name, ReportConflict{.noun = noun, .other = "sky", .name = name});
            }
        } else {
            if (object->has("material")) {
                description.material = object->required("material", read_string).value_or("");
            } else {
                object->report(AssetProblemCode::missing_member,
                               std::format(R"(the {} has no "material" or "sky", which it must )"
                                           "have one of",
                                           noun));
            }
            read_placement_members(*object, description);
        }
        object->finish();
        return description;
    }
};

std::optional<WallDescription> read_wall(const Field& field, JsonFile& file) {
    auto wall = ReadObject{.noun = "wall"}(field, file);
    if (!wall) {
        return std::nullopt;
    }
    WallDescription description{
        .id = wall->optional("id", read_string),
        .start = wall->required("start", read_vector2).value_or(Vector2{}),
        .surface = wall->required("surface", read_surface).value_or(SurfaceDescription{}),
        .upper = wall->optional("upper", read_surface),
        .lower = wall->optional("lower", read_surface),
        .properties = wall->optional("properties", read_properties).value_or(""),
    };
    wall->finish();
    return description;
}

std::optional<std::vector<WallDescription>> read_loop(const Field& field, JsonFile& file) {
    const auto elements = read_array(field, file);
    if (!elements) {
        return std::nullopt;
    }
    std::vector<WallDescription> walls;
    for (const auto& element : *elements) {
        walls.push_back(read_wall(element, file).value_or(WallDescription{}));
    }
    return walls;
}

std::optional<SectorDescription> read_sector(const Field& field, JsonFile& file) {
    auto sector = ReadObject{.noun = "sector"}(field, file);
    if (!sector) {
        return std::nullopt;
    }
    SectorDescription description{
        .id = sector->required("id", read_string).value_or(""),
        .floor = sector->required("floor", ReadFloorOrCeiling{.noun = "floor"})
                     .value_or(FloorOrCeilingDescription{}),
        .ceiling = sector->required("ceiling", ReadFloorOrCeiling{.noun = "ceiling"})
                       .value_or(FloorOrCeilingDescription{}),
        .ambient = sector->optional("ambient", read_ambient),
    };
    if (const auto loops = sector->required("loops", read_array)) {
        for (const auto& loop : *loops) {
            description.loops.push_back(
                read_loop(loop, file).value_or(std::vector<WallDescription>{}));
        }
    }
    description.properties = sector->optional("properties", read_properties).value_or("");
    sector->finish();
    return description;
}

/// Each kind of portal side, with the name of the member that names it.
constexpr std::array portal_side_kinds{
    std::pair{PortalSideKind::wall, std::string_view{"wall"}},
    std::pair{PortalSideKind::floor, std::string_view{"floor"}},
    std::pair{PortalSideKind::ceiling, std::string_view{"ceiling"}},
};

/// Reads a side of a portal, an object whose one member names a wall, or a sector for its floor
/// or ceiling.
std::optional<PortalSideDescription> read_portal_side(const Field& field, JsonFile& file) {
    constexpr std::string_view noun = "portal side";
    auto side = ReadObject{.noun = noun}(field, file);
    if (!side) {
        return std::nullopt;
    }
    std::optional<PortalSideDescription> description;
    // The member that names the side, of those that may.
    std::string_view named;
    for (const auto& [kind, name] : portal_side_kinds) {
        if (!side->has(name)) {
            continue;
        }
        if (description) {
            std::ignore =
                side->optional(name, ReportConflict{.noun = noun, .other = named, .name = name});
        } else {
            description = PortalSideDescription{
                .kind = kind,
                .id = side->required(name, read_string).value_or(""),
            };
            named = name;
        }
    }
    if (!description) {
        side->report(AssetProblemCode::missing_member,
                     R"(the portal side has no "wall", "floor" or "ceiling", which it must have )"
                     "one of");
    }
    side->finish();
    return description.value_or(PortalSideDescription{});
}

std::optional<PortalDescription> read_portal(const Field& field, JsonFile& file) {
    auto portal = ReadObject{.noun = "portal"}(field, file);
    if (!portal) {
        return std::nullopt;
    }
    PortalDescription description{
        .a = portal->required("a", read_portal_side).value_or(PortalSideDescription{}),
        .b = portal->required("b", read_portal_side).value_or(PortalSideDescription{}),
        .properties = portal->optional("properties", read_properties).value_or(""),
    };
    portal->finish();
    return description;
}

std::optional<StartPositionDescription> read_start(const Field& field, JsonFile& file) {
    auto start = ReadObject{.noun = "start position"}(field, file);
    if (!start) {
        return std::nullopt;
    }
    StartPositionDescription description{
        .sector = start->required("sector", read_string).value_or(""),
        .point = start->required("point", read_vector3).value_or(Vector3{}),
        .yaw = start->required("yaw", read_angle).value_or(Angle{}),
        .pitch = start->optional("pitch", read_angle).value_or(Angle{}),
    };
    start->finish();
    return description;
}

} // namespace

std::optional<MapDescription> decode_map(JsonFile& file) {
    const auto root = file.document().root();
    if (!check_format_and_version(root, map_format, file)) {
        return std::nullopt;
    }

    ObjectReader map(root, pedantjson::Pointer{}, "map", file);
    // Both are checked already. The schema an editor checks the file against is the editor's
    // business.
    std::ignore = map.required("format", read_string);
    std::ignore = map.required("version", read_number);
    std::ignore = map.optional("$schema", read_string);

    MapDescription description{
        .ambient = map.optional("ambient", read_ambient),
        .exposure = map.optional("exposure", read_exposure).value_or(Exposure{}),
    };
    if (const auto sectors = map.required("sectors", read_array)) {
        for (const auto& sector : *sectors) {
            description.sectors.push_back(read_sector(sector, file).value_or(SectorDescription{}));
        }
    }
    if (const auto portals = map.optional("portals", read_array)) {
        for (const auto& portal : *portals) {
            description.portals.push_back(read_portal(portal, file).value_or(PortalDescription{}));
        }
    }
    description.start = map.required("start", read_start).value_or(StartPositionDescription{});
    description.properties = map.optional("properties", read_properties).value_or("");
    map.finish();
    return description;
}

} // namespace wulfhilde::detail
