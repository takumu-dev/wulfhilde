// The features a device must have for the renderer to come up on it, and the features the renderer
// turns on.
#pragma once

#include "vulkan.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace wulfhilde::detail {

/// Vulkan's feature structures for one device, chained for `vkGetPhysicalDeviceFeatures2` or
/// `vkCreateDevice`. Vulkan 1.4's structure joins the chain only for a device of Vulkan 1.4 or
/// newer, which is the only kind allowed to see it.
struct FeatureChain {

    /// Makes a chain of empty structures for a device of the given Vulkan version, 1.3 or newer.
    explicit FeatureChain(std::uint32_t api_version) noexcept;

    FeatureChain(const FeatureChain&) = delete;
    FeatureChain(FeatureChain&&) = delete;
    FeatureChain& operator=(const FeatureChain&) = delete;
    FeatureChain& operator=(FeatureChain&&) = delete;
    ~FeatureChain() = default;

    VkPhysicalDeviceFeatures2 features{};
    VkPhysicalDeviceVulkan11Features vulkan11{};
    VkPhysicalDeviceVulkan12Features vulkan12{};
    VkPhysicalDeviceVulkan13Features vulkan13{};
    VkPhysicalDeviceVulkan14Features vulkan14{};
};

/// Names each part of the required profile that a device lacks, in the profile's order, or
/// nothing when it has the whole profile. A device older than Vulkan 1.3 lacks only that.
///
/// \param device The device.
/// \param api_version The Vulkan version the device is taken to have: its own, or older.
[[nodiscard]] std::vector<std::string_view> missing_features(VkPhysicalDevice device,
                                                             std::uint32_t api_version);

/// Turns on, in `enabled`, every feature of the required profile, and those of Vulkan 1.4 that the
/// renderer takes up where `device` has them.
///
/// \param device A device that has the whole profile.
/// \param api_version The Vulkan version the device is taken to have, 1.3 or newer.
/// \param enabled The chain for creating the device, made for that version.
void enable_features(VkPhysicalDevice device, std::uint32_t api_version, FeatureChain& enabled);

} // namespace wulfhilde::detail
