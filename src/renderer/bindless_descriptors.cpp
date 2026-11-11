#include "bindless_descriptors.hpp"

#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/device_error.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <utility>

namespace wulfhilde::detail {

std::expected<BindlessDescriptors, DeviceError> BindlessDescriptors::create(VkDevice device) {
    const std::array bindings{
        VkDescriptorSetLayoutBinding{
            .binding = sampler_binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = sampler_count,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = sampled_image_binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = sampled_image_count,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = storage_image_binding,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = storage_image_count,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
    };
    constexpr VkDescriptorBindingFlags binding_flags = flags_of(
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT);
    const std::array flags{binding_flags, binding_flags, binding_flags};
    const VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = narrow<std::uint32_t>(flags.size()),
        .pBindingFlags = flags.data(),
    };
    const VkDescriptorSetLayoutCreateInfo layout_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_info,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = narrow<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    VkDescriptorSetLayout made_layout = VK_NULL_HANDLE;
    if (const auto error =
            failure(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &made_layout),
                    "creating the bindless set's layout")) {
        return std::unexpected(*error);
    }
    DescriptorSetLayoutHandle layout(device, made_layout);

    const std::array sizes{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = sampler_count},
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = sampled_image_count,
        },
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = storage_image_count,
        },
    };
    const VkDescriptorPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = narrow<std::uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };
    VkDescriptorPool made_pool = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateDescriptorPool(device, &pool_info, nullptr, &made_pool),
                                   "creating the bindless set's pool")) {
        return std::unexpected(*error);
    }
    DescriptorPoolHandle pool(device, made_pool);

    const VkDescriptorSetAllocateInfo set_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = made_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &made_layout,
    };
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (const auto error = failure(vkAllocateDescriptorSets(device, &set_info, &set),
                                   "allocating the bindless set")) {
        return std::unexpected(*error);
    }
    return BindlessDescriptors(device, std::move(layout), std::move(pool), set);
}

DescriptorHandle BindlessDescriptors::add_sampled_image(VkImageView view) {
    WULFHILDE_ASSERT(sampled_images_ < sampled_image_count,
                     "the bindless set has room for another sampled image");
    const VkDescriptorImageInfo image{
        .imageView = view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set_,
        .dstBinding = sampled_image_binding,
        .dstArrayElement = narrow<std::uint32_t>(sampled_images_),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &image,
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return {.index = narrow<std::uint32_t>(sampled_images_++)};
}

DescriptorHandle BindlessDescriptors::add_sampler(VkSampler sampler) {
    WULFHILDE_ASSERT(samplers_ < sampler_count, "the bindless set has room for another sampler");
    const VkDescriptorImageInfo image{.sampler = sampler};
    const VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = set_,
        .dstBinding = sampler_binding,
        .dstArrayElement = narrow<std::uint32_t>(samplers_),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &image,
    };
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
    return {.index = narrow<std::uint32_t>(samplers_++)};
}

} // namespace wulfhilde::detail
