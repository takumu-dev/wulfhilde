#pragma once

#include <wulfhilde/assets/texture_image.hpp>

#include <cstdint>
#include <vector>

namespace wulfhilde::detail {

/// What a texture's texels hold, which decides how its mips are filtered.
enum class TexelMeaning : std::uint8_t {
    color,  ///< Colours in sRGB's transfer function, averaged in linear light.
    value,  ///< Linear values, averaged as they are.
    normal, ///< Unit vectors in tangent space, averaged and renormalised.
};

/// Builds a texture's mip chain on the CPU, so that every GPU samples the same texels.
///
/// \param image The full image, which is neither zero texels wide nor high.
/// \param meaning What its texels hold.
/// \return Every level, from the full image down to one texel.
[[nodiscard]] std::vector<MipLevel> build_mip_chain(MipLevel image, TexelMeaning meaning);

} // namespace wulfhilde::detail
