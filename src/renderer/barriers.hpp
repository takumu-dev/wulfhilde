// The synchronisation the renderer records between the steps of its work.
#pragma once

#include "vulkan.hpp"

namespace wulfhilde::detail {

/// One side of a barrier: the stages and accesses that come before it or after it.
struct BarrierScope {
    VkPipelineStageFlags2 stages = VK_PIPELINE_STAGE_2_NONE;
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
};

/// The side of a barrier that waits for nothing, as before an image's first use.
inline constexpr BarrierScope no_earlier_work{};

/// Records a barrier that moves the whole of a one-level image from one layout to another, after
/// the work in `before` and ahead of the work in `after`. The aspects are those of its format.
inline void record_image_barrier(VkCommandBuffer commands, VkImage image, VkImageLayout from,
                                 VkImageLayout to, BarrierScope before, BarrierScope after,
                                 VkImageAspectFlags aspects = VK_IMAGE_ASPECT_COLOR_BIT) {
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = before.stages,
        .srcAccessMask = before.access,
        .dstStageMask = after.stages,
        .dstAccessMask = after.access,
        .oldLayout = from,
        .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = aspects,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(commands, &dependency);
}

/// Records a barrier over all memory, after the work in `before` and ahead of the work in `after`.
inline void record_memory_barrier(VkCommandBuffer commands, BarrierScope before,
                                  BarrierScope after) {
    const VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = before.stages,
        .srcAccessMask = before.access,
        .dstStageMask = after.stages,
        .dstAccessMask = after.access,
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(commands, &dependency);
}

} // namespace wulfhilde::detail
