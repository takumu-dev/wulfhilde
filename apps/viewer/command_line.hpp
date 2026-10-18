#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace viewer {

/// What the command line asks the viewer to do.
struct CommandLine {
    /// Whether to print the usage and exit.
    bool help = false;
};

/// A command line the viewer can't follow.
struct UsageError {
    /// What is wrong with the command line, such as `unknown argument: --fly`.
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
