// GPU memory, from VulkanMemoryAllocator: the allocator, and the buffers and images it backs.
#pragma once

#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>

#include <vk_mem_alloc.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace wulfhilde::detail {

/// Owns a VulkanMemoryAllocator allocator, which hands out the memory of one device.
class Allocator {
  public:
    /// Creates an allocator for a device, whose buffers may be reached by device address.
    [[nodiscard]] static std::expected<Allocator, DeviceError>
    create(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device,
           std::uint32_t api_version);

    Allocator(Allocator&& other) noexcept : allocator_(std::exchange(other.allocator_, nullptr)) {}

    Allocator& operator=(Allocator&& other) noexcept {
        if (this != &other) {
            reset();
            allocator_ = std::exchange(other.allocator_, nullptr);
        }
        return *this;
    }

    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    ~Allocator() { reset(); }

    /// Returns the allocator, still owned.
    [[nodiscard]] VmaAllocator get() const noexcept { return allocator_; }

  private:
    explicit Allocator(VmaAllocator allocator) noexcept : allocator_(allocator) {}

    void reset() noexcept;

    VmaAllocator allocator_ = nullptr;
};

/// A buffer in memory that the CPU sees, mapped for as long as the buffer lives, and that shaders
/// reach by its device address.
class Buffer {
  public:
    /// Creates a buffer of `size` bytes, its contents undefined.
    ///
    /// \param allocator The allocator of the device.
    /// \param device The device.
    /// \param size The buffer's size in bytes, more than zero.
    /// \param usage What else the buffer is used for, beyond being reached by device address.
    [[nodiscard]] static std::expected<Buffer, DeviceError>
    create(VmaAllocator allocator, VkDevice device, std::size_t size, VkBufferUsageFlags usage);

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    ~Buffer() { reset(); }

    /// Returns the buffer, still owned.
    [[nodiscard]] VkBuffer get() const noexcept { return buffer_; }

    /// Returns the address at which shaders reach the buffer.
    [[nodiscard]] VkDeviceAddress address() const noexcept { return address_; }

    /// Returns the buffer's memory as the CPU sees it, coherent with the device's view.
    [[nodiscard]] std::span<std::byte> memory() const noexcept { return memory_; }

  private:
    Buffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation,
           VkDeviceAddress address, std::span<std::byte> memory) noexcept
        : allocator_(allocator), buffer_(buffer), allocation_(allocation), address_(address),
          memory_(memory) {}

    void reset() noexcept;

    VmaAllocator allocator_ = nullptr;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
    VkDeviceAddress address_ = 0;
    std::span<std::byte> memory_;
};

/// A two-dimensional image in the device's own memory, with one mip level, and a view of all of
/// it.
class GpuImage {
  public:
    /// Creates an image, its contents undefined.
    ///
    /// \param allocator The allocator of the device.
    /// \param device The device.
    /// \param format The image's format, a colour format.
    /// \param extent The image's size, neither side zero.
    /// \param usage What the image is used for.
    [[nodiscard]] static std::expected<GpuImage, DeviceError> create(VmaAllocator allocator,
                                                                     VkDevice device,
                                                                     VkFormat format, Extent extent,
                                                                     VkImageUsageFlags usage);

    GpuImage(GpuImage&& other) noexcept;
    GpuImage& operator=(GpuImage&& other) noexcept;
    GpuImage(const GpuImage&) = delete;
    GpuImage& operator=(const GpuImage&) = delete;

    ~GpuImage() { reset(); }

    /// Returns the image, still owned.
    [[nodiscard]] VkImage get() const noexcept { return image_; }

    /// Returns the view of the whole image, still owned.
    [[nodiscard]] VkImageView view() const noexcept { return view_.get(); }

  private:
    GpuImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation) noexcept
        : allocator_(allocator), image_(image), allocation_(allocation) {}

    void reset() noexcept;

    VmaAllocator allocator_ = nullptr;
    VkImage image_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
    ImageViewHandle view_;
};

} // namespace wulfhilde::detail
