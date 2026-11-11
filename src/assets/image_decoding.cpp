#include "image_decoding.hpp"

#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/core/narrow.hpp>

#include <lodepng.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <string_view>
#include <vector>

namespace wulfhilde::detail {

std::expected<DecodedImage, AssetProblem> decode_png(std::string_view bytes) {
    const std::vector<unsigned char> data(bytes.begin(), bytes.end());
    lodepng::State state;
    unsigned width = 0;
    unsigned height = 0;
    // The header alone, so that an image too large to use is never decoded.
    if (const auto error = lodepng_inspect(&width, &height, &state, data.data(), data.size());
        error != 0) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::invalid_image,
            .message = std::format("the file isn't a PNG image that can be decoded: {}",
                                   lodepng_error_text(error)),
        });
    }
    if (width > largest_image_side || height > largest_image_side) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::image_too_large,
            .message = std::format("the image is {} by {} pixels, but an image may be at most {} "
                                   "pixels on a side",
                                   width, height, largest_image_side),
        });
    }
    // The decoder narrows 16-bit channels to 8, widens fewer bits to 8, and expands palettes and
    // grey.
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;
    std::vector<unsigned char> pixels;
    if (const auto error = lodepng::decode(pixels, width, height, state, data); error != 0) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::invalid_image,
            .message = std::format("the file isn't a PNG image that can be decoded: {}",
                                   lodepng_error_text(error)),
        });
    }
    const auto& png = state.info_png.color;
    return DecodedImage{
        .width = narrow<std::size_t>(width),
        .height = narrow<std::size_t>(height),
        .pixels = std::vector<std::uint8_t>(pixels.begin(), pixels.end()),
        .grey = lodepng_is_greyscale_type(&png) != 0,
        .alpha = lodepng_can_have_alpha(&png) != 0,
    };
}

} // namespace wulfhilde::detail
