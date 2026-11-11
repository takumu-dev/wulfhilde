// The one descriptor set every shader reaches its textures and samplers through, by the
// `DescriptorHandle`s in its push constants and buffers.
#pragma once

#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/renderer/device_error.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>

namespace wulfhilde::detail {

/// The C++ twin of Slang's `DescriptorHandle<T>`, which it lowers to a `uint2`. The first half
/// indexes the array of T's descriptor type in the bindless set; the second is unused.
struct DescriptorHandle {
    std::uint32_t index = 0;
    std::uint32_t unused = 0;
};

/// The bindless set: one array per descriptor type, at the bindings that Slang's
/// `BindlessDescriptorOptions.None` preset gives them, in set 0. Descriptors may be added while
/// the set is bound, and those never added may be left unwritten.
class BindlessDescriptors {
  public:
    static constexpr std::uint32_t sampler_binding = 0;
    static constexpr std::uint32_t sampled_image_binding = 2;
    static constexpr std::uint32_t storage_image_binding = 3;

    static constexpr std::uint32_t sampler_count = 64;
    static constexpr std::uint32_t sampled_image_count = 16384;
    static constexpr std::uint32_t storage_image_count = 1024;

    /// Creates the set's layout and the set itself, which holds no descriptors yet.
    [[nodiscard]] static std::expected<BindlessDescriptors, DeviceError> create(VkDevice device);

    /// Returns the set's layout, still owned.
    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept { return layout_.get(); }

    /// Returns the set, which lives as long as this does.
    [[nodiscard]] VkDescriptorSet set() const noexcept { return set_; }

    /// Adds an image view that shaders sample, in the shader-read-only layout, and returns its
    /// handle. The view must outlive every use of the handle.
    [[nodiscard]] DescriptorHandle add_sampled_image(VkImageView view);

    /// Adds a sampler and returns its handle. The sampler must outlive every use of the handle.
    [[nodiscard]] DescriptorHandle add_sampler(VkSampler sampler);

  private:
    BindlessDescriptors(VkDevice device, DescriptorSetLayoutHandle layout,
                        DescriptorPoolHandle pool, VkDescriptorSet set) noexcept
        : device_(device), layout_(std::move(layout)), pool_(std::move(pool)), set_(set) {}

    VkDevice device_;
    DescriptorSetLayoutHandle layout_;
    DescriptorPoolHandle pool_;
    VkDescriptorSet set_;
    std::size_t sampled_images_ = 0;
    std::size_t samplers_ = 0;
};

} // namespace wulfhilde::detail
