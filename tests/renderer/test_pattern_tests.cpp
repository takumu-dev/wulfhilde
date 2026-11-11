#include "test_pattern.hpp"

#include "gpu_test.hpp"
#include <wulfhilde/renderer/image.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

#include "test_pattern_spirv.h"

// The pixel at column `x` and row `y` of an image.
std::vector<std::uint8_t> pixel(const wulfhilde::Image& image, std::size_t x, std::size_t y) {
    const auto start = ((y * image.extent().width) + x) * wulfhilde::Image::bytes_per_pixel;
    const auto pixels = image.pixels().subspan(start, wulfhilde::Image::bytes_per_pixel);
    return {pixels.begin(), pixels.end()};
}

} // namespace

TEST_CASE_METHOD(GpuTest, "the test pattern samples a texture through the bindless set",
                 "[renderer]") {
    // Under Vulkan 1.3, pipelines take their SPIR-V through shader modules.
    const auto vulkan_minor_version = GENERATE(4U, 3U);
    CAPTURE(vulkan_minor_version);
    const auto image =
        test_pattern::draw(test_pattern_spirv, {.width = 64, .height = 64}, vulkan_minor_version);

    REQUIRE(image.extent() == wulfhilde::Extent{.width = 64, .height = 64});
    // The top row of texels is drawn at full brightness, as the texture holds it.
    CHECK(pixel(image, 0, 0) == test_pattern::texel(0, 0));
    CHECK(pixel(image, 63, 15) == test_pattern::texel(3, 0));
    // The bottom row has a quarter of the light, which sRGB encodes as about half the value.
    const auto bottom_right = pixel(image, 63, 63);
    const auto texel = test_pattern::texel(3, 3);
    CHECK(bottom_right.at(0) > texel.at(0) * 4 / 10);
    CHECK(bottom_right.at(0) < texel.at(0) * 6 / 10);
    CHECK(bottom_right.at(3) == 255);
}
