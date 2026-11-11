# vcpkg's x64-linux-release, for the build tools of the Linux presets: static libraries, built for
# release only. The validation layers are built without the window systems' surfaces, which only a
# test that opens a window would validate, so that building them needs no X11 or Wayland headers.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)

if(PORT STREQUAL "vulkan-validationlayers")
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS
        -DBUILD_WSI_XCB_SUPPORT=OFF
        -DBUILD_WSI_XLIB_SUPPORT=OFF
        -DBUILD_WSI_XLIB_XRANDR_SUPPORT=OFF
        -DBUILD_WSI_WAYLAND_SUPPORT=OFF
    )
endif()
