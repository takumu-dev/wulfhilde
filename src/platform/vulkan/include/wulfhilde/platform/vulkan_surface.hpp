// What Vulkan needs from the platform to draw into a window. Only the build sees this header,
// through the wulfhilde_platform_vulkan target that the renderer links, so no installed header
// names Vulkan.
#pragma once

#include <wulfhilde/platform/platform_error.hpp>
#include <wulfhilde/platform/window.hpp>
#include <wulfhilde/platform/window_size.hpp>

// Whoever calls Vulkan loads its functions at run time, as the renderer does through volk, so the
// header declares none of them.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan_core.h>

#include <expected>
#include <span>

namespace wulfhilde::detail {

/// Returns the instance extensions that Vulkan needs to make a surface for a window of this
/// platform's, or none if the platform can't say.
///
/// \param window A window, which shows that video is up.
[[nodiscard]] std::span<const char* const> vulkan_surface_extensions(const Window& window);

/// Makes a Vulkan surface that presents into a window.
///
/// \param window The window, which must outlive the surface.
/// \param instance An instance made with every extension `vulkan_surface_extensions` names, which
///                 destroys the surface with `vkDestroySurfaceKHR`.
/// \return The surface, or why the window has none.
[[nodiscard]] std::expected<VkSurfaceKHR, PlatformError> create_vulkan_surface(const Window& window,
                                                                               VkInstance instance);

/// Returns the size of a window's drawable area in pixels, which the images it shows match.
///
/// \param window The window.
[[nodiscard]] WindowSize pixel_size(const Window& window);

} // namespace wulfhilde::detail
