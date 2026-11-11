#include "device.hpp"

#include "bindless_descriptors.hpp"
#include "instance.hpp"
#include "memory.hpp"
#include "owned_handle.hpp"
#include "required_profile.hpp"
#include "vulkan.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/log_sink.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

// A device found on the machine, with what it lacks of the required profile, which includes a
// queue family for graphics and compute when it has none.
struct Candidate {
    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties{};
    std::uint32_t queue_family = 0;
    std::vector<std::string_view> missing;
};

// A GPU of its own comes first, then one built into the processor, then any other.
int preference(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 0;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 1;
    default:
        return 2;
    }
}

// The first queue family that runs both graphics and compute, which every frame needs.
std::optional<std::uint32_t> graphics_queue_family(VkPhysicalDevice device) {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    constexpr VkQueueFlags needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    for (std::size_t index = 0; const auto& family : families) {
        if ((family.queueFlags & needed) == needed) {
            return narrow<std::uint32_t>(index);
        }
        ++index;
    }
    return std::nullopt;
}

std::expected<std::vector<Candidate>, DeviceError>
find_candidates(VkInstance instance, std::uint32_t newest_api_version) {
    std::uint32_t count = 0;
    if (const auto error = failure(vkEnumeratePhysicalDevices(instance, &count, nullptr),
                                   "listing Vulkan devices")) {
        return std::unexpected(*error);
    }
    std::vector<VkPhysicalDevice> devices(count);
    if (const auto error = failure(vkEnumeratePhysicalDevices(instance, &count, devices.data()),
                                   "listing Vulkan devices")) {
        return std::unexpected(*error);
    }
    std::vector<Candidate> candidates;
    for (auto* const device : devices) {
        Candidate candidate{.device = device};
        vkGetPhysicalDeviceProperties(device, &candidate.properties);
        candidate.properties.apiVersion =
            std::min(candidate.properties.apiVersion, newest_api_version);
        candidate.missing = missing_features(device, candidate.properties.apiVersion);
        if (const auto family = graphics_queue_family(device)) {
            candidate.queue_family = *family;
        } else {
            candidate.missing.emplace_back("a queue for graphics and compute");
        }
        candidates.push_back(std::move(candidate));
    }
    std::ranges::stable_sort(candidates, {}, [](const Candidate& candidate) {
        return preference(candidate.properties.deviceType);
    });
    return candidates;
}

// Joins names as prose: "a", "a and b", "a, b and c".
std::string join_names(const std::vector<std::string_view>& names) {
    std::string joined;
    for (std::size_t index = 0; const auto name : names) {
        if (index > 0) {
            joined += index + 1 == names.size() ? " and " : ", ";
        }
        joined += name;
        ++index;
    }
    return joined;
}

DeviceError no_suitable_device(const std::vector<Candidate>& candidates) {
    if (candidates.empty()) {
        return {
            .code = DeviceErrorCode::no_suitable_device,
            .message = "no Vulkan device was found",
        };
    }
    std::string message = "no Vulkan device has every feature the renderer needs:";
    for (std::size_t index = 0; const auto& candidate : candidates) {
        std::format_to(std::back_inserter(message), "{} {} lacks {}", index == 0 ? "" : ";",
                       name_of(candidate.properties.deviceName), join_names(candidate.missing));
        ++index;
    }
    return {.code = DeviceErrorCode::no_suitable_device, .message = std::move(message)};
}

std::expected<PipelineLayoutHandle, DeviceError>
create_pipeline_layout(VkDevice device, VkDescriptorSetLayout bindless_layout) {
    const VkPushConstantRange push_constants{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .size = Device::push_constant_size,
    };
    const VkPipelineLayoutCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &bindless_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constants,
    };
    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreatePipelineLayout(device, &info, nullptr, &layout),
                                   "creating the pipeline layout")) {
        return std::unexpected(*error);
    }
    return PipelineLayoutHandle(device, layout);
}

} // namespace

