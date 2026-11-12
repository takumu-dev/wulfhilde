# volk loads Vulkan at run time, so nothing links the Vulkan loader, and VulkanMemoryAllocator's
# header takes its functions from volk. Slang is a build tool, which the tests' shaders use too.
set(VOLK_PULL_IN_VULKAN OFF)
find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(earcut_hpp CONFIG REQUIRED)
# vcpkg installs build tools for the host, apart from the libraries, where its toolchain doesn't look.
set(slang_hints "")
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_HOST_TRIPLET)
    set(slang_hints "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}")
endif()
find_package(slang 2026.19 CONFIG REQUIRED GLOBAL HINTS ${slang_hints})

include(WulfhildeShaders)

wulfhilde_add_module(renderer
    HEADERS
        include/wulfhilde/renderer/camera.hpp
        include/wulfhilde/renderer/device_error.hpp
        include/wulfhilde/renderer/extent.hpp
        include/wulfhilde/renderer/image.hpp
        include/wulfhilde/renderer/renderer.hpp
        include/wulfhilde/renderer/renderer_settings.hpp
    SOURCES
        barriers.hpp
        bindless_descriptors.cpp
        bindless_descriptors.hpp
        device.cpp
        device.hpp
        drawing.cpp
        drawing.hpp
        formats.hpp
        image.cpp
        instance.cpp
        instance.hpp
        memory.cpp
        memory.hpp
        owned_handle.hpp
        pipelines.cpp
        pipelines.hpp
        renderer.cpp
        required_profile.cpp
        required_profile.hpp
        sector_geometry.cpp
        sector_geometry.hpp
        vma.cpp
        vulkan.cpp
        vulkan.hpp
    DEPENDS
        world
        assets
    PACKAGES
        volk
)
# VulkanMemoryAllocator and earcut are headers alone, compiled into the renderer, so a game never
# needs them.
target_link_libraries(wulfhilde_renderer PRIVATE
    volk::volk
    $<BUILD_INTERFACE:GPUOpen::VulkanMemoryAllocator>
    $<BUILD_INTERFACE:earcut_hpp::earcut_hpp>
)
# Vulkan's C structures are filled with designated initialisers that leave every other member zero,
# as Vulkan intends, which GCC warns of member by member.
function(wulfhilde_renderer_settings target)
    target_compile_definitions(${target} PRIVATE
        VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    )
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE -Wno-missing-field-initializers)
    endif()
endfunction()
wulfhilde_renderer_settings(wulfhilde_renderer)

wulfhilde_add_shaders(wulfhilde_renderer
    SOURCES
        shaders/resolve.slang
        shaders/sectors.slang
    MODULE_DIRECTORIES
        shaders
)

set(WULFHILDE_SHADER_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/shaders" CACHE INTERNAL
    "Where the renderer's shader modules are, for the shaders that import them"
)

# The renderer's tests' way into it, which runs the shaders they supply on a device made as the
# renderer makes its own. It is built only with the tests, and never installed.
if(WULFHILDE_BUILD_TESTS)
    add_library(wulfhilde_renderer_testing STATIC testing/test_device.cpp)
    target_include_directories(wulfhilde_renderer_testing
        PUBLIC testing/include
        PRIVATE .
    )
    target_link_libraries(wulfhilde_renderer_testing
        PUBLIC wulfhilde::renderer
        PRIVATE volk::volk GPUOpen::VulkanMemoryAllocator
    )
    wulfhilde_target_conventions(wulfhilde_renderer_testing)
    wulfhilde_renderer_settings(wulfhilde_renderer_testing)
endif()
