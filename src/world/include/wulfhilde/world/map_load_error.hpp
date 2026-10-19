/// \file
/// The error of a map that couldn't be made: every problem found in its description.

#pragma once

#include <wulfhilde/world/map_problem.hpp>

#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace wulfhilde {

/// Why a map couldn't be made: every problem found in its description, sector by sector in the
/// description's order, and then the start position's.
struct MapLoadError {
    /// The problems, of which there is at least one.
    std::vector<MapProblem> problems{};

    /// Compares two errors exactly.
    bool operator==(const MapLoadError&) const = default;
};

} // namespace wulfhilde

/// Formats an error as its problems, each on a line of its own.
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
            std::format_to(std::back_inserter(lines), "{}{}", separator, problem);
            separator = "\n";
        }
        return std::formatter<std::string_view>::format(lines, context);
    }
};
