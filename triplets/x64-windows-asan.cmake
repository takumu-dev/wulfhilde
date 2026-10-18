# x64-windows-static-md with AddressSanitizer, for the MSVC ASan preset: static libraries on the
# dynamic CRT, so every library in the program is instrumented.
#
# MSVC refuses /fsanitize=address beside the /RTC1 that vcpkg puts in every debug build, so only
# release libraries are built, and the preset builds the engine against the release CRT.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

set(VCPKG_C_FLAGS "/fsanitize=address")
set(VCPKG_CXX_FLAGS "/fsanitize=address")
