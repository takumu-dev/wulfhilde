#pragma once

#include <cstdint>
#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace viewer {

/// What the command line asks the viewer to do.
struct CommandLine {
    /// Whether to print the usage and exit.
    bool help = false;
};

/// What kind of mistake makes a command line one the viewer can't follow.
enum class UsageErrorCode : std::uint8_t {
    unknown_argument, ///< An argument the viewer doesn't know.
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
