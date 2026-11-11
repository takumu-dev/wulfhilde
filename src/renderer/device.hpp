#pragma once

#include "bindless_descriptors.hpp"
#include "instance.hpp"
#include "memory.hpp"
#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace wulfhilde::detail {

/// The device a renderer draws with, and what every pipeline shares on it: the bindless set, the
/// one pipeline layout, and a queue that runs one batch of commands at a time.
class Device {
    // Lets `create` alone call the public constructor that std::make_unique needs.
    struct Key {
        explicit Key() = default;
    };

  public:
    /// The size of every pipeline's push constants, the least that Vulkan guarantees, which each
    /// shader's constants must fit within.
    static constexpr std::uint32_t push_constant_size = 128;

    /// Everything a device is made of, in the order it is made.
    struct Parts {
        Instance instance;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        std::uint32_t api_version = 0;
        bool takes_spirv_directly = false;
        DeviceHandle device;
        Allocator allocator;
        VkQueue queue = VK_NULL_HANDLE;
        CommandPoolHandle command_pool;
        VkCommandBuffer commands = VK_NULL_HANDLE;
        FenceHandle fence;
        BindlessDescriptors descriptors;
        PipelineLayoutHandle pipeline_layout;
    };

    /// Comes up on the first device that has the whole required profile.
    ///
    /// \param settings How the device is set up.
    /// \param newest_api_version The newest Vulkan version to take any device to have: 1.4 for
    ///        a renderer, and older for the tests, to stand in for an older device.
    [[nodiscard]] static std::expected<std::unique_ptr<Device>, DeviceError>
    create(const RendererSettings& settings, std::uint32_t newest_api_version = VK_API_VERSION_1_4);

    /// Takes the parts that `create` made.
    Device(Key /*key*/, Parts parts) noexcept : parts_(std::move(parts)) {}

    Device(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(const Device&) = delete;
    Device& operator=(Device&&) = delete;

    /// Waits for the device to finish its work before its parts are released.
    ~Device();

    [[nodiscard]] VkDevice get() const noexcept { return parts_.device.get(); }

    [[nodiscard]] VmaAllocator allocator() const noexcept { return parts_.allocator.get(); }

    [[nodiscard]] VkPipelineLayout pipeline_layout() const noexcept {
        return parts_.pipeline_layout.get();
    }

    [[nodiscard]] BindlessDescriptors& descriptors() noexcept { return parts_.descriptors; }

    /// Whether pipelines take their SPIR-V directly, as Vulkan 1.4's `maintenance5` allows,
    /// rather than through a shader module.
    [[nodiscard]] bool takes_spirv_directly() const noexcept { return parts_.takes_spirv_directly; }

    /// Records the binding of a pipeline, with the bindless set and push constants that every
    /// pipeline shares.
    ///
    /// \param commands The command buffer to record into.
    /// \param bind_point Whether the pipeline draws or computes.
    /// \param pipeline The pipeline, made over the device's pipeline layout.
    /// \param push_constants The push constants, no more than `push_constant_size` bytes.
    void record_binding(VkCommandBuffer commands, VkPipelineBindPoint bind_point,
                        VkPipeline pipeline, std::span<const std::byte> push_constants) const;

    /// Records commands with `record`, runs them, and waits until they finish.
    template <std::invocable<VkCommandBuffer> Record>
    [[nodiscard]] std::expected<void, DeviceError> execute(Record&& record) {
        if (auto begun = begin_commands(); !begun) {
            return begun;
        }
        std::forward<Record>(record)(parts_.commands);
        return finish_commands();
    }

  private:
    [[nodiscard]] std::expected<void, DeviceError> begin_commands() const;
    [[nodiscard]] std::expected<void, DeviceError> finish_commands();

    Parts parts_;
};

} // namespace wulfhilde::detail
