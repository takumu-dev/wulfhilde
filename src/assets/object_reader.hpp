#pragma once

#include "json_file.hpp"
#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>

#include <pedantjson/pedantjson.hpp>

#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace wulfhilde::detail {

/// A value of a file, with its JSON pointer.
struct Field {
    /// The value.
    pedantjson::Value value;

    /// The value's JSON pointer.
    pedantjson::Pointer pointer;
};

/// Reports a problem in a value, at its position.
///
/// \param field The value.
/// \param code What kind of problem it is.
/// \param message What is wrong.
/// \param file The file.
void report_at(const Field& field, AssetProblemCode code, std::string message, JsonFile& file);

/// Reads one object of a file, member by member, reporting every structural problem in it once.
///
/// Making one reports each duplicate key. Each member is then asked for by name, as required or
/// optional, and decoded by a function that reports what is wrong with its value and gives nothing
/// back. A value of the wrong kind is reported once, and what lies under it isn't looked at.
/// `finish` reports every member nobody asked for.
class ObjectReader {
  public:
    /// Starts reading an object, and reports each of its keys that an earlier key has already.
    ///
    /// \param object The object, which must be one.
    /// \param pointer The object's JSON pointer.
    /// \param noun What the object is, for messages, such as `floor`.
    /// \param file The file, which must outlive this.
    ObjectReader(pedantjson::Value object, pedantjson::Pointer pointer, std::string_view noun,
                 JsonFile& file);

    /// Reads a member that the object must have, reporting it if the object hasn't.
    ///
    /// \param name The member's name.
    /// \param decode What reads its value: a function of a `Field` and a `JsonFile&` that returns
    ///               an optional value, empty after it has reported what is wrong.
    /// \return The value, or nothing if it is missing or wrong.
    template <typename Decode> auto required(std::string_view name, Decode decode) {
        const auto field = take(name, true);
        return field ? decode(*field, file_.get()) : decltype(decode(*field, file_.get())){};
    }

    /// Reads a member that the object may leave out.
    ///
    /// \param name The member's name.
    /// \param decode What reads its value, as for `required`.
    /// \return The value, or nothing if it is left out or wrong.
    template <typename Decode> auto optional(std::string_view name, Decode decode) {
        const auto field = take(name, false);
        return field ? decode(*field, file_.get()) : decltype(decode(*field, file_.get())){};
    }

    /// Tells whether the object has a member, without asking for it.
    ///
    /// \param name The member's name.
    [[nodiscard]] bool has(std::string_view name) const;

    /// Reports a problem of the object as a whole, at its position.
    ///
    /// \param code What kind of problem it is.
    /// \param message What is wrong.
    void report(AssetProblemCode code, std::string message);

    /// Reports every member that nobody asked for as one that no object of its kind has.
    void finish();

  private:
    /// Finds a member, marking it asked for, and those of its name that come after it too, which
    /// were reported as duplicates already.
    ///
    /// \return The first member of that name, or nothing if there is none.
    std::optional<Field> take(std::string_view name, bool required);

    pedantjson::Object object_;
    std::size_t offset_ = 0;
    pedantjson::Pointer pointer_;
    std::string_view noun_;
    std::reference_wrapper<JsonFile> file_;
    std::vector<bool> asked_for_;
};

/// Reads an object, as one of a kind.
struct ReadObject {
    /// What the object is, for messages, such as `floor`.
    std::string_view noun;

    /// Reads a value as an object.
    ///
    /// \return A reader of it, or nothing if it isn't an object.
    std::optional<ObjectReader> operator()(const Field& field, JsonFile& file) const;
};

/// Reads a value as an array.
///
/// \return Its elements, each with its pointer, or nothing if it isn't an array.
[[nodiscard]] std::optional<std::vector<Field>> read_array(const Field& field, JsonFile& file);

/// Reads a value as a string.
[[nodiscard]] std::optional<std::string> read_string(const Field& field, JsonFile& file);

/// Reads a value as a number, which a `float` holds.
[[nodiscard]] std::optional<float> read_number(const Field& field, JsonFile& file);

/// Reads a value as an array of a given count of numbers, each read by a function that may check
/// its range too.
///
/// \param read_element What reads each number: a function of a `Field` and a `JsonFile&` that
///                     returns an optional `float`, empty after it has reported what is wrong.
/// \return The numbers, or nothing if the array or any of them is wrong.
template <std::size_t Count, typename ReadElement>
[[nodiscard]] std::optional<std::array<float, Count>>
read_numbers(const Field& field, JsonFile& file, ReadElement read_element) {
    const auto elements = read_array(field, file);
    if (!elements) {
        return std::nullopt;
    }
    if (elements->size() != Count) {
        report_at(field, AssetProblemCode::wrong_length,
                  std::format("expected {} numbers, not {}", Count, elements->size()), file);
        return std::nullopt;
    }
    std::array<float, Count> numbers{};
    auto read = true;
    for (auto&& [number, element] : std::views::zip(numbers, *elements)) {
        const auto value = read_element(element, file);
        read = value.has_value() && read;
        number = value.value_or(0.0F);
    }
    return read ? std::optional(numbers) : std::nullopt;
}

/// Reads a value as an angle, a number of degrees.
[[nodiscard]] std::optional<Angle> read_angle(const Field& field, JsonFile& file);

/// Reads a value as a pair of numbers, `[x, y]`.
[[nodiscard]] std::optional<Vector2> read_vector2(const Field& field, JsonFile& file);

/// Reads a value as three numbers, `[x, y, z]`.
[[nodiscard]] std::optional<Vector3> read_vector3(const Field& field, JsonFile& file);

/// Reads a value as a colour, three numbers `[r, g, b]`.
[[nodiscard]] std::optional<Color> read_color(const Field& field, JsonFile& file);

/// Reads a value as an object of game-defined data, which the engine keeps and ignores, reporting
/// any duplicate key within it.
///
/// \return The object's JSON text, as the file writes it, or nothing if it isn't an object.
[[nodiscard]] std::optional<std::string> read_properties(const Field& field, JsonFile& file);

/// Names a kind of value with its article, as messages do: `an object`, `null`.
[[nodiscard]] std::string_view kind_name(pedantjson::Kind kind) noexcept;

} // namespace wulfhilde::detail
