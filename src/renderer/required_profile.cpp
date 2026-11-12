#include "required_profile.hpp"

#include "formats.hpp"
#include "vulkan.hpp"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace wulfhilde::detail {

namespace {

// One feature of the required profile, by its name in the specification.
struct Requirement {
    std::string_view name;
    VkBool32& (*field)(FeatureChain& chain);
};

// The required profile, beyond Vulkan 1.3 itself: descriptor indexing as Roadmap 2022 has it, so
// that shaders reach every texture through one set; indirect draws with counts the GPU writes;
// clip distances for confining views to portals; one memory layout for shaders and C++; and
// anisotropic filtering. Vulkan 1.3 guarantees the rest, which are listed so that they are turned
// on.
constexpr std::array requirements{
    Requirement{
        .name = "multiDrawIndirect",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.features.features.multiDrawIndirect;
        },
    },
    Requirement{
        .name = "drawIndirectCount",
        .field = [](FeatureChain& chain) -> VkBool32& { return chain.vulkan12.drawIndirectCount; },
    },
    Requirement{
        .name = "shaderClipDistance",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.features.features.shaderClipDistance;
        },
    },
    Requirement{
        .name = "scalarBlockLayout",
        .field = [](FeatureChain& chain) -> VkBool32& { return chain.vulkan12.scalarBlockLayout; },
    },
    Requirement{
        .name = "samplerAnisotropy",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.features.features.samplerAnisotropy;
        },
    },
    Requirement{
        .name = "descriptorIndexing",
        .field = [](FeatureChain& chain) -> VkBool32& { return chain.vulkan12.descriptorIndexing; },
    },
    Requirement{
        .name = "runtimeDescriptorArray",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.runtimeDescriptorArray;
        },
    },
    Requirement{
        .name = "descriptorBindingPartiallyBound",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.descriptorBindingPartiallyBound;
        },
    },
    Requirement{
        .name = "descriptorBindingUpdateUnusedWhilePending",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.descriptorBindingUpdateUnusedWhilePending;
        },
    },
    Requirement{
        .name = "descriptorBindingSampledImageUpdateAfterBind",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.descriptorBindingSampledImageUpdateAfterBind;
        },
    },
    Requirement{
        .name = "descriptorBindingStorageImageUpdateAfterBind",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.descriptorBindingStorageImageUpdateAfterBind;
        },
    },
    Requirement{
        .name = "shaderSampledImageArrayNonUniformIndexing",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.shaderSampledImageArrayNonUniformIndexing;
        },
    },
    Requirement{
        .name = "shaderStorageImageArrayNonUniformIndexing",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.shaderStorageImageArrayNonUniformIndexing;
        },
    },
    Requirement{
        .name = "multiview",
        .field = [](FeatureChain& chain) -> VkBool32& { return chain.vulkan11.multiview; },
    },
    Requirement{
        .name = "bufferDeviceAddress",
        .field = [](FeatureChain& chain) -> VkBool32& {
            return chain.vulkan12.bufferDeviceAddress;
        },
    },
    Requirement{
        .name = "dynamicRendering",
        .field = [](FeatureChain& chain) -> VkBool32& { return chain.vulkan13.dynamicRendering; },
    },
    Requirement{
        .name = "synchronization2",
        .field = [](FeatureChain& chain) -> VkBool32& { return chain.vulkan13.synchronization2; },
    },
};

} // namespace

FeatureChain::FeatureChain(std::uint32_t api_version) noexcept {
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &vulkan11;
    vulkan11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11.pNext = &vulkan12;
    vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12.pNext = &vulkan13;
    vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
    if (api_version >= VK_API_VERSION_1_4) {
        vulkan13.pNext = &vulkan14;
    }
}

std::vector<std::string_view> missing_features(VkPhysicalDevice device, std::uint32_t api_version) {
    if (api_version < VK_API_VERSION_1_3) {
        return {"Vulkan 1.3"};
    }
    FeatureChain supported(api_version);
    vkGetPhysicalDeviceFeatures2(device, &supported.features);
    std::vector<std::string_view> missing;
    for (const auto& requirement : requirements) {
        if (requirement.field(supported) != VK_TRUE) {
            missing.push_back(requirement.name);
        }
    }
    // Vulkan guarantees a depth format with stencil, but not which, and only this one has float
    // depth.
    VkFormatProperties depth{};
    vkGetPhysicalDeviceFormatProperties(device, scene_depth_format, &depth);
    if ((depth.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0) {
        missing.emplace_back("a 32-bit float depth format with stencil");
    }
    return missing;
}

void enable_features(VkPhysicalDevice device, std::uint32_t api_version, FeatureChain& enabled) {
    for (const auto& requirement : requirements) {
        requirement.field(enabled) = VK_TRUE;
    }
    if (api_version >= VK_API_VERSION_1_4) {
        FeatureChain supported(VK_API_VERSION_1_4);
        vkGetPhysicalDeviceFeatures2(device, &supported.features);
        // Pipelines then take their SPIR-V directly, with no shader module to make and destroy.
        enabled.vulkan14.maintenance5 = supported.vulkan14.maintenance5;
    }
}

} // namespace wulfhilde::detail
