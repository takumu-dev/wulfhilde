#include "command_line.hpp"

#include <wulfhilde/assets/load_map.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace viewer {
namespace {

std::unexpected<UsageError> usage_error(UsageErrorCode code, std::string message) {
    return std::unexpected(UsageError{.code = code, .message = std::move(message)});
}

/// Returns a path as an absolute one in normal form, with no separator at its end, or an empty path
/// if the working directory can't be found.
std::filesystem::path normal_absolute(const std::filesystem::path& path) {
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error).lexically_normal();
    if (!absolute.has_filename() && absolute.has_relative_path()) {
        absolute = absolute.parent_path();
    }
    return absolute;
}

/// Works out the asset path of a map's file in a content root, which is the file's path within the
/// root without its extension.
///
/// \return The asset path, or nothing if the file isn't within the root.
std::optional<std::string> asset_path_in(const std::filesystem::path& map,
                                         const std::filesystem::path& content) {
    const auto relative = normal_absolute(map).lexically_relative(normal_absolute(content));
    if (relative.empty() || relative == "." || *relative.begin() == "..") {
        return std::nullopt;
    }
    auto path = relative.generic_string();
    path.resize(path.size() - wulfhilde::map_extension.size());
    return path;
}

} // namespace

std::string_view usage() noexcept {
    return "usage: wulfhilde-viewer <map> [--content <dir>]\n"
           "       wulfhilde-viewer --help\n"
           "\n"
           "The map-preview tool of the wulfhilde engine. It loads a map's file, ending "
           ".map.json,\n"
           "and reports every problem it finds there, each as file:line:column.\n"
           "\n"
           "options:\n"
           "  --content <dir>  the content root that holds the map and its assets; by default, "
           "the\n"
           "                   map file's directory\n"
           "  --help           print this usage and exit\n";
}

std::expected<CommandLine, UsageError>
parse_command_line(std::span<const std::string_view> arguments) {
    CommandLine command_line;
    std::optional<std::filesystem::path> content;
    for (auto argument = arguments.begin(); argument != arguments.end(); ++argument) {
        if (*argument == "--help") {
            command_line.help = true;
        } else if (*argument == "--content") {
            if (std::next(argument) == arguments.end()) {
                return usage_error(UsageErrorCode::missing_value,
                                   "--content needs a directory after it");
            }
            ++argument;
            if (content) {
                return usage_error(UsageErrorCode::extra_argument,
                                   std::format("a second content root: {}, after {}", *argument,
                                               content->string()));
            }
            content = *argument;
        } else if (argument->starts_with('-')) {
            return usage_error(UsageErrorCode::unknown_argument,
                               std::format("unknown argument: {}", *argument));
        } else if (!command_line.map.empty()) {
            return usage_error(UsageErrorCode::extra_argument,
                               std::format("a second map file: {}, after {}", *argument,
                                           command_line.map.string()));
        } else {
            command_line.map = *argument;
        }
    }
    if (command_line.help) {
        return command_line;
    }

    if (command_line.map.empty()) {
        return usage_error(UsageErrorCode::missing_map, "no map file to open");
    }
    const auto name = command_line.map.filename().string();
    if (name.size() <= wulfhilde::map_extension.size() ||
        !name.ends_with(wulfhilde::map_extension)) {
        return usage_error(UsageErrorCode::not_a_map_file,
                           std::format("{} isn't a map file, whose name ends in {}",
                                       command_line.map.string(), wulfhilde::map_extension));
    }

    // A map file in the working directory has no directory of its own written.
    command_line.content = content.value_or(command_line.map.parent_path());
    if (command_line.content.empty()) {
        command_line.content = ".";
    }
    auto asset = asset_path_in(command_line.map, command_line.content);
    if (!asset) {
        return usage_error(UsageErrorCode::map_outside_content,
                           std::format("{} isn't in the content root, {}",
                                       command_line.map.string(), command_line.content.string()));
    }
    command_line.map_asset_path = std::move(*asset);
    return command_line;
}

} // namespace viewer
