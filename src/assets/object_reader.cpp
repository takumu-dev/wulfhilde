#include "object_reader.hpp"

#include "json_file.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <pedantjson/pedantjson.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace wulfhilde::detail {
namespace {

/// Reports each key of an object that an earlier key of the object has already, at the later key.
void report_duplicates(const pedantjson::Object& object, const pedantjson::Pointer& pointer,
                       std::string_view noun, JsonFile& file) {
    std::unordered_map<std::string_view, std::size_t> first_offsets;
    for (const auto& member : object) {
        const auto name = member.key.name();
        const auto [first, inserted] = first_offsets.try_emplace(name, member.key.offset());
        if (!inserted) {
            file.report(MapProblemCode::duplicate_member, pointer / name, member.key.offset(),
                        std::format("the {} has {:?} already, on line {}", noun, name,
                                    file.document().position(first->second).line));
        }
    }
}

/// Reports each duplicate key in a value of game-defined data, and in everything within it however
/// deep.
void report_duplicates_within(const Field& field, JsonFile& file) {
    std::vector<Field> pending{field};
    while (!pending.empty()) {
        const auto [value, pointer] = std::move(pending.back());
        pending.pop_back();
        if (const auto object = value.as_object()) {
            report_duplicates(*object, pointer, "object", file);
            for (const auto& member : *object) {
                pending.push_back({.value = member.value, .pointer = pointer / member.key.name()});
            }
        } else if (const auto array = value.as_array()) {
            for (std::size_t i = 0; const auto element : *array) {
                pending.push_back({.value = element, .pointer = pointer / i++});
            }
        }
    }
}

/// Reports a problem in a value, at its position.
void report_at(const Field& field, MapProblemCode code, std::string message, JsonFile& file) {
    file.report(code, field.pointer, field.value.offset(), std::move(message));
}

/// Reports a value of the wrong kind.
void report_wrong_type(const Field& field, std::string_view expected, JsonFile& file) {
    report_at(field, MapProblemCode::wrong_type,
              std::format("expected {}, not {}", expected, kind_name(field.value.kind())), file);
}

/// Returns the JSON text of the object that begins at an offset of a file's text, which is JSON.
std::string_view object_text(std::string_view text, std::size_t offset) {
    const auto rest = text.substr(offset);
    std::size_t depth = 0;
    auto in_string = false;
    auto escaped = false;
    for (std::size_t end = 1; const char c : rest) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else if (c == '"') {
            in_string = true;
        } else if (c == '{' || c == '[') {
            ++depth;
        } else if ((c == '}' || c == ']') && --depth == 0) {
            return rest.substr(0, end);
        }
        ++end;
    }
    return rest;
}

/// Reads a value as an array of a given count of numbers.
template <std::size_t Count>
std::optional<std::array<float, Count>> read_numbers(const Field& field, JsonFile& file) {
    const auto elements = read_array(field, file);
    if (!elements) {
        return std::nullopt;
    }
    if (elements->size() != Count) {
        report_at(field, MapProblemCode::wrong_length,
                  std::format("expected {} numbers, not {}", Count, elements->size()), file);
        return std::nullopt;
    }
    std::array<float, Count> numbers{};
    auto read = true;
    for (auto&& [number, element] : std::views::zip(numbers, *elements)) {
        const auto value = read_number(element, file);
        read = value.has_value() && read;
        number = value.value_or(0.0F);
    }
    return read ? std::optional(numbers) : std::nullopt;
}

} // namespace

ObjectReader::ObjectReader(pedantjson::Value object, pedantjson::Pointer pointer,
                           std::string_view noun, JsonFile& file)
    : object_(object.as_object().value_or(pedantjson::Object{})), offset_(object.offset()),
      pointer_(std::move(pointer)), noun_(noun), file_(file), asked_for_(object_.size(), false) {
    WULFHILDE_DEBUG_ASSERT(object.kind() == pedantjson::Kind::object,
                           "an object reader reads an object");
    report_duplicates(object_, pointer_, noun_, file);
}

