#include "material_decoding.hpp"

#include "file_format.hpp"
#include "json_file.hpp"
#include "object_reader.hpp"
#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/asset_path.hpp>

#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace wulfhilde::detail {
namespace {

/// What a material's file begins with.
constexpr FileFormat material_format{
    .name = "wulfhilde-material",
    .noun = "material",
    .version = 1,
};

/// The properties of a material that may read a texture, besides a normal, which must.
constexpr std::array textured_properties{"color", "roughness", "metallic", "occlusion", "emission"};

/// The coolest and hottest temperatures an emission may have, in kelvin.
constexpr float coolest_temperature = 1'000.0F;
constexpr float hottest_temperature = 40'000.0F;

/// Returns a number's text, as the file writes it.
std::string_view number_text(const Field& field) {
    return field.value.as_number().transform(&pedantjson::Number::text).value_or("");
}

/// Reads a number that must lie in a range, reporting one outside it at the number.
///
/// \param in_range Whether a value lies in the range.
/// \param code The kind of problem a value outside it is.
/// \param describe What is wrong with a value outside it, given its text.
template <typename InRange, typename Describe>
std::optional<float> read_number_in(const Field& field, JsonFile& file, InRange in_range,
                                    AssetProblemCode code, Describe describe) {
    const auto value = read_number(field, file);
    if (value && !in_range(*value)) {
        report_at(field, code, describe(number_text(field)), file);
        return std::nullopt;
    }
    return value;
}

/// Reads a number from 0 to 1.
struct ReadFraction {
    /// What the number is, for messages, such as `factor`.
    std::string_view noun;

    std::optional<float> operator()(const Field& field, JsonFile& file) const {
        return read_number_in(
            field, file, [](float value) { return value >= 0.0F && value <= 1.0F; },
            AssetProblemCode::value_out_of_range,
            [this](std::string_view text) {
                return std::format("{} is outside a {}'s range, 0 to 1", text, noun);
            });
    }
};

/// Reads a component of a colour, from 0 to 1.
std::optional<float> read_color_component(const Field& field, JsonFile& file) {
    return read_number_in(
        field, file, [](float value) { return value >= 0.0F && value <= 1.0F; },
        AssetProblemCode::color_out_of_range,
        [](std::string_view text) {
            return std::format("{} is outside a colour component's range, 0 to 1", text);
        });
}

/// Reads one of a size's two lengths, which is more than 0.
std::optional<float> read_length(const Field& field, JsonFile& file) {
    return read_number_in(
        field, file, [](float value) { return value > 0.0F; }, AssetProblemCode::value_out_of_range,
        [](std::string_view text) {
            return std::format("{} isn't a size, which is more than 0", text);
        });
}

/// Reads a light level in nits, which is 0 or more.
std::optional<float> read_nits(const Field& field, JsonFile& file) {
    return read_number_in(
        field, file, [](float value) { return value >= 0.0F; },
        AssetProblemCode::light_level_out_of_range,
        [](std::string_view text) {
            return std::format("{} nits isn't a light level, which is 0 or more", text);
        });
}

/// Reads a temperature in kelvin, from the coolest to the hottest an emission may have.
std::optional<float> read_temperature(const Field& field, JsonFile& file) {
    return read_number_in(
        field, file,
        [](float value) { return value >= coolest_temperature && value <= hottest_temperature; },
        AssetProblemCode::value_out_of_range,
        [](std::string_view text) {
            return std::format("{} K is outside a temperature's range, 1,000 to 40,000 K", text);
        });
}

/// Reads a colour, three numbers `[r, g, b]` from 0 to 1.
std::optional<Color> read_unit_color(const Field& field, JsonFile& file) {
    return read_numbers<3>(field, file, read_color_component).transform([](const auto& numbers) {
        const auto [r, g, b] = numbers;
        return Color{.r = r, .g = g, .b = b};
    });
}

/// Reads a size, two lengths `[u, v]` of more than 0.
std::optional<Vector2> read_size(const Field& field, JsonFile& file) {
    return read_numbers<2>(field, file, read_length).transform([](const auto& numbers) {
        const auto [u, v] = numbers;
        return Vector2{.x = u, .y = v};
    });
}

/// Reads a texture's asset path, reporting one that breaks the syntax of an asset path.
std::optional<TextureReference> read_texture(const Field& field, JsonFile& file) {
    auto path = read_string(field, file);
    if (!path) {
        return std::nullopt;
    }
    // A path that isn't an asset path may lead out of the content root.
    if (!is_asset_path(*path)) {
        report_at(field, AssetProblemCode::invalid_asset_path,
                  std::format(R"("{}" isn't an asset path, which is segments of a-z, 0-9, _ and - )"
                              "separated by / and without an extension",
                              *path),
                  file);
        return std::nullopt;
    }
    return TextureReference{.path = std::move(*path), .field = field};
}

/// Reads the channel of a texture that a single-value property reads.
std::optional<ChannelReference> read_channel(const Field& field, JsonFile& file) {
    const auto name = read_string(field, file);
    if (!name) {
        return std::nullopt;
    }
    constexpr std::array channels{
        std::pair{std::string_view{"r"}, Channel::r},
        std::pair{std::string_view{"g"}, Channel::g},
        std::pair{std::string_view{"b"}, Channel::b},
        std::pair{std::string_view{"a"}, Channel::a},
    };
    for (const auto& [channel_name, channel] : channels) {
        if (*name == channel_name) {
            return ChannelReference{.channel = channel, .field = field};
        }
    }
    report_at(field, AssetProblemCode::unknown_channel,
              std::format(R"(expected "r", "g", "b" or "a", not {:?})", *name), file);
    return std::nullopt;
}

/// Reports a member that applies only to a texture, where there is none.
struct ReportWithoutTexture {
    /// What the object that has the member is, for messages, such as `roughness`.
    std::string_view noun;

