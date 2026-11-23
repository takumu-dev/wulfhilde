#include "cameras.hpp"
#include "command_line.hpp"
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/platform/event.hpp>
#include <wulfhilde/platform/platform.hpp>
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/renderer.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <expected>
#include <format>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

// Reports an error of the engine's, or of the system's, and fails.
template <typename Error> int fail(const Error& error) {
    std::print(stderr, "wulfhilde-viewer: {}\n", error);
    return EXIT_FAILURE;
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
    const auto arguments = std::span(argv, wulfhilde::narrow<std::size_t>(argc)) |
                           std::views::drop(1) | std::ranges::to<std::vector<std::string_view>>();

    // With nothing to do, there is nothing to do but show how the viewer is used.
    if (arguments.empty()) {
        std::print(stderr, "{}", viewer::usage());
        return EXIT_FAILURE;
    }

    const auto command_line = viewer::parse_command_line(arguments);
    if (!command_line) {
        std::print(stderr, "wulfhilde-viewer: {}\n\n{}", command_line.error(), viewer::usage());
        return EXIT_FAILURE;
    }

    if (command_line->help) {
        std::print("{}", viewer::usage());
        return EXIT_SUCCESS;
    }

    const auto root = wulfhilde::ContentRoot::open(command_line->content);
    if (!root) {
        return fail(root.error());
    }

    auto map = wulfhilde::load_map(*root, command_line->map_asset_path);
    if (!map) {
        // A problem names its file within the content root, which becomes a path from where the
        // viewer was started, so that a terminal or an editor finds it.
        auto error = std::move(map.error());
        error.file = (command_line->content / error.file).lexically_normal().generic_string();
        std::print(stderr, "{}\n", error);
        return EXIT_FAILURE;
    }

    auto assets = wulfhilde::load_map_assets(*root, *map);
    if (!assets) {
        // As a map's problems do, each names its file as a path from where the viewer was started,
        // and a problem in the map, whose file the map doesn't know, names the map's.
        auto error = std::move(assets.error());
        for (auto& problem : error.problems) {
            problem.file =
                problem.file.empty()
                    ? command_line->map.generic_string()
                    : (command_line->content / problem.file).lexically_normal().generic_string();
        }
        std::print(stderr, "{}\n", error);
        return EXIT_FAILURE;
    }

    // A camera that the command line names is placed once there is a map to place it in.
    const auto camera =
        command_line->camera
            ? viewer::camera_from_option(*map, *command_line->camera)
            : std::expected<wulfhilde::Camera, viewer::UsageError>(viewer::camera_at_start(*map));
    if (!camera) {
        std::print(stderr, "wulfhilde-viewer: {}\n\n{}", camera.error(), viewer::usage());
        return EXIT_FAILURE;
    }

    auto platform = wulfhilde::Platform::create();
    if (!platform) {
        return fail(platform.error());
    }
    auto window = wulfhilde::Window::create(
        *platform, {
                       .title = std::format("{} - wulfhilde-viewer",
                                            command_line->map.filename().generic_string()),
                       .size = {.width = 1280, .height = 720},
                       .fullscreen = command_line->fullscreen,
                   });
    if (!window) {
        return fail(window.error());
    }
    auto renderer = wulfhilde::Renderer::create(*window, {.sample_count = command_line->samples});
    if (!renderer) {
        return fail(renderer.error());
    }
    if (auto set = renderer->set_map(*map, *assets); !set) {
        return fail(set.error());
    }

    // Each frame is shown at the display's next refresh, which paces the loop, except while the
    // window has no area to show one, such as when it is minimised.
    while (true) {
        const auto events = platform->poll_events();
        if (std::ranges::any_of(events, [](const wulfhilde::Event& event) {
                return std::holds_alternative<wulfhilde::WindowClosed>(event);
            })) {
            return EXIT_SUCCESS;
        }
        if (auto drawn = renderer->draw(*camera); !drawn) {
            return fail(drawn.error());
        }
    }
}
