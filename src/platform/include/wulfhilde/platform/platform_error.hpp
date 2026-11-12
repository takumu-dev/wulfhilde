/// \file
/// The error of a platform or a window that couldn't come up, or that refused a request.

#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace wulfhilde {

/// What kind of failure the platform met, for a program to branch on.
enum class PlatformErrorCode : std::uint8_t {
    no_video,       ///< Video couldn't start, such as with no display to open windows on.
    window_failure, ///< A window couldn't open, or refused a request made of it.
};

/// Why the platform or one of its windows couldn't do what was asked of it.
struct PlatformError {
    /// What kind of failure it is.
    PlatformErrorCode code{};

    /// What went wrong, for a person to read, with the reason the system gave.
    std::string message{};

    /// Compares two errors exactly.
    bool operator==(const PlatformError&) const = default;
};

} // namespace wulfhilde

/// Formats an error as its message.
template <> struct std::formatter<wulfhilde::PlatformError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::PlatformError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};
