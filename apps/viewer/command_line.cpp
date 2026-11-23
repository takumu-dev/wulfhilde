#include "command_line.hpp"

#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/core/angle.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace viewer {
namespace {

std::unexpected<UsageError> usage_error(UsageErrorCode code, std::string message) {
    return std::unexpected(UsageError{.code = code, .message = std::move(message)});
}

constexpr std::string_view camera_form = "<sector>,<x>,<y>,<z>,<yaw>,<pitch>[,<roll>]";

/// Every sample count that Vulkan can name.
constexpr std::array<std::size_t, 7> sample_counts{1, 2, 4, 8, 16, 32, 64};

/// Reads the whole of a text as a number of a type, or nothing if any of it doesn't read.
template <typename T> std::optional<T> read_number(std::string_view text) {
    T value{};
    const auto* const end = std::to_address(text.end());
    const auto [stop, error] = std::from_chars(std::to_address(text.begin()), end, value);
    if (error != std::errc{} || stop != end) {
        return std::nullopt;
    }
    return value;
}

/// One of the numbers after a camera's sector, by its name, with the largest size it may have, if
/// it is an angle.
struct CameraField {
    std::string_view name;
    std::optional<float> limit;
};

constexpr std::array<CameraField, 6> camera_fields{
    {
        {.name = "x", .limit = std::nullopt},
        {.name = "y", .limit = std::nullopt},
        {.name = "z", .limit = std::nullopt},
        {.name = "yaw", .limit = 180.0F},
        {.name = "pitch", .limit = 90.0F},
        {.name = "roll", .limit = 180.0F},
    },
};

std::expected<CameraOption, UsageError> parse_camera(std::string_view text) {
    const auto fields = std::views::split(text, ',') |
                        std::views::transform([](auto field) { return std::string_view(field); }) |
                        std::ranges::to<std::vector>();
    if (fields.size() != camera_fields.size() && fields.size() != camera_fields.size() + 1) {
        return usage_error(UsageErrorCode::invalid_value,
                           std::format("--camera needs {}, not {}", camera_form, text));
    }
    if (fields.front().empty()) {
        return usage_error(UsageErrorCode::invalid_value,
                           std::format("--camera names no sector: {}", text));
    }
    // A roll that isn't written stays at zero, since the fields run out before it.
    std::array<float, camera_fields.size()> values{};
    for (auto&& [field, written, value] :
         std::views::zip(camera_fields, fields | std::views::drop(1), values)) {
        const auto number = read_number<float>(written);
        if (!number || !std::isfinite(*number)) {
            return usage_error(
                UsageErrorCode::invalid_value,
                std::format("--camera's {} isn't a number: {}", field.name, written));
        }
        if (field.limit && std::abs(*number) > *field.limit) {
            return usage_error(UsageErrorCode::out_of_range,
                               std::format("--camera's {} must be from -{} to {} degrees, not {}",
                                           field.name, *field.limit, *field.limit, written));
        }
        value = *number;
    }
    const auto [x, y, z, yaw, pitch, roll] = values;
    return CameraOption{
        .sector = std::string(fields.front()),
        .point = {.x = x, .y = y, .z = z},
        .yaw = wulfhilde::Angle::degrees(yaw),
        .pitch = wulfhilde::Angle::degrees(pitch),
        .roll = wulfhilde::Angle::degrees(roll),
    };
}

std::expected<std::size_t, UsageError> parse_samples(std::string_view text) {
    const auto samples = read_number<std::size_t>(text);
    if (!samples) {
        return usage_error(UsageErrorCode::invalid_value,
                           std::format("--samples needs a whole number, not {}", text));
    }
    if (!std::ranges::contains(sample_counts, *samples)) {
        return usage_error(UsageErrorCode::out_of_range,
                           std::format("--samples must be 1, 2, 4, 8, 16, 32 or 64, not {}", text));
    }
    return *samples;
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

/// The arguments as they are written, each put in its place, before any is read as what it means.
struct WrittenArguments {
    bool help = false;
    bool fullscreen = false;
    std::optional<std::string_view> map;
    std::optional<std::string_view> content;
    std::optional<std::string_view> camera;
    std::optional<std::string_view> samples;
};

/// An option that takes the argument after it as its value, which it may be given once: what must
/// follow it, what its value is called, and where the value goes.
struct ValueOption {
    std::string_view name;
    std::string_view needs;
    std::string_view value_name;
    std::optional<std::string_view> WrittenArguments::* value;
};

constexpr std::array<ValueOption, 3> value_options{
    {
        {
            .name = "--content",
            .needs = "a directory",
            .value_name = "content root",
            .value = &WrittenArguments::content,
        },
        {
            .name = "--camera",
            .needs = camera_form,
            .value_name = "camera",
            .value = &WrittenArguments::camera,
        },
        {
            .name = "--samples",
            .needs = "a sample count",
            .value_name = "sample count",
            .value = &WrittenArguments::samples,
        },
    },
};

/// The option that takes a value, by its name, or none.
const ValueOption* value_option(std::string_view name) {
    for (const auto& option : value_options) {
        if (option.name == name) {
            return &option;
        }
    }
    return nullptr;
}

std::expected<WrittenArguments, UsageError>
read_arguments(std::span<const std::string_view> arguments) {
    WrittenArguments written;
    for (auto argument = arguments.begin(); argument != arguments.end(); ++argument) {
        if (const auto* const option = value_option(*argument); option != nullptr) {
            if (std::next(argument) == arguments.end()) {
                return usage_error(
                    UsageErrorCode::missing_value,
                    std::format("{} needs {} after it", option->name, option->needs));
            }
            ++argument;
            auto& value = written.*option->value;
            if (value) {
                return usage_error(UsageErrorCode::extra_argument,
                                   std::format("a second {}: {}, after {}", option->value_name,
                                               *argument, *value));
            }
            value = *argument;
        } else if (*argument == "--help") {
            written.help = true;
        } else if (*argument == "--fullscreen") {
            written.fullscreen = true;
        } else if (argument->starts_with('-')) {
            return usage_error(UsageErrorCode::unknown_argument,
                               std::format("unknown argument: {}", *argument));
        } else if (written.map) {
            return usage_error(
                UsageErrorCode::extra_argument,
                std::format("a second map file: {}, after {}", *argument, *written.map));
        } else {
            written.map = *argument;
        }
    }
    return written;
}

} // namespace

std::string_view usage() noexcept {
    return "usage: wulfhilde-viewer <map> [--content <dir>] [--camera <camera>] [--fullscreen]\n"
           "                        [--samples <n>]\n"
           "       wulfhilde-viewer --help\n"
           "\n"
           "The map-preview tool of the wulfhilde engine. It loads a map's file, ending "
           ".map.json,\n"
           "and draws the map in a window of 1280x720, from the eye of someone at its start\n"
           "position. It reports every problem it finds in the map instead, each as\n"
           "file:line:column. Closing the window quits.\n"
           "\n"
           "options:\n"
           "  --content <dir>    the content root that holds the map and its assets; by "
           "default,\n"
           "                     the map file's directory\n"
           "  --camera <sector>,<x>,<y>,<z>,<yaw>,<pitch>[,<roll>]\n"
           "                     draw from a camera instead: a sector's id, a point in its\n"
           "                     coordinates in metres, and a facing in degrees, with yaw and "
           "roll\n"
           "                     from -180 to 180, pitch from -90 to 90, and no roll by "
           "default\n"
           "  --fullscreen       cover the display instead of opening a window\n"
           "  --samples <n>      how many samples each pixel takes, to smooth edges: 1, 2, 4, "
           "8,\n"
           "                     16, 32 or 64, of those the GPU can take; 4 by default\n"
           "  --help             print this usage and exit\n";
}

std::expected<CommandLine, UsageError>
parse_command_line(std::span<const std::string_view> arguments) {
    const auto written = read_arguments(arguments);
    if (!written) {
        return std::unexpected(written.error());
    }
    CommandLine command_line{.help = written->help, .fullscreen = written->fullscreen};
    if (command_line.help) {
        return command_line;
    }

    if (written->camera) {
        auto camera = parse_camera(*written->camera);
        if (!camera) {
            return std::unexpected(std::move(camera).error());
        }
        command_line.camera = std::move(*camera);
    }
    if (written->samples) {
        const auto samples = parse_samples(*written->samples);
        if (!samples) {
            return std::unexpected(samples.error());
        }
        command_line.samples = *samples;
    }

    if (!written->map) {
        return usage_error(UsageErrorCode::missing_map, "no map file to open");
    }
    command_line.map = *written->map;
    const auto name = command_line.map.filename().string();
    if (name.size() <= wulfhilde::map_extension.size() ||
        !name.ends_with(wulfhilde::map_extension)) {
        return usage_error(UsageErrorCode::not_a_map_file,
                           std::format("{} isn't a map file, whose name ends in {}",
                                       command_line.map.string(), wulfhilde::map_extension));
    }

    // A map file in the working directory has no directory of its own written.
    command_line.content = written->content ? std::filesystem::path(*written->content)
                                            : command_line.map.parent_path();
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
