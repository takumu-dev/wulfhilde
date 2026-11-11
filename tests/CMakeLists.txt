# The engine's tests. Each one carries one CTest label: `unit` (CPU only), `gpu` (a Vulkan device)
# or `image` (compared with a reference image). Each test's name begins with the module it tests,
# matching the tag on its Catch2 test case, or with the part of the build it tests: `build` for
# the configuration and `consumers` for the package.

find_package(Catch2 3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
include(Catch)

set(WULFHILDE_TEST_SUPPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/support")

# Adds a module's Catch2 test cases as one program, registering each case with CTest under the
# module's name.
#
#   SOURCES    the program's sources
#   LIBRARIES  what it links
#   LABEL      the label of every case: unit, the default; gpu, for cases that use a Vulkan device
#              under validation; or image, for cases compared with reference images
#
# A module's program of `unit` tests is wulfhilde_<module>_tests, and one of another label
# wulfhilde_<module>_<label>_tests.
function(wulfhilde_add_tests module)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "LABEL" "SOURCES;LIBRARIES")
    if(NOT DEFINED arg_LABEL)
        set(arg_LABEL unit)
    endif()
    if(arg_LABEL STREQUAL "unit")
        set(target wulfhilde_${module}_tests)
    else()
        set(target wulfhilde_${module}_${arg_LABEL}_tests)
    endif()
    add_executable(${target} ${arg_SOURCES})
    target_include_directories(${target} PRIVATE "${WULFHILDE_TEST_SUPPORT_DIR}")
    target_link_libraries(${target} PRIVATE ${arg_LIBRARIES} Catch2::Catch2WithMain)
    wulfhilde_target_warnings(${target})
    set(properties LABELS ${arg_LABEL})
    if(NOT arg_LABEL STREQUAL "unit")
        list(APPEND properties ENVIRONMENT_MODIFICATION "${WULFHILDE_GPU_TEST_ENVIRONMENT}")
    endif()
    catch_discover_tests(${target}
        TEST_PREFIX "${module}: "
        PROPERTIES ${properties}
    )
endfunction()

# The validation layers that the tests' vcpkg feature installs, for the Vulkan loader to find
# beside any installed on the machine. A gpu or image test that can't find them fails.
set(WULFHILDE_GPU_TEST_ENVIRONMENT "")
if(DEFINED VCPKG_INSTALLED_DIR)
    set(host_tree "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}")
    if(WIN32)
        # The manifest names the library beside it.
        set(layers "${host_tree}/bin")
    else()
        # The manifest names the library alone, for the system's library path to find, so a copy
        # names it where vcpkg installed it.
        set(layers "${CMAKE_CURRENT_BINARY_DIR}/validation-layers")
        set(manifest "${host_tree}/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json")
        file(READ "${manifest}" layer)
        string(JSON library GET "${layer}" layer library_path)
        string(JSON layer SET "${layer}" layer library_path "\"${host_tree}/lib/${library}\"")
        file(WRITE "${layers}/VkLayer_khronos_validation.json" "${layer}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
    endif()
    set(WULFHILDE_GPU_TEST_ENVIRONMENT "VK_ADD_LAYER_PATH=path_list_prepend:${layers}")
endif()

# The image tests' harness, which compares what they draw with their reference images.
find_package(lodepng CONFIG REQUIRED)
add_library(wulfhilde_image_comparison STATIC support/image_comparison.cpp)
target_include_directories(wulfhilde_image_comparison PUBLIC support)
target_link_libraries(wulfhilde_image_comparison PUBLIC wulfhilde::renderer PRIVATE lodepng)
wulfhilde_target_warnings(wulfhilde_image_comparison)

# Builds a test program of a module from one source, naming the target after the source.
function(wulfhilde_add_test_program out_target module source)
    cmake_path(GET source STEM stem)
    set(target wulfhilde_${module}_${stem})
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${ARGN})
    wulfhilde_target_warnings(${target})
    set(${out_target} ${target} PARENT_SCOPE)
endfunction()

# Adds a test that runs a program and checks how it ends, through support/check_program.cmake.
#
#   SOURCE     the program's one source file; or PROGRAM, a program target built elsewhere
#   LIBRARIES  what the program links
#   ARGUMENTS, EXPECTED_RESULT, EXPECTED_STDERR, STDOUT_MATCHES, STDERR_MATCHES
#              passed to the script, which describes them
function(wulfhilde_add_program_test module name)
    set(checks EXPECTED_RESULT EXPECTED_STDERR STDOUT_MATCHES STDERR_MATCHES)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "SOURCE;PROGRAM;${checks}" "LIBRARIES;ARGUMENTS")
    if(DEFINED arg_SOURCE)
        wulfhilde_add_test_program(arg_PROGRAM ${module} ${arg_SOURCE} ${arg_LIBRARIES})
    endif()
    if(DEFINED arg_EXPECTED_STDERR)
        cmake_path(ABSOLUTE_PATH arg_EXPECTED_STDERR)
    endif()

    set(options "-DPROGRAM=$<TARGET_FILE:${arg_PROGRAM}>")
    if(DEFINED arg_ARGUMENTS)
        list(JOIN arg_ARGUMENTS "$<SEMICOLON>" arguments)
        list(APPEND options "-DARGUMENTS=${arguments}")
    endif()
    foreach(check IN LISTS checks)
        if(DEFINED arg_${check})
            list(APPEND options "-D${check}=${arg_${check}}")
        endif()
    endforeach()

    set(test_name "${module}: ${name}")
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" ${options} -P "${WULFHILDE_TEST_SUPPORT_DIR}/check_program.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
endfunction()

# The main program that every precondition test shares.
add_library(wulfhilde_precondition_main OBJECT support/precondition_main.cpp)
target_include_directories(wulfhilde_precondition_main PUBLIC support)
target_link_libraries(wulfhilde_precondition_main PUBLIC wulfhilde::core)
wulfhilde_target_warnings(wulfhilde_precondition_main)

# Adds a test that a precondition fires. A failed check ends the process, so each such test is a
# program of its own, which CTest expects to fail. The source defines break_precondition() from
# support/precondition_test.hpp.
#
#   SOURCE     the program's one source file
#   LIBRARIES  what the program links besides core
function(wulfhilde_add_precondition_test module name)
    cmake_parse_arguments(PARSE_ARGV 2 arg "" "SOURCE" "LIBRARIES")
    wulfhilde_add_test_program(target ${module} ${arg_SOURCE}
        wulfhilde_precondition_main ${arg_LIBRARIES}
    )
    set(test_name "${module}: ${name}")
    add_test(NAME "${test_name}" COMMAND ${target})
    set_tests_properties("${test_name}" PROPERTIES LABELS unit WILL_FAIL TRUE)
endfunction()

add_subdirectory(cmake)
add_subdirectory(consumers)
add_subdirectory(core)
add_subdirectory(world)
add_subdirectory(assets)
add_subdirectory(renderer)

if(WULFHILDE_BUILD_VIEWER)
    add_subdirectory(viewer)
endif()
