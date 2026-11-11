#pragma once

#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/renderer/device_error.hpp>

#include <cstdint>
#include <expected>
#include <utility>

namespace wulfhilde::detail {

/// A Vulkan instance, through which the renderer finds its device, with the validation layers'
/// messages sent to the log sink when validation is on.
class Instance {
  public:
    /// Loads Vulkan and creates an instance of Vulkan 1.4 or, where the loader is older, 1.3.
    ///
    /// \param validation Whether to run under the validation layers, synchronisation validation
    ///                   included, which must be installed.
    [[nodiscard]] static std::expected<Instance, DeviceError> create(bool validation);

    /// Returns the instance, still owned.
    [[nodiscard]] VkInstance get() const noexcept { return instance_.get(); }

    /// Returns the newest Vulkan version the instance was created for, which caps the version of
    /// every device used through it.
    [[nodiscard]] std::uint32_t api_version() const noexcept { return api_version_; }

  private:
    Instance(InstanceHandle instance, DebugMessengerHandle messenger,
             std::uint32_t api_version) noexcept
        : instance_(std::move(instance)), messenger_(std::move(messenger)),
          api_version_(api_version) {}

    InstanceHandle instance_;
    DebugMessengerHandle messenger_;
    std::uint32_t api_version_;
};

} // namespace wulfhilde::detail
