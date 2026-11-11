#include "instance.hpp"

#include "owned_handle.hpp"
#include "vulkan.hpp"
#include <wulfhilde/core/log_sink.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/renderer/device_error.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

namespace {

constexpr const char* validation_layer = "VK_LAYER_KHRONOS_validation";

// Sends one of the validation layers' messages to the log sink, as one line.
VKAPI_ATTR VkBool32 VKAPI_CALL report(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                      VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                      const VkDebugUtilsMessengerCallbackDataEXT* data,
                                      void* /*user_data*/) {
    std::string message = std::format("Vulkan validation: {}", data->pMessage);
    std::ranges::replace(message, '\n', ' ');
    log((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0 ? LogSeverity::error
                                                                        : LogSeverity::warning,
        message);
    return VK_FALSE;
}

// The layers' own messages. General ones come from the loader too, such as its notice of a layer
// installed twice, and aren't validation's.
constexpr VkDebugUtilsMessengerCreateInfoEXT messenger_info{
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = report,
};

// Loads the Vulkan loader once for the process, as volk keeps what it loads in globals.
std::expected<std::uint32_t, DeviceError> load_vulkan() {
    static const VkResult loaded = volkInitialize();
    if (loaded != VK_SUCCESS) {
        return std::unexpected(DeviceError{
            .code = DeviceErrorCode::no_vulkan,
            .message = "no Vulkan loader was found",
        });
    }
    std::uint32_t version = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion != nullptr) {
        if (const auto error =
                failure(vkEnumerateInstanceVersion(&version), "finding Vulkan's version")) {
            return std::unexpected(*error);
        }
    }
    if (version < VK_API_VERSION_1_3) {
        return std::unexpected(DeviceError{
            .code = DeviceErrorCode::no_vulkan,
            .message = std::format("the Vulkan loader is version {}.{}, older than Vulkan 1.3",
                                   VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version)),
        });
    }
    return version;
}

std::expected<bool, DeviceError> has_validation_layer() {
    std::uint32_t count = 0;
    if (const auto error = failure(vkEnumerateInstanceLayerProperties(&count, nullptr),
                                   "listing Vulkan's layers")) {
        return std::unexpected(*error);
    }
    std::vector<VkLayerProperties> layers(count);
    if (const auto error = failure(vkEnumerateInstanceLayerProperties(&count, layers.data()),
                                   "listing Vulkan's layers")) {
        return std::unexpected(*error);
    }
    return std::ranges::any_of(layers, [](const VkLayerProperties& layer) {
        return name_of(layer.layerName) == validation_layer;
    });
}

} // namespace

std::expected<Instance, DeviceError> Instance::create(bool validation) {
    const auto loader_version = load_vulkan();
    if (!loader_version) {
        return std::unexpected(loader_version.error());
    }

    if (validation) {
        const auto found = has_validation_layer();
        if (!found) {
            return std::unexpected(found.error());
        }
        if (!*found) {
            return std::unexpected(DeviceError{
                .code = DeviceErrorCode::no_validation,
                .message = std::format("validation is on, and Vulkan has no {} installed",
                                       validation_layer),
            });
        }
    }

    const VkApplicationInfo application{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pEngineName = "wulfhilde",
        .apiVersion = std::min(*loader_version, VK_API_VERSION_1_4),
    };

    // Synchronisation validation is off by default, and finds the hazards that the other checks
    // can't.
    const VkBool32 enabled = VK_TRUE;
    const VkLayerSettingEXT sync_validation{
        .pLayerName = validation_layer,
        .pSettingName = "validate_sync",
        .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
        .valueCount = 1,
        .pValues = &enabled,
    };
    const VkLayerSettingsCreateInfoEXT settings{
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
        .pNext = &messenger_info,
        .settingCount = 1,
        .pSettings = &sync_validation,
    };
    const std::array layers{validation_layer};
    const std::array extensions{
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_EXT_LAYER_SETTINGS_EXTENSION_NAME,
    };
    const VkInstanceCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        // The messenger in the chain reports what goes wrong while the instance is being made.
        .pNext = validation ? &settings : nullptr,
        .pApplicationInfo = &application,
        .enabledLayerCount = validation ? narrow<std::uint32_t>(layers.size()) : 0,
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = validation ? narrow<std::uint32_t>(extensions.size()) : 0,
        .ppEnabledExtensionNames = extensions.data(),
    };
    VkInstance created = VK_NULL_HANDLE;
    if (const auto error =
            failure(vkCreateInstance(&info, nullptr, &created), "creating a Vulkan instance")) {
        return std::unexpected(*error);
    }
    InstanceHandle instance(created);
    volkLoadInstance(created);

    DebugMessengerHandle messenger;
    if (validation) {
        VkDebugUtilsMessengerEXT made = VK_NULL_HANDLE;
        if (const auto error =
                failure(vkCreateDebugUtilsMessengerEXT(created, &messenger_info, nullptr, &made),
                        "creating the validation layers' messenger")) {
            return std::unexpected(*error);
        }
        messenger = DebugMessengerHandle(created, made);
    }
    return Instance(std::move(instance), std::move(messenger), application.apiVersion);
}

} // namespace wulfhilde::detail
