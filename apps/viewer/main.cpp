#include "cameras.hpp"
#include "command_line.hpp"
#include "controller.hpp"
#include "input.hpp"
#include "pacing.hpp"
#include "tick_clock.hpp"
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/log_sink.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/platform/platform.hpp>
#include <wulfhilde/platform/platform_error.hpp>
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/world/map.hpp>

#include <chrono>
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
#include <vector>

namespace {

// Reports an error of the engine's, or of the system's, and fails.
template <typename Error> int fail(const Error& error) {
    std::print(stderr, "wulfhilde-viewer: {}\n", error);
    return EXIT_FAILURE;
}

// Gives the warning of a body that doesn't fit at the start position, if the controller's is one.
void warn_if_unfit(const wulfhilde::Map& map, const viewer::Controller& controller) {
    if (!controller.body()) {
        wulfhilde::log(wulfhilde::LogSeverity::warning, viewer::unfit_start_warning(map));
    }
}

// Does what the player asks in a frame, with the ticks due in it, or fails if the mouse can't be
// taken or let go.
std::expected<void, wulfhilde::PlatformError> play(const viewer::FrameInput& frame,
                                                   std::size_t ticks, const wulfhilde::Map& map,
                                                   wulfhilde::Window& window,
                                                   viewer::Controller& controller) {
    if (frame.capture_mouse) {
        if (auto captured = window.set_relative_mouse_mode(*frame.capture_mouse); !captured) {
            return captured;
        }
    }
    controller.look(frame.yaw, frame.pitch);
    if (frame.return_to_start) {
        controller.return_to_start(map);
        warn_if_unfit(map, controller);
    }
    for (std::size_t tick = 0; tick < ticks; ++tick) {
        controller.tick(map, frame.actions);
    }
    if (frame.print_eye) {
        std::print("--camera {}\n", viewer::camera_option(map, controller.eye(map)));
        std::fflush(stdout);
    }
    return {};
}

// Lets the player walk the map in the window until it is closed, then succeeds, or fails if a frame
// can't be drawn.
//
// Each frame is shown at the display's next refresh, which paces the loop. A minimised window has
// nothing to show one in, so then the viewer rests until something happens instead. The ticks due
// over a long rest are capped, as over any long frame.
int show(wulfhilde::Platform& platform, wulfhilde::Window& window, wulfhilde::Renderer& renderer,
         const wulfhilde::Map& map, viewer::Controller controller, wulfhilde::Angle sensitivity) {
    if (auto captured = window.set_relative_mouse_mode(true); !captured) {
        return fail(captured.error());
    }
    viewer::Pacing pacing;
    viewer::Input input(sensitivity);
    viewer::TickClock clock;
    auto pace = viewer::Pace::draw;
    auto last = std::chrono::steady_clock::now();
    while (true) {
        const auto events =
            pace == viewer::Pace::rest ? platform.wait_events() : platform.poll_events();
        pace = pacing.follow(events);
        if (pace == viewer::Pace::quit) {
            return EXIT_SUCCESS;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto ticks = clock.advance(now - last);
        last = now;
        if (auto played = play(input.follow(events), ticks, map, window, controller); !played) {
            return fail(played.error());
        }
        if (pace == viewer::Pace::draw) {
            if (auto drawn = renderer.draw(viewer::camera_at(controller.eye(map))); !drawn) {
                return fail(drawn.error());
            }
        }
    }
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

    // A camera that the command line names is placed once there is a map to place it in, and is a
    // viewpoint without a body.
    auto controller = viewer::Controller::at_start(*map);
    if (const auto& option = command_line->camera) {
        const auto camera = viewer::camera_from_option(*map, *option);
        if (!camera) {
            std::print(stderr, "wulfhilde-viewer: {}\n\n{}", camera.error(), viewer::usage());
            return EXIT_FAILURE;
        }
        controller = viewer::Controller::at_camera(camera->location, option->yaw, option->pitch,
                                                   option->roll);
    } else {
        warn_if_unfit(*map, controller);
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
    auto renderer = wulfhilde::Renderer::create(*window, {
                                                             .sample_count = command_line->samples,
                                                             .anisotropy = command_line->anisotropy,
                                                         });
    if (!renderer) {
        return fail(renderer.error());
    }
    if (auto set = renderer->set_map(*map, *assets); !set) {
        return fail(set.error());
    }

    return show(*platform, *window, *renderer, *map, controller, command_line->sensitivity);
}
