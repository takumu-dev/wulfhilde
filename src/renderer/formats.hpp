// The formats of the renderer's targets, which every pipeline that draws into them is made for.
#pragma once

#include "vulkan.hpp"

namespace wulfhilde::detail {

/// The scene's colour, in linear light, with the range that light in physical units needs.
inline constexpr VkFormat scene_color_format = VK_FORMAT_R16G16B16A16_SFLOAT;

/// The scene's depth, reversed and in float for precision all the way out to an infinite far
/// plane, with the stencil that confines views to their portals' openings.
inline constexpr VkFormat scene_depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;

/// The image the tone-mapping pass writes, in 8-bit sRGB.
inline constexpr VkFormat output_format = VK_FORMAT_R8G8B8A8_SRGB;

/// The aspects of an image of one of the renderer's formats: depth and stencil for the scene's
/// depth, and colour for any other.
[[nodiscard]] constexpr VkImageAspectFlags aspects_of(VkFormat format) noexcept {
    return format == scene_depth_format
               ? flags_of(VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_STENCIL_BIT)
               : flags_of(VK_IMAGE_ASPECT_COLOR_BIT);
}

} // namespace wulfhilde::detail
