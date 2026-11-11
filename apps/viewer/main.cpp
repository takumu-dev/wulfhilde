#include "command_line.hpp"
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/core/narrow.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

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
        std::print(stderr, "wulfhilde-viewer: {}\n", root.error());
        return EXIT_FAILURE;
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

    // Until the viewer draws the map, loading it is all there is to do.
    std::print("{}: loaded\n", command_line->map.generic_string());
    return EXIT_SUCCESS;
}