std::expected<std::unique_ptr<Device>, DeviceError>
Device::create(const RendererSettings& settings, std::uint32_t newest_api_version) {
    auto instance = Instance::create(settings.validation);
    if (!instance) {
        return std::unexpected(std::move(instance).error());
    }
    const auto candidates =
        find_candidates(instance->get(), std::min(newest_api_version, instance->api_version()));
    if (!candidates) {
        return std::unexpected(candidates.error());
    }
    const auto chosen = std::ranges::find_if(
        *candidates, [](const Candidate& candidate) { return candidate.missing.empty(); });
    if (chosen == candidates->end()) {
        return std::unexpected(no_suitable_device(*candidates));
    }

    const std::uint32_t api_version = chosen->properties.apiVersion;
    FeatureChain enabled(api_version);
    enable_features(chosen->device, api_version, enabled);

    const float priority = 1.0F;
    const VkDeviceQueueCreateInfo queue_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = chosen->queue_family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    const VkDeviceCreateInfo device_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled.features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
    };
    VkDevice made_device = VK_NULL_HANDLE;
    if (const auto error =
            failure(vkCreateDevice(chosen->device, &device_info, nullptr, &made_device),
                    "creating the Vulkan device")) {
        return std::unexpected(*error);
    }
    DeviceHandle device(made_device);

    auto allocator = Allocator::create(instance->get(), chosen->device, made_device, api_version);
    if (!allocator) {
        return std::unexpected(std::move(allocator).error());
    }

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(made_device, chosen->queue_family, 0, &queue);

    const VkCommandPoolCreateInfo pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = chosen->queue_family,
    };
    VkCommandPool made_pool = VK_NULL_HANDLE;
    if (const auto error =
            failure(vkCreateCommandPool(made_device, &pool_info, nullptr, &made_pool),
                    "creating a command pool")) {
        return std::unexpected(*error);
    }
    CommandPoolHandle command_pool(made_device, made_pool);

    const VkCommandBufferAllocateInfo commands_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = made_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commands = VK_NULL_HANDLE;
    if (const auto error = failure(vkAllocateCommandBuffers(made_device, &commands_info, &commands),
                                   "allocating a command buffer")) {
        return std::unexpected(*error);
    }

    const VkFenceCreateInfo fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence made_fence = VK_NULL_HANDLE;
    if (const auto error = failure(vkCreateFence(made_device, &fence_info, nullptr, &made_fence),
                                   "creating a fence")) {
        return std::unexpected(*error);
    }
    FenceHandle fence(made_device, made_fence);

    auto descriptors = BindlessDescriptors::create(made_device);
    if (!descriptors) {
        return std::unexpected(std::move(descriptors).error());
    }
    auto pipeline_layout = create_pipeline_layout(made_device, descriptors->layout());
    if (!pipeline_layout) {
        return std::unexpected(std::move(pipeline_layout).error());
    }

    log(LogSeverity::info,
        std::format("the renderer draws with {}, through Vulkan {}.{}",
                    name_of(chosen->properties.deviceName), VK_API_VERSION_MAJOR(api_version),
                    VK_API_VERSION_MINOR(api_version)));
    return std::make_unique<Device>(
        Key{}, Parts{
                   .instance = std::move(*instance),
                   .physical_device = chosen->device,
                   .api_version = api_version,
                   .takes_spirv_directly = enabled.vulkan14.maintenance5 == VK_TRUE,
                   .device = std::move(device),
                   .allocator = std::move(*allocator),
                   .queue = queue,
                   .command_pool = std::move(command_pool),
                   .commands = commands,
                   .fence = std::move(fence),
                   .descriptors = std::move(*descriptors),
                   .pipeline_layout = std::move(*pipeline_layout),
               });
}

Device::~Device() {
    // Nothing is released while the device may still be using it, and a device that is lost has
    // nothing left to wait for.
    static_cast<void>(vkDeviceWaitIdle(parts_.device.get()));
}

void Device::record_binding(VkCommandBuffer commands, VkPipelineBindPoint bind_point,
                            VkPipeline pipeline, std::span<const std::byte> push_constants) const {
    WULFHILDE_ASSERT(push_constants.size() <= push_constant_size,
                     "the push constants fit within the pipeline layout's");
    const std::array sets{parts_.descriptors.set()};
    vkCmdBindPipeline(commands, bind_point, pipeline);
    vkCmdBindDescriptorSets(commands, bind_point, pipeline_layout(), 0, 1, sets.data(), 0, nullptr);
    if (!push_constants.empty()) {
        vkCmdPushConstants(commands, pipeline_layout(), VK_SHADER_STAGE_ALL, 0,
                           narrow<std::uint32_t>(push_constants.size()), push_constants.data());
    }
}

std::expected<void, DeviceError> Device::begin_commands() const {
    if (const auto error = failure(vkResetCommandPool(get(), parts_.command_pool.get(), 0),
                                   "resetting the command pool")) {
        return std::unexpected(*error);
    }
    const VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (const auto error = failure(vkBeginCommandBuffer(parts_.commands, &begin_info),
                                   "beginning a command buffer")) {
        return std::unexpected(*error);
    }
    return {};
}

std::expected<void, DeviceError> Device::finish_commands() {
    if (const auto error =
            failure(vkEndCommandBuffer(parts_.commands), "ending a command buffer")) {
        return std::unexpected(*error);
    }
    auto* const fence = parts_.fence.get();
    if (const auto error = failure(vkResetFences(get(), 1, &fence), "resetting a fence")) {
        return std::unexpected(*error);
    }
    const VkCommandBufferSubmitInfo commands_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = parts_.commands,
    };
    const VkSubmitInfo2 submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commands_info,
    };
    if (const auto error =
            failure(vkQueueSubmit2(parts_.queue, 1, &submit, fence), "submitting commands")) {
        return std::unexpected(*error);
    }
    if (const auto error = failure(
            vkWaitForFences(get(), 1, &fence, VK_TRUE, std::numeric_limits<std::uint64_t>::max()),
            "waiting for commands to finish")) {
        return std::unexpected(*error);
    }
    return {};
}

} // namespace wulfhilde::detail
