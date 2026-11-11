#include "image_comparison.hpp"

#include <wulfhilde/renderer/image.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace {

// An image of `count` pixels in one row, each of the given gray.
wulfhilde::Image gray_row(std::size_t count, std::uint8_t gray) {
    std::vector<std::uint8_t> pixels;
    for (std::size_t pixel = 0; pixel < count; ++pixel) {
        pixels.insert(pixels.end(), {gray, gray, gray, 255});
    }
    return {{.width = count, .height = 1}, pixels};
}

} // namespace

TEST_CASE("pixels within the tolerance of their reference don't differ", "[renderer]") {
    const auto comparison = compare_images(gray_row(4, 100), gray_row(4, 102), 2);

    CHECK(comparison.differing_pixels == 0);
    CHECK(comparison.largest_difference == 2);
}

TEST_CASE("a pixel with one channel beyond the tolerance differs, and shows red", "[renderer]") {
    const auto actual = gray_row(2, 90);
    std::vector<std::uint8_t> pixels(actual.pixels().begin(), actual.pixels().end());
    pixels.at(6) = 60; // The second pixel's blue.

    const auto comparison = compare_images({actual.extent(), pixels}, gray_row(2, 90), 2);

    CHECK(comparison.differing_pixels == 1);
    CHECK(comparison.largest_difference == 30);
    const auto difference = comparison.difference.pixels();
    CHECK(std::vector<std::uint8_t>(difference.begin(), difference.end()) ==
          std::vector<std::uint8_t>{30, 30, 30, 255, 255, 0, 0, 255});
}

TEST_CASE("an image matches a reference with fewer differing pixels than the cap", "[renderer]") {
    const auto directory = std::filesystem::path(WULFHILDE_RENDERER_SCRATCH) / "comparison";
    std::filesystem::remove_all(directory);
    const ImageReferences references{.references = directory, .failures = directory / "failures"};
    REQUIRE(write_png(directory / "gray.png", gray_row(10, 128)));
    const auto actual = gray_row(10, 128);
    std::vector<std::uint8_t> pixels(actual.pixels().begin(), actual.pixels().end());
    pixels.at(0) = 0;

    CHECK(!check_reference_image({actual.extent(), pixels}, "gray", references,
                                 {.channel = 2, .differing_share = 0.1}));
    CHECK(!std::filesystem::exists(directory / "failures"));
}

TEST_CASE("an image that differs from its reference leaves what it drew and where it differs",
          "[renderer]") {
    const auto directory = std::filesystem::path(WULFHILDE_RENDERER_SCRATCH) / "failing";
    std::filesystem::remove_all(directory);
    const ImageReferences references{.references = directory, .failures = directory / "failures"};
    REQUIRE(write_png(directory / "gray.png", gray_row(10, 128)));

    const auto failure = check_reference_image(gray_row(10, 64), "gray", references);

    CHECK(failure.has_value());
    const auto first_byte =
        read_png(directory / "failures" / "gray.actual.png")
            .transform([](const wulfhilde::Image& actual) { return actual.pixels().front(); });
    CHECK(first_byte == std::optional{std::uint8_t{64}});
    CHECK(std::filesystem::exists(directory / "failures" / "gray.difference.png"));
}

TEST_CASE("an image without a reference fails", "[renderer]") {
    const auto directory = std::filesystem::path(WULFHILDE_RENDERER_SCRATCH) / "missing";
    std::filesystem::remove_all(directory);
    const ImageReferences references{.references = directory, .failures = directory / "failures"};

    CHECK(check_reference_image(gray_row(2, 1), "nowhere", references).has_value());
}
