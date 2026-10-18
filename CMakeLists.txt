# wulfhilde: a library of static modules, an umbrella target that links them all, and
# wulfhilde-viewer, the map-preview tool, as a client of the installed headers like any game.
#
# Every supported build is a preset in CMakePresets.json, so a developer and CI run the same
# `cmake --workflow --preset <preset>`. Dependencies come only through find_package; the build never
# downloads anything.
cmake_minimum_required(VERSION 4.4)

project(wulfhilde
    VERSION 0.1.0
    DESCRIPTION "A library for games built from sectors joined by portals"
    HOMEPAGE_URL "https://github.com/takumu-dev/wulfhilde"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(WulfhildeCompilerFloor)
include(WulfhildeConventions)
include(WulfhildeModules)

wulfhilde_check_compiler_floor("${CMAKE_CXX_COMPILER_ID}" "${CMAKE_CXX_COMPILER_VERSION}")

# Standard C++23 as ordinary headers: no compiler extensions, and no scanning for C++ modules,
# which the engine doesn't use. Each module's target asks for C++23 itself.
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# A game that adds the engine as a subproject gets the libraries alone.
option(WULFHILDE_BUILD_TESTS "Build the engine's tests." ${PROJECT_IS_TOP_LEVEL})
option(WULFHILDE_BUILD_VIEWER "Build wulfhilde-viewer, the map-preview tool." ${PROJECT_IS_TOP_LEVEL})
option(WULFHILDE_INSTALL "Install the engine's package and the viewer." ${PROJECT_IS_TOP_LEVEL})
option(WULFHILDE_EXCEPTIONS
    "Build the engine and the viewer with exceptions enabled. The engine never throws, so it builds either way."
    ON
)

add_subdirectory(src)

if(WULFHILDE_BUILD_VIEWER)
    add_subdirectory(apps/viewer)
endif()

if(WULFHILDE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(WULFHILDE_INSTALL)
    include(WulfhildeInstall)
endif()

if(PROJECT_IS_TOP_LEVEL)
    include(WulfhildeLint)
endif()
