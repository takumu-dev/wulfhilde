/// \file
/// The error of assets that couldn't be found or read.

#pragma once

#include <wulfhilde/assets/asset_problem.hpp>

#include <cstdint>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace wulfhilde {

/// What kind of failure kept assets from being found or read, for a program to branch on.
enum class AssetErrorCode : std::uint8_t {
    missing_content_root, ///< Nothing is where a content root was to be opened.
    not_a_directory,      ///< What is where a content root was to be opened isn't a directory.
    asset_problems,       ///< Assets a map uses are missing or have problems, which `problems`
                          ///< lists.
};

/// Why assets couldn't be found or read.
struct AssetError {
    /// What kind of failure it is.
    AssetErrorCode code{};

    /// What went wrong, for a person to read, such as `no directory is at "content"`.
    std::string message{};

    /// Every problem found in the assets, for `asset_problems`, and none otherwise.
    std::vector<AssetProblem> problems{};

    /// Compares two errors exactly.
    bool operator==(const AssetError&) const = default;
};

} // namespace wulfhilde

/// Formats an error as its problems, each on a line of its own, or as its message if it has none.
template <> struct std::formatter<wulfhilde::AssetError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::AssetError& error, FormatContext& context) const {
        if (error.problems.empty()) {
            return std::formatter<std::string_view>::format(error.message, context);
        }
        std::string lines;
        std::string_view separator;
        for (const auto& problem : error.problems) {
            std::format_to(std::back_inserter(lines), "{}{}", separator, problem);
            separator = "\n";
        }
        return std::formatter<std::string_view>::format(lines, context);
    }
};
