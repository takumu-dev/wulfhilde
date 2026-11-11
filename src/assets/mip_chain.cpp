#include "mip_chain.hpp"

#include <wulfhilde/assets/texture_image.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <utility>
#include <vector>

namespace wulfhilde::detail {
namespace {

/// A texel's four channels, decoded: linear light for a colour, the value for a value, and the
/// vector's components from -1 to 1 for a normal, with a linear alpha in each.
using Texel = std::array<float, 4>;

/// The largest value of an 8-bit channel.
constexpr float channel_max = 255.0F;

/// Decodes a channel encoded with sRGB's transfer function into linear light.
float srgb_to_linear(float encoded) {
    return encoded <= 0.04045F ? encoded / 12.92F : std::pow((encoded + 0.055F) / 1.055F, 2.4F);
}

/// Encodes linear light with sRGB's transfer function.
float linear_to_srgb(float linear) {
    return linear <= 0.0031308F ? linear * 12.92F
                                : (1.055F * std::pow(linear, 1.0F / 2.4F)) - 0.055F;
}

/// Decodes one texel of an 8-bit level.
///
/// \param texels The level's texels, four bytes each.
/// \param index The texel's index.
/// \param meaning What the texels hold.
Texel decode(const std::vector<std::uint8_t>& texels, std::size_t index, TexelMeaning meaning) {
    Texel texel{};
    for (std::size_t c = 0; auto& channel : texel) {
        channel = static_cast<float>(texels.at((index * 4) + c++)) / channel_max;
    }
    if (meaning == TexelMeaning::color) {
        for (auto& channel : texel | std::views::take(3)) {
            channel = srgb_to_linear(channel);
        }
    } else if (meaning == TexelMeaning::normal) {
        for (auto& channel : texel | std::views::take(3)) {
            channel = (channel * 2.0F) - 1.0F;
        }
    }
    return texel;
}

/// Encodes one texel into 8 bits per channel, rounding to the nearest step.
std::array<std::uint8_t, 4> encode(Texel texel, TexelMeaning meaning) {
    if (meaning == TexelMeaning::color) {
        for (auto& channel : texel | std::views::take(3)) {
            channel = linear_to_srgb(channel);
        }
    } else if (meaning == TexelMeaning::normal) {
        for (auto& channel : texel | std::views::take(3)) {
            channel = (channel + 1.0F) * 0.5F;
        }
    }
    std::array<std::uint8_t, 4> bytes{};
    for (auto&& [byte, channel] : std::views::zip(bytes, texel)) {
        byte = narrow<std::uint8_t>(std::lround(std::clamp(channel, 0.0F, 1.0F) * channel_max));
    }
    return bytes;
}

/// The texels of a source row or column that one texel of a smaller level covers, with the share
/// of it that each covers.
struct Footprint {
    /// The first source texel it covers.
    std::size_t first = 0;

    /// The share of each source texel from the first on, which add up to 1.
    std::array<float, 3> weights{};

    /// The count of source texels it covers.
    std::size_t count = 0;
};

/// Works out the footprint of each texel along one axis, when `source` texels shrink to
/// `destination`, which is at least half of it, rounded down, so that each covers two texels or a
/// little more, and an odd level is filtered without shifting.
std::vector<Footprint> footprints(std::size_t source, std::size_t destination) {
    const auto scale = static_cast<double>(source) / static_cast<double>(destination);
    std::vector<Footprint> result;
    result.reserve(destination);
    for (std::size_t i = 0; i < destination; ++i) {
        const auto low = static_cast<double>(i) * scale;
        const auto high = low + scale;
        Footprint footprint{.first = static_cast<std::size_t>(low)};
        for (auto texel = footprint.first; static_cast<double>(texel) < high && texel < source;
             ++texel) {
            const auto covered = std::min(high, static_cast<double>(texel + 1)) -
                                 std::max(low, static_cast<double>(texel));
            // A texel of a level covers at most three of the level before.
            footprint.weights.at(footprint.count++) = static_cast<float>(covered / scale);
        }
        result.push_back(footprint);
    }
    return result;
}

/// A level filtered from the one before it, decoded, so that rounding never builds up down the
/// chain.
struct DecodedLevel {
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<Texel> texels{};
};

/// Filters a level down to the next, half its size, rounded down, but at least one texel.
///
/// \param width The level's width.
/// \param height The level's height.
/// \param texel_at Returns the level's texel at an index, decoded.
/// \param meaning What the texels hold.
template <typename TexelAt>
DecodedLevel shrink(std::size_t width, std::size_t height, TexelAt texel_at, TexelMeaning meaning) {
    DecodedLevel next{
        .width = std::max<std::size_t>(width / 2, 1),
        .height = std::max<std::size_t>(height / 2, 1),
    };
    const auto across = footprints(width, next.width);
    const auto down = footprints(height, next.height);
    next.texels.reserve(next.width * next.height);
    for (const auto& row : down) {
        for (const auto& column : across) {
            Texel sum{};
            for (std::size_t y = 0; y < row.count; ++y) {
                for (std::size_t x = 0; x < column.count; ++x) {
                    const auto texel = texel_at(((row.first + y) * width) + column.first + x);
                    const auto weight = row.weights.at(y) * column.weights.at(x);
                    for (auto&& [total, channel] : std::views::zip(sum, texel)) {
                        total += weight * channel;
                    }
                }
            }
            if (meaning == TexelMeaning::normal) {
                // Averaged directions are shorter than one; normals that cancel point straight
                // out of the surface.
                const auto [nx, ny, nz, alpha] = sum;
                const auto length = std::sqrt((nx * nx) + (ny * ny) + (nz * nz));
                sum = length > 0.0F ? Texel{nx / length, ny / length, nz / length, alpha}
                                    : Texel{0.0F, 0.0F, 1.0F, alpha};
            }
            next.texels.push_back(sum);
        }
    }
    return next;
}

/// Encodes a decoded level into 8 bits per channel.
MipLevel encode_level(const DecodedLevel& level, TexelMeaning meaning) {
    MipLevel encoded{.width = level.width, .height = level.height};
    encoded.texels.reserve(level.texels.size() * 4);
    for (const auto& texel : level.texels) {
        std::ranges::copy(encode(texel, meaning), std::back_inserter(encoded.texels));
    }
    return encoded;
}

} // namespace

std::vector<MipLevel> build_mip_chain(MipLevel image, TexelMeaning meaning) {
    WULFHILDE_DEBUG_ASSERT(image.width > 0 && image.height > 0 &&
                               image.texels.size() == image.width * image.height * 4,
                           "a mip chain is built from an image with four bytes for each texel");
    std::vector<MipLevel> levels;
    if (image.width == 1 && image.height == 1) {
        levels.push_back(std::move(image));
        return levels;
    }
    // The full image is decoded a texel at a time, as it is read, since decoded it would be four
    // times its size.
    auto level = shrink(
        image.width, image.height,
        [&image, meaning](std::size_t index) { return decode(image.texels, index, meaning); },
        meaning);
    levels.push_back(std::move(image));
    levels.push_back(encode_level(level, meaning));
    while (level.width > 1 || level.height > 1) {
        level = shrink(
            level.width, level.height,
            [&level](std::size_t index) { return level.texels.at(index); }, meaning);
        levels.push_back(encode_level(level, meaning));
    }
    return levels;
}

} // namespace wulfhilde::detail
