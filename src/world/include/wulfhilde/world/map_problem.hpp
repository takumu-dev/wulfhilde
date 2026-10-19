/// \file
/// A problem in a map's description, which keeps a map from being made.

#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace wulfhilde {

/// What kind of problem a map's description has, for a program to branch on.
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
};

/// A problem in a map's description, found by `Map::create`.
struct MapProblem {
    /// What kind of problem it is.
    MapProblemCode code{};

    /// What is wrong, for a person to read.
    std::string message{};

    /// The JSON pointer of the value the problem lies in, such as `/sectors/0/floor/height`, which
    /// is also its path through the description.
    std::string pointer{};

    /// Compares two problems exactly.
    bool operator==(const MapProblem&) const = default;
};

} // namespace wulfhilde

/// Formats a problem as its message followed by its pointer in parentheses, such as
/// `no sector has the id "yard" (/start/sector)`.
template <> struct std::formatter<wulfhilde::MapProblem> : std::formatter<std::string_view> {
    /// Writes a problem, taking the format specification of a string.
    ///
    /// \param problem The problem.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::MapProblem& problem, FormatContext& context) const {
        return std::formatter<std::string_view>::format(
            std::format("{} ({})", problem.message, problem.pointer), context);
    }
};
