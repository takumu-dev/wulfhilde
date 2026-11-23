#include "presentation.hpp"

#include "device.hpp"
#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/platform/vulkan_surface.hpp>
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

std::expected<VkSurfaceCapabilitiesKHR, DeviceError> capabilities_of(const Device& device) {
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization): Vulkan fills it in
    VkSurfaceCapabilitiesKHR capabilities{};
    if (const auto error = failure(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                                       device.physical_device(), device.surface(), &capabilities),
                                   "finding what the window's surface takes")) {
        return std::unexpected(*error);
    }
    return capabilities;
}

// The size of the images the surface takes now: its own, or the window's pixels where it leaves
// the size to the swapchain, as Wayland's does.
Extent extent_for(const VkSurfaceCapabilitiesKHR& capabilities, const Window& window) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return {
            .width = capabilities.currentExtent.width,
            .height = capabilities.currentExtent.height,
        };
    }
    const auto pixels = pixel_size(window);
    return {
        .width = std::clamp<std::size_t>(pixels.width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width),
        .height = std::clamp<std::size_t>(pixels.height, capabilities.minImageExtent.height,
                                          capabilities.maxImageExtent.height),
    };
}

// The frame is opaque, and a surface that blends with what lies behind the window is told so in
// whichever way it understands.
VkCompositeAlphaFlagBitsKHR composite_alpha(VkCompositeAlphaFlagsKHR supported) {
    constexpr std::array preferred{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
    };
    for (const auto bit : preferred) {
        if ((supported & flags_of(bit)) != 0) {
            return bit;
        }
    }
    return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
}

std::expected<SemaphoreHandle, DeviceError> create_semaphore(VkDevice device) {
    const VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateSemaphore(device, &info, nullptr, &semaphore),
                                   "creating a semaphore")) {
        return std::unexpected(*error);
    }
    return SemaphoreHandle(device, semaphore);
}

std::expected<ImageViewHandle, DeviceError> create_view(VkDevice device, VkImage image,
                                                        VkFormat format) {
    const VkImageViewCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkImageView view = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateImageView(device, &info, nullptr, &view),
                                   "creating a view of a window's image")) {
        return std::unexpected(*error);
    }
    return ImageViewHandle(device, view);
}

} // namespace

// A swapchain of one size, with a view of each of its images. Each image has a semaphore of its
// own that says it is rendered, since the present that waits on one may not have finished with it
// until that image is acquired again.
struct Presentation::Swapchain {
    SwapchainHandle swapchain;
    Extent extent;
    std::vector<VkImage> images;
    std::vector<ImageViewHandle> views;
    std::vector<SemaphoreHandle> rendered;
    SemaphoreHandle acquired;
};

std::expected<std::unique_ptr<Presentation>, DeviceError>
Presentation::create(const Device& device, const Window& window) {
    std::uint32_t count = 0;
    if (const auto error = failure(vkGetPhysicalDeviceSurfaceFormatsKHR(
                                       device.physical_device(), device.surface(), &count, nullptr),
                                   "listing the window's formats")) {
        return std::unexpected(*error);
    }
    std::vector<VkSurfaceFormatKHR> formats(count);
    if (const auto error =
            failure(vkGetPhysicalDeviceSurfaceFormatsKHR(device.physical_device(), device.surface(),
                                                         &count, formats.data()),
                    "listing the window's formats")) {
        return std::unexpected(*error);
    }
    // A surface that takes no sRGB format, such as a headless one, still shows 8-bit colours in
    // sRGB's colour space.
    constexpr std::array preferred{
        WindowFormat{.format = VK_FORMAT_B8G8R8A8_SRGB},
        WindowFormat{.format = VK_FORMAT_R8G8B8A8_SRGB},
        WindowFormat{.format = VK_FORMAT_B8G8R8A8_UNORM, .encodes_srgb = true},
        WindowFormat{.format = VK_FORMAT_R8G8B8A8_UNORM, .encodes_srgb = true},
    };
    for (const auto& wanted : preferred) {
        if (std::ranges::any_of(formats, [&wanted](const VkSurfaceFormatKHR& format) {
                return format.format == wanted.format &&
                       format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            })) {
            return std::make_unique<Presentation>(Key{}, window, wanted);
        }
    }
    return std::unexpected(DeviceError{
        .code = DeviceErrorCode::device_failure,
        .message = "the window takes no 8-bit format in sRGB's colour space",
    });
}

Presentation::Presentation(Key /*key*/, const Window& window, WindowFormat format) noexcept
    : window_(window), format_(format) {}

Presentation::~Presentation() = default;

