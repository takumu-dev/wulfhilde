/// \file
/// A problem in an asset that a map uses, which keeps the map's assets from loading.

#pragma once

#include <wulfhilde/world/text_position.hpp>

#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace wulfhilde {

/// What kind of problem an asset has, for a program to branch on.
enum class AssetProblemCode : std::uint8_t {
    missing_file,    ///< No file is where an asset path leads.
    unreadable_file, ///< An asset's file is there, but can't be read.
    syntax_error,    ///< A JSON asset's file isn't JSON.
    unknown_format,  ///< A JSON asset's file isn't of the kind expected: its `format` is another.
    unsupported_version, ///< A JSON asset's `version` isn't one this version of the engine reads.
    wrong_type,          ///< A value is of the wrong kind, such as a string for a number.
    wrong_length,        ///< An array has more or fewer elements than it must.
    missing_member,      ///< An object lacks a member it must have.
    unknown_member,      ///< An object has a member that no object of its kind has.
    duplicate_member,    ///< An object has a member of the same name as an earlier one.
    number_out_of_range, ///< A number is beyond what its value can hold.
    conflicting_members, ///< An object has two members of which it may have only one, such as an
                         ///< emission's `color` and `temperature`.
    member_without_texture,   ///< A member that applies only to a texture, such as `channel` or a
                              ///< material's `size`, is written where there is no texture.
    unknown_channel,          ///< A `channel` isn't one of `r`, `g`, `b` and `a`.
    invalid_asset_path,       ///< An asset path breaks the syntax of one.
    color_out_of_range,       ///< A component of a colour is outside 0 to 1.
    light_level_out_of_range, ///< A light level, in nits, is below 0.
    value_out_of_range, ///< A number is outside the range its property allows, such as a roughness
                        ///< above 1 or a size of 0.
    invalid_image,      ///< An image's file isn't a PNG image that can be decoded.
    image_too_large,    ///< An image is more than 4,096 pixels on a side.
    alpha_in_color_texture, ///< A material's colour reads an image with an alpha channel, though a
                            ///< material is opaque.
    grey_normal_texture,    ///< A material's normal reads a grey image, which holds no direction.
    missing_channel,        ///< A single-value property reads an image of several channels without
                            ///< naming the one it reads.
    no_such_channel, ///< A single-value property names the alpha channel of an image without one.
};

/// A problem in an asset that a map uses, at the place in its file where it lies.
///
/// It has the shape of a problem in a map, with the file it lies in beside it, since a map's assets
/// span many files.
struct AssetProblem {
    /// What kind of problem it is.
    AssetProblemCode code{};

    /// What is wrong, for a person to read.
    std::string message{};

    /// The file the problem lies in, as its asset path with the extension of its kind of asset,
    /// such as `materials/brick.material.json`, or empty for a problem in the map, whose file the
    /// map doesn't know.
    std::string file{};

    /// The JSON pointer of the value the problem lies in, such as `/roughness/factor`, or empty
    /// for a problem in the whole file.
    std::string pointer{};

    /// Where in the file the problem lies, or nothing for a problem in the whole file or in the
    /// map.
    std::optional<TextPosition> position{};

    /// Compares two problems exactly.
    bool operator==(const AssetProblem&) const = default;
};

} // namespace wulfhilde

/// Formats a problem as its file, its position, its message, and its pointer in parentheses, such
/// as `materials/brick.material.json:4:17: expected a number, not a string (/roughness/factor)`.
/// Compilers write their problems so, and terminals and editors turn them into links. A problem in
/// the whole file has neither position nor pointer, and one in the map has a pointer alone, such as
/// `there is no file at "brick.material.json" (/sectors/0/floor/material)`.
template <> struct std::formatter<wulfhilde::AssetProblem> : std::formatter<std::string_view> {
    /// Writes a problem, taking the format specification of a string.
    ///
    /// \param problem The problem.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::AssetProblem& problem, FormatContext& context) const {
        auto text = problem.file;
        if (problem.position) {
            // A position follows the file's name as a compiler writes it, with no space.
            std::format_to(std::back_inserter(text), "{}{}:{}: ", text.empty() ? "" : ":",
                           problem.position->line, problem.position->column);
        } else if (!text.empty()) {
            text += ": ";
        }
        text += problem.message;
        if (!problem.pointer.empty()) {
            std::format_to(std::back_inserter(text), " ({})", problem.pointer);
        }
        return std::formatter<std::string_view>::format(text, context);
    }
};
