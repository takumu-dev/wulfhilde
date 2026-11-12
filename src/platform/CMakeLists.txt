# SDL3 stays inside the platform: no SDL type reaches a public header. The renderer reaches a
# window's Vulkan surface through vulkan/include/wulfhilde/platform/vulkan_surface.hpp, which only
# the build sees, so the installed headers never name Vulkan.
find_package(SDL3 CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)

wulfhilde_add_module(platform
    HEADERS
        include/wulfhilde/platform/event.hpp
        include/wulfhilde/platform/key.hpp
        include/wulfhilde/platform/platform.hpp
        include/wulfhilde/platform/platform_error.hpp
        include/wulfhilde/platform/window.hpp
        include/wulfhilde/platform/window_size.hpp
    SOURCES
        native_window.hpp
        platform.cpp
        sdl_error.hpp
        vulkan/include/wulfhilde/platform/vulkan_surface.hpp
        vulkan_surface.cpp
        window.cpp
    DEPENDS
        core
    PACKAGES
        SDL3
)
target_include_directories(wulfhilde_platform PRIVATE vulkan/include)
target_link_libraries(wulfhilde_platform PRIVATE
    SDL3::SDL3
    $<BUILD_INTERFACE:Vulkan::Headers>
)

# The renderer's way to a window's Vulkan surface, which only it links. It is never installed.
add_library(wulfhilde_platform_vulkan INTERFACE)
target_include_directories(wulfhilde_platform_vulkan INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vulkan/include>"
)
target_link_libraries(wulfhilde_platform_vulkan INTERFACE wulfhilde::platform Vulkan::Headers)
