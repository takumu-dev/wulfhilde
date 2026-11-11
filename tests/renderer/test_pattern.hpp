// The fixed pattern that proves the graphics path: a 4 by 4 texture of distinct colours, drawn over
// a whole image through the bindless set, each row of texels darker than the one above.
#pragma once

#include "gpu_test.hpp"
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/testing/test_device.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace test_pattern {

// The twin of Slang's DescriptorHandle<T>.
struct DescriptorHandle {
    std::uint32_t index = 0;
    std::uint32_t unused = 0;
};

struct PatternConstants {
    DescriptorHandle texture;
    DescriptorHandle sampler;
};

#include "test_pattern_layout.hpp" // IWYU pragma: keep

// The texture's texel in column `x` and row `y` of its 4 by 4: red rising to the right, green
// rising down, and blue on alternate texels.
inline std::vector<std::uint8_t> texel(std::size_t x, std::size_t y) {
    return {
        wulfhilde::narrow<std::uint8_t>((64 * x) + 63),
        wulfhilde::narrow<std::uint8_t>((64 * y) + 63),
        wulfhilde::narrow<std::uint8_t>((x + y) % 2 == 0 ? 255 : 0),
        255,
    };
}

inline wulfhilde::Image texture() {
    std::vector<std::uint8_t> pixels;
    for (std::size_t y = 0; y < 4; ++y) {
        for (std::size_t x = 0; x < 4; ++x) {
            const auto color = texel(x, y);
            pixels.insert(pixels.end(), color.begin(), color.end());
        }
    }
    return {{.width = 4, .height = 4}, pixels};
}

// Draws the pattern with the given test shader's SPIR-V into an image of the given size, on a
// device taken to have Vulkan 1.4 or the given older version.
inline wulfhilde::Image draw(std::span<const std::uint32_t> spirv, wulfhilde::Extent extent,
                             std::uint32_t vulkan_minor_version = 4) {
    auto device = wulfhilde::detail::TestDevice::create(gpu_test_settings, vulkan_minor_version);
    require_value(device);
    const auto texture_index = device->add_texture(texture());
    require_value(texture_index);
    const auto sampler_index = device->add_nearest_sampler();
    require_value(sampler_index);
    const PatternConstants constants{
        .texture = {.index = *texture_index},
        .sampler = {.index = *sampler_index},
    };
    // The shader's specialisation constant is the texture's number of rows.
    const std::array<std::uint32_t, 1> texel_rows{4};
    auto image = device->draw({
        .spirv = spirv,
        .vertex_entry_point = "cover",
        .fragment_entry_point = "paint",
        .specialization_constants = texel_rows,
        .push_constants = std::as_bytes(std::span(&constants, 1)),
        .vertex_count = 3,
        .extent = extent,
    });
    require_value(image);
    return *std::move(image);
}

} // namespace test_pattern
