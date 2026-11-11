// Move-only owners of Vulkan handles, each destroying its handle with the function Vulkan pairs
// with the function that created it.
#pragma once

#include "vulkan.hpp"

#include <utility>

namespace wulfhilde::detail {

/// Owns a handle that nothing else owns, such as an instance or a device, and destroys it with
/// `Destroy(handle, nullptr)`.
template <typename Handle, auto& Destroy> class OwnedRootHandle {
  public:
    /// Owns nothing.
    OwnedRootHandle() = default;

    /// Takes ownership of a handle.
    explicit OwnedRootHandle(Handle handle) noexcept : handle_(handle) {}

    /// Takes over another owner's handle.
    OwnedRootHandle(OwnedRootHandle&& other) noexcept
        : handle_(std::exchange(other.handle_, VK_NULL_HANDLE)) {}

    /// Destroys this owner's handle, then takes over another's.
    OwnedRootHandle& operator=(OwnedRootHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
        }
        return *this;
    }

    OwnedRootHandle(const OwnedRootHandle&) = delete;
    OwnedRootHandle& operator=(const OwnedRootHandle&) = delete;

    ~OwnedRootHandle() { reset(); }

    /// Returns the handle, still owned.
    [[nodiscard]] Handle get() const noexcept { return handle_; }

  private:
    void reset() noexcept {
        if (handle_ != VK_NULL_HANDLE) {
            // The function as volk has loaded it by now. clang-tidy sees the template argument it
            // refers to as parenthesised.
            // NOLINTNEXTLINE(readability-redundant-parentheses): no parentheses are written
            const auto destroy = Destroy;
            destroy(handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    Handle handle_ = VK_NULL_HANDLE;
};

/// Owns a handle made from a parent, such as a device's buffer, and destroys it with
/// `Destroy(parent, handle, nullptr)`. The parent must outlive it.
template <typename Parent, typename Handle, auto& Destroy> class OwnedHandle {
  public:
    /// Owns nothing.
    OwnedHandle() = default;

    /// Takes ownership of a handle made from `parent`.
    OwnedHandle(Parent parent, Handle handle) noexcept : parent_(parent), handle_(handle) {}

    /// Takes over another owner's handle.
    OwnedHandle(OwnedHandle&& other) noexcept
        : parent_(other.parent_), handle_(std::exchange(other.handle_, VK_NULL_HANDLE)) {}

    /// Destroys this owner's handle, then takes over another's.
    OwnedHandle& operator=(OwnedHandle&& other) noexcept {
        if (this != &other) {
            reset();
            parent_ = other.parent_;
            handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
        }
        return *this;
    }

    OwnedHandle(const OwnedHandle&) = delete;
    OwnedHandle& operator=(const OwnedHandle&) = delete;

    ~OwnedHandle() { reset(); }

    /// Returns the handle, still owned.
    [[nodiscard]] Handle get() const noexcept { return handle_; }

  private:
    void reset() noexcept {
        if (handle_ != VK_NULL_HANDLE) {
            // NOLINTNEXTLINE(readability-redundant-parentheses): no parentheses are written
            const auto destroy = Destroy;
            destroy(parent_, handle_, nullptr);
            handle_ = VK_NULL_HANDLE;
        }
    }

    Parent parent_ = VK_NULL_HANDLE;
    Handle handle_ = VK_NULL_HANDLE;
};

using InstanceHandle = OwnedRootHandle<VkInstance, vkDestroyInstance>;
using DeviceHandle = OwnedRootHandle<VkDevice, vkDestroyDevice>;
using DebugMessengerHandle =
    OwnedHandle<VkInstance, VkDebugUtilsMessengerEXT, vkDestroyDebugUtilsMessengerEXT>;
using CommandPoolHandle = OwnedHandle<VkDevice, VkCommandPool, vkDestroyCommandPool>;
using DescriptorPoolHandle = OwnedHandle<VkDevice, VkDescriptorPool, vkDestroyDescriptorPool>;
using DescriptorSetLayoutHandle =
    OwnedHandle<VkDevice, VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>;
using FenceHandle = OwnedHandle<VkDevice, VkFence, vkDestroyFence>;
using ImageViewHandle = OwnedHandle<VkDevice, VkImageView, vkDestroyImageView>;
using PipelineHandle = OwnedHandle<VkDevice, VkPipeline, vkDestroyPipeline>;
using PipelineLayoutHandle = OwnedHandle<VkDevice, VkPipelineLayout, vkDestroyPipelineLayout>;
using SamplerHandle = OwnedHandle<VkDevice, VkSampler, vkDestroySampler>;
using ShaderModuleHandle = OwnedHandle<VkDevice, VkShaderModule, vkDestroyShaderModule>;

} // namespace wulfhilde::detail
