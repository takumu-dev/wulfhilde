/// \file
/// An image that materials read as a texture, decoded and ready for the GPU.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wulfhilde {

/// How a texture image's texels encode its values, which the property that reads it decides,
/// never the image's own metadata.
enum class ColorSpace : std::uint8_t {
    srgb,   ///< Red, green and blue in sRGB's transfer function, and a linear alpha: colours.
    linear, ///< Every channel linear: single values and normals.
};

/// One level of a texture image's mip chain.
///
/// Its rows run from the top of the image down, as a PNG stores them, and each row's texels from
/// left to right. Each texel is four bytes, red, green, blue and alpha, and the rows are packed.
struct MipLevel {
    /// The level's width in texels.
    std::size_t width = 0;

    /// The level's height in texels.
    std::size_t height = 0;

    /// The level's texels, four bytes each, row after row.
    std::vector<std::uint8_t> texels{};

    /// Compares two levels exactly.
    bool operator==(const MipLevel&) const = default;
};

/// An image that materials read as a texture, in one way, as its full mip chain.
///
/// An image read in two ways, such as one PNG read as a colour and as a single value, is two
/// texture images, since each way filters its mips differently.
struct TextureImage {
    /// The image's asset path.
    std::string path{};

    /// How the texels encode their values.
    ColorSpace color_space{};

    /// The mip chain, from the full image down to one texel, each level half the size of the one
    /// before, rounded down, and never less than one texel on a side.
    std::vector<MipLevel> levels{};

    /// Compares two images exactly.
    bool operator==(const TextureImage&) const = default;
};

} // namespace wulfhilde
