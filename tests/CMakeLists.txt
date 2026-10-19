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
function(wulfhilde_add_tests module)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBRARIES")
    set(target wulfhilde_${module}_tests)
    add_executable(${target} ${arg_SOURCES})
    target_include_directories(${target} PRIVATE "${WULFHILDE_TEST_SUPPORT_DIR}")
    target_link_libraries(${target} PRIVATE ${arg_LIBRARIES} Catch2::Catch2WithMain)
    wulfhilde_target_warnings(${target})
    catch_discover_tests(${target}
        TEST_PREFIX "${module}: "
        PROPERTIES LABELS unit
    )
endfunction()

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

if(WULFHILDE_BUILD_VIEWER)
    add_subdirectory(viewer)
endif()
