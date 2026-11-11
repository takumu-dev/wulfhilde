#include <wulfhilde/renderer/image.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

TEST_CASE("an image keeps its size and its pixels, row after row from the top", "[renderer]") {
    const std::vector<std::uint8_t> pixels{
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, //
        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    };

    const wulfhilde::Image image({.width = 3, .height = 2}, pixels);

    CHECK(image.extent() == wulfhilde::Extent{.width = 3, .height = 2});
    CHECK(std::vector<std::uint8_t>(image.pixels().begin(), image.pixels().end()) == pixels);
}
