#include <wulfhilde/platform/vulkan_surface.hpp>

#include "native_window.hpp"
#include "sdl_error.hpp"
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/platform/platform_error.hpp>
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/platform/window_size.hpp>

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cstddef>
#include <expected>
#include <span>

namespace wulfhilde::detail {

std::span<const char* const> vulkan_surface_extensions(const Window& /*window*/) {
    Uint32 count = 0;
    const char* const* const names = SDL_Vulkan_GetInstanceExtensions(&count);
    if (names == nullptr) {
        return {};
    }
    return {names, count};
}

std::expected<VkSurfaceKHR, PlatformError> create_vulkan_surface(const Window& window,
                                                                 VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(WindowAccess::get(window), instance, nullptr, &surface)) {
        return std::unexpected(sdl_error(PlatformErrorCode::window_failure,
                                         "Vulkan couldn't make a surface for the window"));
    }
    return surface;
}

WindowSize pixel_size(const Window& window) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(WindowAccess::get(window), &width, &height);
    return {.width = narrow<std::size_t>(width), .height = narrow<std::size_t>(height)};
}

} // namespace wulfhilde::detail
