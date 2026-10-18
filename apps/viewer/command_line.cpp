#include "command_line.hpp"

#include <expected>
#include <format>
#include <span>
#include <string_view>

namespace viewer {

std::string_view usage() noexcept {
    return "usage: wulfhilde-viewer [--help]\n"
           "\n"
           "The map-preview tool of the wulfhilde engine.\n"
           "\n"
           "options:\n"
           "  --help  print this usage and exit\n";
}

std::expected<CommandLine, UsageError>
parse_command_line(std::span<const std::string_view> arguments) {
    CommandLine command_line;
    for (const std::string_view argument : arguments) {
        if (argument == "--help") {
            command_line.help = true;
        } else {
            return std::unexpected(UsageError{
                .code = UsageErrorCode::unknown_argument,
                .message = std::format("unknown argument: {}", argument),
            });
        }
    }
    return command_line;
}

} // namespace viewer
