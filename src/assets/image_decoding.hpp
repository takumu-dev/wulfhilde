#pragma once

#include <wulfhilde/assets/asset_problem.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

namespace wulfhilde::detail {

/// The largest width or height of an image, in pixels: the size Vulkan 1.3 guarantees every device
/// can sample.
inline constexpr std::size_t largest_image_side = 4'096;

/// A PNG image, decoded into 8-bit red, green, blue and alpha.
struct DecodedImage {
    /// The width in pixels.
    std::size_t width = 0;

    /// The height in pixels.
    std::size_t height = 0;

    /// The pixels, four bytes each, row after row from the top. A grey image's grey fills red,
    /// green and blue, and an image without alpha has an alpha of 255.
    std::vector<std::uint8_t> pixels;

    /// Whether the image is grey, with or without alpha.
    bool grey = false;

    /// Whether the image has an alpha channel, which a palette or a transparent colour gives too.
    bool alpha = false;
};

/// Decodes a PNG image of palette, grey or RGB, at most `largest_image_side` pixels on a side,
/// into 8 bits per channel.
///
/// \param bytes The PNG file's bytes.
/// \return The image, or the problem of the whole file that keeps it from being one, which names
///         no file.
[[nodiscard]] std::expected<DecodedImage, AssetProblem> decode_png(std::string_view bytes);

} // namespace wulfhilde::detail
