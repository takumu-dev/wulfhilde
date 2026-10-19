/// \file
/// A problem in a map, which keeps it from being made.

#pragma once

#include <wulfhilde/world/text_position.hpp>

#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>

namespace wulfhilde {

/// What kind of problem a map has, for a program to branch on.
///
/// `Map::create` finds problems in a map's description. The assets module, reading a map's file
/// into a description, finds those that only a file can have, from `missing_file` on.
enum class MapProblemCode : std::uint8_t {
    duplicate_sector_id,     ///< A sector has the id of an earlier one.
    unknown_sector,          ///< A reference names a sector that no sector has the id of.
    coordinate_out_of_range, ///< A coordinate is more than 4,096 m from the origin, or not a
                             ///< number.
    invalid_asset_path,      ///< An asset path breaks the syntax of one.
    missing_outer_loop,      ///< A sector has no loops, so no outer loop.
    unsupported_inner_loop,  ///< A sector has an inner loop, which this version of the engine can't
                             ///< make.
    too_few_walls,           ///< A loop has fewer than three walls.
    zero_length_wall,        ///< A wall starts where the next wall in its loop starts.
    self_intersecting_loop,  ///< Two walls of a loop cross or touch, other than where one ends and
                             ///< the next starts.
    wrong_winding,           ///< An outer loop runs clockwise seen from above.
    color_out_of_range,      ///< A component of a light's colour is outside 0 to 1, or not a
                             ///< number.
    light_level_out_of_range, ///< A light level is below 0, or not a finite number.
    missing_file,             ///< No file is at the map's asset path.
    unreadable_file,          ///< The map's file is there, but can't be read.
    syntax_error,             ///< The map's file isn't JSON.
    unknown_format,           ///< The file isn't a map: its `format` isn't `wulfhilde-map`.
    unsupported_version,      ///< The map's `version` isn't one this version of the engine reads.
    wrong_type,               ///< A value is of the wrong kind, such as a string for a number.
    wrong_length,             ///< An array has more or fewer elements than it must, such as a
                              ///< point of three coordinates where a point seen from above belongs.
    missing_member,           ///< An object lacks a member it must have.
    unknown_member,           ///< An object has a member that no object of its kind has.
    duplicate_member,         ///< An object has a member of the same name as an earlier one.
    number_out_of_range,      ///< A number is beyond what its value can hold.
};

/// A problem in a map, found by `Map::create` in its description, or by the assets module in its
/// file.
struct MapProblem {
    /// What kind of problem it is.
    MapProblemCode code{};

    /// What is wrong, for a person to read.
    std::string message{};

    /// The JSON pointer of the value the problem lies in, such as `/sectors/0/floor/height`, which
    /// is also its path through the description, or empty for a problem in the whole file.
    std::string pointer{};

    /// Where in the map's file the problem lies, or nothing for one found in a description built
    /// in code, or in a file that couldn't be read.
    std::optional<TextPosition> position{};

    /// Compares two problems exactly.
    bool operator==(const MapProblem&) const = default;
};

} // namespace wulfhilde

/// Formats a problem as its position, its message, and its pointer in parentheses, such as
/// `12:7: no sector has the id "yard" (/start/sector)`. A problem without a position has none
/// before its message, and one in the whole file no pointer after it.
template <> struct std::formatter<wulfhilde::MapProblem> : std::formatter<std::string_view> {
    /// Writes a problem, taking the format specification of a string.
    ///
    /// \param problem The problem.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::MapProblem& problem, FormatContext& context) const {
        std::string text;
        if (problem.position) {
            std::format_to(std::back_inserter(text), "{}:{}: ", problem.position->line,
                           problem.position->column);
        }
        text += problem.message;
        if (!problem.pointer.empty()) {
            std::format_to(std::back_inserter(text), " ({})", problem.pointer);
        }
        return std::formatter<std::string_view>::format(text, context);
    }
};