std::expected<std::optional<Frame>, DeviceError> Presentation::begin_frame(Device& device) {
    const auto capabilities = capabilities_of(device);
    if (!capabilities) {
        return std::unexpected(capabilities.error());
    }
    const auto extent = extent_for(*capabilities, window_);
    if (extent.width == 0 || extent.height == 0) {
        return std::nullopt;
    }
    if (!swapchain_ || stale_ || swapchain_->extent != extent) {
        if (auto remade = remake_swapchain(device, extent); !remade) {
            return std::unexpected(std::move(remade).error());
        }
    }

    std::uint32_t index = 0;
    const VkResult acquired = vkAcquireNextImageKHR(
        device.get(), swapchain_->swapchain.get(), std::numeric_limits<std::uint64_t>::max(),
        swapchain_->acquired.get(), VK_NULL_HANDLE, &index);
    // A swapchain the surface has outgrown is made anew for the next frame, and this one is
    // skipped. One that merely no longer suits it still shows this frame.
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        stale_ = true;
        return std::nullopt;
    }
    if (acquired == VK_SUBOPTIMAL_KHR) {
        stale_ = true;
    } else if (const auto error = failure(acquired, "acquiring a window's image")) {
        return std::unexpected(*error);
    }
    const std::size_t image = index;
    return Frame{
        .index = index,
        .image = swapchain_->images.at(image),
        .view = swapchain_->views.at(image).get(),
        .extent = extent,
        .acquired = swapchain_->acquired.get(),
        .rendered = swapchain_->rendered.at(image).get(),
    };
}

std::expected<void, DeviceError> Presentation::end_frame(const Device& device, const Frame& frame) {
    VkSwapchainKHR swapchain = swapchain_->swapchain.get();
    const VkPresentInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.rendered,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &frame.index,
    };
    const VkResult presented = vkQueuePresentKHR(device.queue(), &info);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
        stale_ = true;
        return {};
    }
    if (const auto error = failure(presented, "presenting a frame")) {
        return std::unexpected(*error);
    }
    return {};
}

std::expected<void, DeviceError> Presentation::remake_swapchain(const Device& device,
                                                                Extent extent) {
    // Nothing may still use the old swapchain's images or semaphores. Presenting isn't fenced, but
    // an idle device has finished every present it was given.
    if (const auto error = failure(vkDeviceWaitIdle(device.get()), "waiting for the device")) {
        return std::unexpected(*error);
    }
    const auto capabilities = capabilities_of(device);
    if (!capabilities) {
        return std::unexpected(capabilities.error());
    }
    // One image more than the least, so that the next frame need not wait for the display to let
    // go of one.
    std::uint32_t image_count = capabilities->minImageCount + 1;
    if (capabilities->maxImageCount != 0) {
        image_count = std::min(image_count, capabilities->maxImageCount);
    }
    const VkSwapchainCreateInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = device.surface(),
        .minImageCount = image_count,
        .imageFormat = format_.format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent =
            {
                .width = narrow<std::uint32_t>(extent.width),
                .height = narrow<std::uint32_t>(extent.height),
            },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = capabilities->currentTransform,
        .compositeAlpha = composite_alpha(capabilities->supportedCompositeAlpha),
        // Every device has it, and it shows each frame whole, at the display's refresh.
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = swapchain_ ? swapchain_->swapchain.get() : VK_NULL_HANDLE,
    };
    VkSwapchainKHR made = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateSwapchainKHR(device.get(), &info, nullptr, &made),
                                   "creating the window's swapchain")) {
        return std::unexpected(*error);
    }
    auto swapchain = std::make_unique<Swapchain>();
    swapchain->swapchain = SwapchainHandle(device.get(), made);
    swapchain->extent = extent;

    std::uint32_t count = 0;
    if (const auto error = failure(vkGetSwapchainImagesKHR(device.get(), made, &count, nullptr),
                                   "listing the window's images")) {
        return std::unexpected(*error);
    }
    swapchain->images.resize(count);
    if (const auto error =
            failure(vkGetSwapchainImagesKHR(device.get(), made, &count, swapchain->images.data()),
                    "listing the window's images")) {
        return std::unexpected(*error);
    }
    for (auto* const image : swapchain->images) {
        auto view = create_view(device.get(), image, format_.format);
        if (!view) {
            return std::unexpected(std::move(view).error());
        }
        swapchain->views.push_back(std::move(*view));
        auto rendered = create_semaphore(device.get());
        if (!rendered) {
            return std::unexpected(std::move(rendered).error());
        }
        swapchain->rendered.push_back(std::move(*rendered));
    }
    auto acquired = create_semaphore(device.get());
    if (!acquired) {
        return std::unexpected(std::move(acquired).error());
    }
    swapchain->acquired = std::move(*acquired);

    swapchain_ = std::move(swapchain);
    stale_ = false;
    return {};
}

} // namespace wulfhilde::detail
