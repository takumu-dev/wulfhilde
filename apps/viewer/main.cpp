#include "command_line.hpp"
#include <wulfhilde/core/narrow.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main(int argc, char** argv) {
    const auto arguments = std::span(argv, wulfhilde::narrow<std::size_t>(argc)) |
                           std::views::drop(1) | std::ranges::to<std::vector<std::string_view>>();

    const auto command_line = viewer::parse_command_line(arguments);
    if (!command_line) {
        std::print(stderr, "wulfhilde-viewer: {}\n\n{}", command_line.error().message,
                   viewer::usage());
        return EXIT_FAILURE;
    }

    if (command_line->help) {
        std::print("{}", viewer::usage());
        return EXIT_SUCCESS;
    }

    // With no map to open, there is nothing to do but show how the viewer is used.
    std::print(stderr, "{}", viewer::usage());
    return EXIT_FAILURE;
}
