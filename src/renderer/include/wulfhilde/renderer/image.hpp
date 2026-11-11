/// \file
/// An image the renderer has drawn, read back to the CPU.

#pragma once

#include <wulfhilde/renderer/extent.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace wulfhilde {

/// An image the renderer has drawn, as 8-bit sRGB pixels in memory.
///
/// Its rows run from the top of the image down, and each row's pixels from left to right. Each
/// pixel is four bytes: red, green and blue, encoded with the sRGB transfer function, then a
/// linear alpha. The rows are packed, with nothing between them.
class Image {
  public:
    /// The number of bytes in one pixel.
    static constexpr std::size_t bytes_per_pixel = 4;

    /// Makes an image of the given size from its pixels.
    ///
    /// \param extent The image's size, neither side of it zero.
    /// \param pixels The image's pixels, `bytes_per_pixel` for each of them, row after row.
    Image(Extent extent, std::vector<std::uint8_t> pixels);

    /// Returns the image's size in pixels.
    [[nodiscard]] Extent extent() const noexcept { return extent_; }

    /// Returns the image's pixels, `bytes_per_pixel` for each of them, row after row.
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return pixels_; }

  private:
    /// The image's size in pixels.
    Extent extent_;

    /// The image's pixels, row after row.
    std::vector<std::uint8_t> pixels_;
};

} // namespace wulfhilde
