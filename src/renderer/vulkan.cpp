#include "vulkan.hpp"

#include <wulfhilde/renderer/device_error.hpp>

#include <format>
#include <optional>
#include <string_view>

namespace wulfhilde::detail {

namespace {

// The name a result has in the specification, for the results a renderer can meet.
std::string_view result_name(VkResult result) {
    switch (result) {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    default:
        return "an unexpected result";
    }
}

// The error of a Vulkan call that failed with `result`, naming what was being done.
DeviceError device_failure(VkResult result, std::string_view what) {
    return {
        .code = DeviceErrorCode::device_failure,
        .message = std::format("{} failed with {} ({})", what, result_name(result),
                               static_cast<int>(result)),
    };
}

} // namespace

std::optional<DeviceError> failure(VkResult result, std::string_view what) {
    if (result != VK_SUCCESS) {
        return device_failure(result, what);
    }
    return std::nullopt;
}

} // namespace wulfhilde::detail
