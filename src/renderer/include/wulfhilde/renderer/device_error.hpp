/// \file
/// The error of a renderer that couldn't come up on a device, or whose device failed it.

#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace wulfhilde {

/// What kind of failure kept the renderer from its device, for a program to branch on.
enum class DeviceErrorCode : std::uint8_t {
    no_vulkan,          ///< No Vulkan loader was found, or only one older than Vulkan 1.3.
    no_validation,      ///< Validation was asked for, and the validation layers aren't installed.
    no_suitable_device, ///< No device has every feature the renderer needs.
    device_failure,     ///< The device failed a request, such as by running out of memory.
};

/// Why the renderer couldn't come up on a device, or what failed on it.
struct DeviceError {
    /// What kind of failure it is.
    DeviceErrorCode code{};

    /// What went wrong, for a person to read, naming each feature a device lacks where that is
    /// the reason.
    std::string message{};

    /// Compares two errors exactly.
    bool operator==(const DeviceError&) const = default;
};

} // namespace wulfhilde

/// Formats an error as its message.
template <> struct std::formatter<wulfhilde::DeviceError> : std::formatter<std::string_view> {
    /// Writes an error, taking the format specification of a string.
    ///
    /// \param error The error.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const wulfhilde::DeviceError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};
