#include "map_decoding.hpp"

#include "json_file.hpp"
#include "object_reader.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <pedantjson/pedantjson.hpp>

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace wulfhilde::detail {
namespace {

/// The `format` of a map's file.
constexpr std::string_view map_format = "wulfhilde-map";

/// The one `version` of a map's file that this version of the engine reads. Before 1.0, any change
/// to the format bumps it, and older maps aren't read.
constexpr int map_version = 1;

/// Checks that a file's outermost value is a map of this version, first the format and then the
/// version, reporting only the first of them that isn't.
///
/// \return Whether it is.
bool check_format_and_version(const pedantjson::Value& root, JsonFile& file) {
    const pedantjson::Pointer pointer;
    const auto object = root.as_object();
    if (!object) {
        file.report(MapProblemCode::wrong_type, pointer, root.offset(),
                    std::format("expected an object, not {}", kind_name(root.kind())));
        return false;
    }

    const auto format = object->find("format");
    if (!format) {
        file.report(MapProblemCode::unknown_format, pointer, root.offset(),
                    std::format(R"(the file isn't a wulfhilde map: it has no "format", which a )"
                                "map gives as {:?}",
                                map_format));
        return false;
    }
    const auto name = format->value.as_string();
    if (name != map_format) {
        file.report(MapProblemCode::unknown_format, pointer / "format", format->value.offset(),
                    std::format(R"(the file isn't a wulfhilde map: its "format" is {}, not {:?})",
                                name ? std::format("{:?}", *name)
                                     : std::string{kind_name(format->value.kind())},
                                map_format));
        return false;
    }

    const auto version = object->find("version");
    if (!version) {
        file.report(MapProblemCode::unsupported_version, pointer, root.offset(),
                    std::format(R"(the map has no "version", but this version of the engine )"
                                "reads maps of version {} only",
                                map_version));
        return false;
    }
    // A number written with a zero fraction, such as 1.0, is an integer.
    const auto number = version->value.as_number();
    if (!number || number->to<int>() != map_version) {
        file.report(
            MapProblemCode::unsupported_version, pointer / "version", version->value.offset(),
            std::format("this version of the engine reads maps of version {} only, not {}",
                        map_version, number ? number->text() : kind_name(version->value.kind())));
        return false;
    }
    return true;
}

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

/// Reads the members that say which material covers a surface and how it lies there, which a wall's
/// surface and a floor or a ceiling share, into a description that holds its defaults.
template <typename Description>
void read_surface_members(ObjectReader& object, Description& description) {
    description.material = object.required("material", read_string).value_or("");
    description.offset = object.optional("offset", read_vector2).value_or(description.offset);
    description.scale = object.optional("scale", read_vector2).value_or(description.scale);
    description.rotation = object.optional("rotation", read_angle).value_or(description.rotation);
}

std::optional<SurfaceDescription> read_surface(const Field& field, JsonFile& file) {
    auto surface = ReadObject{.noun = "surface"}(field, file);
    if (!surface) {
        return std::nullopt;
    }
    SurfaceDescription description;
    read_surface_members(*surface, description);
    surface->finish();
    return description;
}

/// Reads a floor or a ceiling, whose surface's members sit beside its height.
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
        };
        read_surface_members(*object, description);
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
        .start = wall->required("start", read_vector2).value_or(Vector2{}),
        .surface = wall->required("surface", read_surface).value_or(SurfaceDescription{}),
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
    if (!check_format_and_version(root, file)) {
        return std::nullopt;
    }

    ObjectReader map(root, pedantjson::Pointer{}, "map", file);
    // Both are checked already. The schema an editor checks the file against is the editor's
    // business.
    std::ignore = map.required("format", read_string);
    std::ignore = map.required("version", read_number);
    std::ignore = map.optional("$schema", read_string);

    MapDescription description{.ambient = map.optional("ambient", read_ambient)};
    if (const auto sectors = map.required("sectors", read_array)) {
        for (const auto& sector : *sectors) {
            description.sectors.push_back(read_sector(sector, file).value_or(SectorDescription{}));
        }
    }
    description.start = map.required("start", read_start).value_or(StartPositionDescription{});
    description.properties = map.optional("properties", read_properties).value_or("");
    map.finish();
    return description;
}

} // namespace wulfhilde::detail