    /// The member's name.
    std::string_view name;

    std::optional<std::monostate> operator()(const Field& field, JsonFile& file) const {
        report_at(field, AssetProblemCode::member_without_texture,
                  std::format("the {} has {:?}, but no texture for it to apply to", noun, name),
                  file);
        return std::nullopt;
    }
};

/// Reads a factor that multiplies a texture, which defaults to `fallback` with a texture and must
/// be written without one.
template <typename Factor, typename ReadFactor>
Factor read_factor(ObjectReader& object, std::string_view noun, bool textured,
                   ReadFactor read_value, Factor fallback) {
    if (textured) {
        return object.optional("factor", read_value).value_or(fallback);
    }
    if (!object.has("factor")) {
        object.report(AssetProblemCode::missing_member,
                      std::format(R"(the {} has no "factor" or "texture", which it must have one )"
                                  "of",
                                  noun));
        return fallback;
    }
    return object.required("factor", read_value).value_or(fallback);
}

/// Reads the members of a single-value property, which an occlusion shares.
ValueDescription read_value_members(ObjectReader& object, std::string_view noun) {
    const auto textured = object.has("texture");
    ValueDescription description{
        .factor = read_factor(object, noun, textured, ReadFraction{.noun = "factor"}, 1.0F),
        .texture = object.optional("texture", read_texture),
    };
    if (textured) {
        description.channel = object.optional("channel", read_channel);
    } else {
        std::ignore =
            object.optional("channel", ReportWithoutTexture{.noun = noun, .name = "channel"});
    }
    return description;
}

std::optional<ColorDescription> read_color_property(const Field& field, JsonFile& file) {
    constexpr std::string_view noun = "colour";
    auto object = ReadObject{.noun = noun}(field, file);
    if (!object) {
        return std::nullopt;
    }
    constexpr Color white{.r = 1.0F, .g = 1.0F, .b = 1.0F};
    ColorDescription description{
        .factor = read_factor(*object, noun, object->has("texture"), read_unit_color, white),
        .texture = object->optional("texture", read_texture),
    };
    object->finish();
    return description;
}

/// Reads a single-value property, such as roughness.
struct ReadValueProperty {
    /// What the property is, for messages, such as `roughness`.
    std::string_view noun;

