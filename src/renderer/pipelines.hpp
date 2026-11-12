// Pipelines made from the SPIR-V modules that the build embeds, each over the device's one pipeline
// layout.
#pragma once

#include "device.hpp"
#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/renderer/device_error.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace wulfhilde::detail {

/// Creates a compute pipeline from an entry point of a SPIR-V module, with the values of its
/// 32-bit specialisation constants, the one with ID `i` taking value `i`.
[[nodiscard]] std::expected<PipelineHandle, DeviceError>
create_compute_pipeline(const Device& device, std::span<const std::uint32_t> spirv,
                        std::string_view entry_point,
                        std::span<const std::uint32_t> specialization_constants = {});

/// What a graphics pipeline that draws triangles into one colour target, and perhaps a depth and
/// stencil target, is made from.
struct GraphicsPipelineDescription {
    /// The SPIR-V module that holds both entry points.
    std::span<const std::uint32_t> spirv;
    std::string_view vertex_entry_point;
    std::string_view fragment_entry_point;
    /// The values of the module's 32-bit specialisation constants, the one with ID `i` taking
    /// value `i`, which both stages see.
    std::span<const std::uint32_t> specialization_constants;
    /// The format of the colour target, drawn into by dynamic rendering.
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    /// The format of the depth and stencil target, or none for a pipeline without depth. Depth is
    /// reversed, so a fragment is drawn where it is greater than what the target holds, and
    /// written.
    VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
    /// How many samples each pixel of the targets holds.
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    /// Whether triangles that face away are drawn, which a surface seen from behind isn't. A
    /// triangle faces the eye when its corners run anticlockwise in the image.
    VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
};

/// Creates a graphics pipeline with no vertex input and no blending, whose viewport and scissor
/// are set when it draws.
[[nodiscard]] std::expected<PipelineHandle, DeviceError>
create_graphics_pipeline(const Device& device, const GraphicsPipelineDescription& description);

} // namespace wulfhilde::detail
