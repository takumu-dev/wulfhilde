#include "memory.hpp"

#include "formats.hpp"
#include "vulkan.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

namespace wulfhilde::detail {

std::expected<Allocator, DeviceError> Allocator::create(VkInstance instance,
                                                        VkPhysicalDevice physical_device,
                                                        VkDevice device,
                                                        std::uint32_t api_version) {
    VmaAllocatorCreateInfo info{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physical_device,
        .device = device,
        .instance = instance,
        .vulkanApiVersion = api_version,
    };
    VmaVulkanFunctions functions{};
    if (const auto error = failure(vmaImportVulkanFunctionsFromVolk(&info, &functions),
                                   "finding the allocator's Vulkan functions")) {
        return std::unexpected(*error);
    }
    info.pVulkanFunctions = &functions;
    VmaAllocator allocator = nullptr;
    if (const auto error =
            failure(vmaCreateAllocator(&info, &allocator), "creating the memory allocator")) {
        return std::unexpected(*error);
    }
    return Allocator(allocator);
}

void Allocator::reset() noexcept {
    if (allocator_ != nullptr) {
        vmaDestroyAllocator(allocator_);
        allocator_ = nullptr;
    }
}

std::expected<Buffer, DeviceError> Buffer::create(VmaAllocator allocator, VkDevice device,
                                                  std::size_t size, VkBufferUsageFlags usage) {
    WULFHILDE_ASSERT(size > 0, "a buffer holds at least one byte");
    const VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    // Coherent memory needs no flushing, which suits buffers that each carry one batch of work.
    const VmaAllocationCreateInfo allocation_info{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VmaAllocationInfo allocated{};
    if (const auto error = failure(vmaCreateBuffer(allocator, &buffer_info, &allocation_info,
                                                   &buffer, &allocation, &allocated),
                                   "creating a buffer")) {
        return std::unexpected(*error);
    }
    const VkBufferDeviceAddressInfo address_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer,
    };
    return Buffer(allocator, buffer, allocation, vkGetBufferDeviceAddress(device, &address_info),
                  {static_cast<std::byte*>(allocated.pMappedData), size});
}

Buffer::Buffer(Buffer&& other) noexcept
    : allocator_(other.allocator_), buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE)),
      allocation_(std::exchange(other.allocation_, nullptr)),
      address_(std::exchange(other.address_, 0)), memory_(std::exchange(other.memory_, {})) {}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        reset();
        allocator_ = other.allocator_;
        buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
        allocation_ = std::exchange(other.allocation_, nullptr);
        address_ = std::exchange(other.address_, 0);
        memory_ = std::exchange(other.memory_, {});
    }
    return *this;
}

void Buffer::reset() noexcept {
    if (buffer_ != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = nullptr;
    }
}

std::expected<GpuImage, DeviceError> GpuImage::create(VmaAllocator allocator, VkDevice device,
                                                      VkFormat format, Extent extent,
                                                      VkImageUsageFlags usage,
                                                      VkSampleCountFlagBits samples) {
    WULFHILDE_ASSERT(extent.width > 0 && extent.height > 0, "an image has at least one pixel");
    const VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent =
            {
                .width = narrow<std::uint32_t>(extent.width),
                .height = narrow<std::uint32_t>(extent.height),
                .depth = 1,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VmaAllocationCreateInfo allocation_info{.usage = VMA_MEMORY_USAGE_AUTO};
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    if (const auto error = failure(
            vmaCreateImage(allocator, &image_info, &allocation_info, &image, &allocation, nullptr),
            "creating an image")) {
        return std::unexpected(*error);
    }
    GpuImage created(allocator, image, allocation);

    const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = aspects_of(format),
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    VkImageView view = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateImageView(device, &view_info, nullptr, &view),
                                   "creating an image view")) {
        return std::unexpected(*error);
    }
    created.view_ = ImageViewHandle(device, view);
    return created;
}

GpuImage::GpuImage(GpuImage&& other) noexcept
    : allocator_(other.allocator_), image_(std::exchange(other.image_, VK_NULL_HANDLE)),
      allocation_(std::exchange(other.allocation_, nullptr)), view_(std::move(other.view_)) {}

GpuImage& GpuImage::operator=(GpuImage&& other) noexcept {
    if (this != &other) {
        reset();
        allocator_ = other.allocator_;
        image_ = std::exchange(other.image_, VK_NULL_HANDLE);
        allocation_ = std::exchange(other.allocation_, nullptr);
        view_ = std::move(other.view_);
    }
    return *this;
}

void GpuImage::reset() noexcept {
    // The view goes before the image it views.
    view_ = {};
    if (image_ != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator_, image_, allocation_);
        image_ = VK_NULL_HANDLE;
        allocation_ = nullptr;
    }
}

} // namespace wulfhilde::detail