    std::optional<ValueDescription> operator()(const Field& field, JsonFile& file) const {
        auto object = ReadObject{.noun = noun}(field, file);
        if (!object) {
            return std::nullopt;
        }
        auto description = read_value_members(*object, noun);
        object->finish();
        return description;
    }
};

std::optional<NormalDescription> read_normal(const Field& field, JsonFile& file) {
    auto object = ReadObject{.noun = "normal"}(field, file);
    if (!object) {
        return std::nullopt;
    }
    auto texture = object->required("texture", read_texture);
    const auto scale = object->optional("scale", read_number).value_or(1.0F);
    object->finish();
    if (!texture) {
        return std::nullopt;
    }
    return NormalDescription{.texture = std::move(*texture), .scale = scale};
}

std::optional<OcclusionDescription> read_occlusion(const Field& field, JsonFile& file) {
    constexpr std::string_view noun = "occlusion";
    auto object = ReadObject{.noun = noun}(field, file);
    if (!object) {
        return std::nullopt;
    }
    OcclusionDescription description{
        .value = read_value_members(*object, noun),
        .strength = object->optional("strength", ReadFraction{.noun = "strength"}).value_or(1.0F),
    };
    object->finish();
    return description;
}

/// Reports a member that may not sit beside another.
struct ReportConflict {
    /// What the object that has both is, for messages.
    std::string_view noun;

    /// The member written beside it.
    std::string_view other;

    /// The member that conflicts with it.
    std::string_view name;

    std::optional<std::monostate> operator()(const Field& field, JsonFile& file) const {
        report_at(field, AssetProblemCode::conflicting_members,
                  std::format("the {} has both {:?} and {:?}, but may have only one of them", noun,
                              other, name),
                  file);
        return std::nullopt;
    }
};

std::optional<EmissionDescription> read_emission(const Field& field, JsonFile& file) {
    constexpr std::string_view noun = "emission";
    auto object = ReadObject{.noun = noun}(field, file);
    if (!object) {
        return std::nullopt;
    }
    EmissionDescription description{
        .nits = object->required("nits", read_nits).value_or(0.0F),
        .texture = object->optional("texture", read_texture),
    };
    if (object->has("color")) {
        description.color = object->required("color", read_unit_color).value_or(description.color);
        std::ignore = object->optional(
            "temperature", ReportConflict{.noun = noun, .other = "color", .name = "temperature"});
    } else if (object->has("temperature")) {
        description.color = object->required("temperature", read_temperature)
                                .transform(&Color::from_temperature)
                                .value_or(description.color);
    } else {
        object->report(
            AssetProblemCode::missing_member,
            R"(the emission has no "color" or "temperature", which it must have one of)");
    }
    object->finish();
    return description;
}

/// Tells whether any of a material's properties names a texture, as the file writes them. A normal
/// always has one, and its lack is a problem of the normal's, so a normal counts as one.
bool names_texture(const pedantjson::Object& material) {
    return material.find("normal") ||
           std::ranges::any_of(textured_properties, [&material](std::string_view name) {
               const auto property = material.find(name).and_then(
                   [](const pedantjson::Member& member) { return member.value.as_object(); });
               return property && property->find("texture");
           });
}

} // namespace

std::optional<MaterialDescription> decode_material(JsonFile& file) {
    const auto root = file.document().root();
    if (!check_format_and_version(root, material_format, file)) {
        return std::nullopt;
    }

    ObjectReader material(root, pedantjson::Pointer{}, "material", file);
    // Both are checked already. The schema an editor checks the file against is the editor's
    // business.
    std::ignore = material.required("format", read_string);
    std::ignore = material.required("version", read_number);
    std::ignore = material.optional("$schema", read_string);

    MaterialDescription description{
        .color = material.required("color", read_color_property).value_or(ColorDescription{}),
        .roughness = material.required("roughness", ReadValueProperty{.noun = "roughness"})
                         .value_or(ValueDescription{}),
        .metallic = material.required("metallic", ReadValueProperty{.noun = "metalness"})
                        .value_or(ValueDescription{}),
        .normal = material.optional("normal", read_normal),
        .occlusion = material.optional("occlusion", read_occlusion),
        .emission = material.optional("emission", read_emission),
    };
    // The object is one, since its format was found in it.
    if (names_texture(root.as_object().value_or(pedantjson::Object{}))) {
        if (!material.has("size")) {
            material.report(AssetProblemCode::missing_member,
                            R"(the material has no "size", which a material with a texture must )"
                            "have");
        }
        description.size = material.optional("size", read_size);
    } else {
        std::ignore =
            material.optional("size", ReportWithoutTexture{.noun = "material", .name = "size"});
    }
    description.properties = material.optional("properties", read_properties).value_or("");
    material.finish();
    return description;
}

} // namespace wulfhilde::detail
