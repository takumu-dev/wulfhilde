/// \file
/// The error of a map that couldn't be made or loaded: every problem found in it.

#pragma once

#include <wulfhilde/world/map_problem.hpp>

#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace wulfhilde {

/// Why a map couldn't be made or loaded: every problem found.
///
/// `Map::create` reports every problem in a description: the map's ambient light's first, then
/// sector by sector in the description's order, and then the start position's. A load of a map's
/// file finds problems in phases, and reports every problem of the first phase that finds any, in
/// the order of their positions in the file.
struct MapLoadError {
    /// The map's file, as its asset path with the extension of a map, such as
    /// `maps/e1m1.map.json`, or empty for a map made from a description built in code.
    std::string file{};

    /// The problems, of which there is at least one.
    std::vector<MapProblem> problems{};

    /// Compares two errors exactly.
    bool operator==(const MapLoadError&) const = default;
};

} // namespace wulfhilde

/// Formats an error as its problems, each on a line of its own after the file it lies in, such as
/// `maps/e1m1.map.json:12:7: no sector has the id "yard" (/start/sector)`. Compilers write their
/// problems so, and terminals and editors turn them into links.
template <> struct std::formatter<wulfhilde::MapLoadError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::MapLoadError& error, FormatContext& context) const {
        std::string lines;
        std::string_view separator;
        for (const auto& problem : error.problems) {
            lines += separator;
            if (!error.file.empty()) {
                // A position follows the file's name as a compiler writes it, with no space.
                std::format_to(std::back_inserter(lines), "{}:{}", error.file,
                               problem.position ? "" : " ");
            }
            std::format_to(std::back_inserter(lines), "{}", problem);
            separator = "\n";
        }
        return std::formatter<std::string_view>::format(lines, context);
    }
};
