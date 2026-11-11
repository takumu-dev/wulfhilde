/// \file
/// The error of assets that couldn't be found or read.

#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace wulfhilde {

/// What kind of failure kept assets from being found or read, for a program to branch on.
enum class AssetErrorCode : std::uint8_t {
    missing_content_root, ///< Nothing is where a content root was to be opened.
    not_a_directory,      ///< What is where a content root was to be opened isn't a directory.
};

/// Why assets couldn't be found or read.
struct AssetError {
    /// What kind of failure it is.
    AssetErrorCode code{};

    /// What went wrong, for a person to read, such as `no directory is at "content"`.
    std::string message{};

    /// Compares two errors exactly.
    bool operator==(const AssetError&) const = default;
};

} // namespace wulfhilde

/// Formats an error as its message.
template <> struct std::formatter<wulfhilde::AssetError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::AssetError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};