void ObjectReader::finish() {
    const std::string_view article = noun_.find_first_of("aeiou") == 0 ? "an" : "a";
    for (auto&& [member, asked_for] : std::views::zip(object_, asked_for_)) {
        if (!asked_for) {
            file_.get().report(
                MapProblemCode::unknown_member, pointer_ / member.key.name(), member.key.offset(),
                std::format("{} {} has no member {:?}", article, noun_, member.key.name()));
            // Its duplicates are reported as duplicates already.
            std::ignore = take(member.key.name(), false);
        }
    }
}

std::optional<Field> ObjectReader::take(std::string_view name, bool required) {
    std::optional<Field> first;
    for (auto&& [member, asked_for] : std::views::zip(object_, asked_for_)) {
        if (member.key.name() == name) {
            asked_for = true;
            if (!first) {
                first = Field{.value = member.value, .pointer = pointer_ / name};
            }
        }
    }
    if (!first && required) {
        file_.get().report(MapProblemCode::missing_member, pointer_, offset_,
                           std::format("the {} has no {:?}, which it must have", noun_, name));
    }
    return first;
}

std::optional<ObjectReader> ReadObject::operator()(const Field& field, JsonFile& file) const {
    if (field.value.kind() != pedantjson::Kind::object) {
        report_wrong_type(field, "an object", file);
        return std::nullopt;
    }
    return ObjectReader(field.value, field.pointer, noun, file);
}

std::optional<std::vector<Field>> read_array(const Field& field, JsonFile& file) {
    const auto array = field.value.as_array();
    if (!array) {
        report_wrong_type(field, "an array", file);
        return std::nullopt;
    }
    std::vector<Field> elements;
    elements.reserve(array->size());
    for (std::size_t i = 0; const auto element : *array) {
        elements.push_back({.value = element, .pointer = field.pointer / i++});
    }
    return elements;
}

std::optional<std::string> read_string(const Field& field, JsonFile& file) {
    const auto string = field.value.as_string();
    if (!string) {
        report_wrong_type(field, "a string", file);
        return std::nullopt;
    }
    return std::string{*string};
}

std::optional<float> read_number(const Field& field, JsonFile& file) {
    const auto number = field.value.as_number();
    if (!number) {
        report_wrong_type(field, "a number", file);
        return std::nullopt;
    }
    // A float holds any number in its range, so out of range is the only way this fails.
    const auto value = number->to<float>();
    if (!value) {
        report_at(field, MapProblemCode::number_out_of_range,
                  std::format("the number {} is out of range, too large or too near zero to read",
                              number->text()),
                  file);
        return std::nullopt;
    }
    return *value;
}

std::optional<Angle> read_angle(const Field& field, JsonFile& file) {
    return read_number(field, file).transform(&Angle::degrees);
}

std::optional<Vector2> read_vector2(const Field& field, JsonFile& file) {
    return read_numbers<2>(field, file).transform([](const auto& numbers) {
        const auto [x, y] = numbers;
        return Vector2{.x = x, .y = y};
    });
}

std::optional<Vector3> read_vector3(const Field& field, JsonFile& file) {
    return read_numbers<3>(field, file).transform([](const auto& numbers) {
        const auto [x, y, z] = numbers;
        return Vector3{.x = x, .y = y, .z = z};
    });
}

std::optional<Color> read_color(const Field& field, JsonFile& file) {
    return read_numbers<3>(field, file).transform([](const auto& numbers) {
        const auto [r, g, b] = numbers;
        return Color{.r = r, .g = g, .b = b};
    });
}

std::optional<std::string> read_properties(const Field& field, JsonFile& file) {
    if (field.value.kind() != pedantjson::Kind::object) {
        report_wrong_type(field, "an object", file);
        return std::nullopt;
    }
    report_duplicates_within(field, file);
    return std::string{object_text(file.text(), field.value.offset())};
}

std::string_view kind_name(pedantjson::Kind kind) noexcept {
    switch (kind) {
    case pedantjson::Kind::null:
        return "null";
    case pedantjson::Kind::boolean:
        return "a boolean";
    case pedantjson::Kind::number:
        return "a number";
    case pedantjson::Kind::string:
        return "a string";
    case pedantjson::Kind::array:
        return "an array";
    case pedantjson::Kind::object:
        return "an object";
    }
    // Only a kind cast from outside the enumeration gets here.
    return "a value";
}

} // namespace wulfhilde::detail
