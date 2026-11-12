#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector3.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace viewer {

/// A camera that `--camera` names, as the command line writes it, before the map is loaded to
/// place it in.
struct CameraOption {
    /// The id of the sector the camera is in.
    std::string sector{};

    /// The eye's point in the sector's coordinates, in metres.
    wulfhilde::Vector3 point{};

    /// How far the facing turns left from +X, from -180° to 180°.
    wulfhilde::Angle yaw{};

    /// How far the facing tilts up from the horizontal, from -90° to 90°.
    wulfhilde::Angle pitch{};

    /// How far the facing tilts its right side down, from -180° to 180°.
    wulfhilde::Angle roll{};
};

/// What the command line asks the viewer to do.
struct CommandLine {
    /// Whether to print the usage and exit.
    bool help = false;

    /// The map's file, as the command line names it.
    std::filesystem::path map{};

    /// The content root that holds the map: the directory `--content` names, or else the map
    /// file's own.
    std::filesystem::path content{};

    /// The map's asset path in its content root.
    std::string map_asset_path{};

    /// The camera to draw from instead of the start position, if `--camera` names one.
    std::optional<CameraOption> camera{};

    /// Whether the window covers the display.
    bool fullscreen = false;

    /// How many samples each pixel takes: 1, 2, 4, 8, 16, 32 or 64.
    std::size_t samples = 4;
};

/// What kind of mistake makes a command line one the viewer can't follow.
enum class UsageErrorCode : std::uint8_t {
    unknown_argument,    ///< An argument the viewer doesn't know.
    missing_value,       ///< An option without the value that must follow it.
    missing_map,         ///< No map file to open.
    extra_argument,      ///< A second map file, or a second value of an option that takes one.
    not_a_map_file,      ///< A map file whose name doesn't end in `.map.json`.
    map_outside_content, ///< A map file that isn't in its content root.
    invalid_value,       ///< An option's value that doesn't read as the option needs.
    out_of_range,        ///< An option's value outside the range the option takes.
    camera_outside_map,  ///< A camera in a sector the map lacks, or outside its sector.
};

/// A command line the viewer can't follow.
struct UsageError {
    /// What kind of mistake it is.
    UsageErrorCode code;

    /// What is wrong, naming the argument at fault, such as `unknown argument: --fly`.
    std::string message;
};

/// The viewer's usage, as `--help` prints it, ending in a line break.
[[nodiscard]] std::string_view usage() noexcept;

/// Reads the arguments that follow the program's name.
///
/// The map file must lie in its content root, which this works out from the two paths alone,
/// without looking at the filesystem, so a path through a symbolic link counts as the path it is
/// written as.
///
/// \param arguments The arguments, in order.
/// \return What they ask for, or what is wrong with them.
[[nodiscard]] std::expected<CommandLine, UsageError>
parse_command_line(std::span<const std::string_view> arguments);

} // namespace viewer

/// Formats a usage error as its message.
template <> struct std::formatter<viewer::UsageError> : std::formatter<std::string_view> {
    /// Writes the error's message, taking the format specification of a string.
    ///
    /// \param error The error to format.
    /// \param context Where to write it.
    /// \return The end of what was written.
    template <typename FormatContext>
    auto format(const viewer::UsageError& error, FormatContext& context) const {
        return std::formatter<std::string_view>::format(error.message, context);
    }
};
