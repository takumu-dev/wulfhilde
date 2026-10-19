# The two ways in for a game: the installed package and the subproject. Each consumer is a project
# of its own, which CTest configures, builds and runs with this build's compiler and flags.

set(prefix "${CMAKE_CURRENT_BINARY_DIR}/prefix")

add_test(NAME "consumers: the engine installs"
    COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}" --prefix "${prefix}" --config $<CONFIG>
)
set_tests_properties("consumers: the engine installs" PROPERTIES
    FIXTURES_SETUP wulfhilde_installed
    LABELS unit
)

# The install rules come after the tests, so the install directories are named here as well.
include(GNUInstallDirs)
add_test(NAME "consumers: the installed README's images are installed beside it"
    COMMAND "${CMAKE_COMMAND}"
        "-DREADME=${prefix}/${CMAKE_INSTALL_DOCDIR}/README.md"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_readme_images.cmake"
)
set_tests_properties("consumers: the installed README's images are installed beside it" PROPERTIES
    FIXTURES_REQUIRED wulfhilde_installed
    LABELS unit
)

# Adds a test that builds the consumer project in `source_dir` and runs the program it builds.
#
#   OPTIONS            more cache settings for the consumer
#   INSTALLED          the consumer needs the installed engine
#   FAILS_MATCHING     the consumer must fail to configure, with output matching this
function(wulfhilde_add_consumer_test name source_dir)
    cmake_parse_arguments(PARSE_ARGV 2 arg "INSTALLED" "FAILS_MATCHING" "OPTIONS")
    set(test_name "consumers: ${name}")
    if(arg_INSTALLED)
        list(APPEND arg_OPTIONS "-DCMAKE_PREFIX_PATH=${prefix}")
    endif()
    string(MAKE_C_IDENTIFIER "${name}" binary_dir)
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_CTEST_COMMAND}"
            --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/${source_dir}"
                "${CMAKE_CURRENT_BINARY_DIR}/${binary_dir}"
            --build-generator "${CMAKE_GENERATOR}"
            --build-makeprogram "${CMAKE_MAKE_PROGRAM}"
            --build-config $<CONFIG>
            --build-options
                -DCMAKE_BUILD_TYPE=$<CONFIG>
                "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
                "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
                "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
                ${arg_OPTIONS}
            --test-command consumer
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(arg_INSTALLED)
        set_tests_properties("${test_name}" PROPERTIES FIXTURES_REQUIRED wulfhilde_installed)
    endif()
    if(DEFINED arg_FAILS_MATCHING)
        set_tests_properties("${test_name}" PROPERTIES PASS_REGULAR_EXPRESSION "${arg_FAILS_MATCHING}")
    endif()
endfunction()

wulfhilde_add_consumer_test("a game links the installed engine through the umbrella target" installed
    INSTALLED
)
# world is a subset that needs core, which comes with it.
wulfhilde_add_consumer_test("a game links a subset of the installed engine's modules" installed
    INSTALLED
    OPTIONS -DWULFHILDE_COMPONENTS=world
)
wulfhilde_add_consumer_test("a game asking for a module the engine lacks is told so" installed
    INSTALLED
    OPTIONS -DWULFHILDE_COMPONENTS=core$<SEMICOLON>flight
    FAILS_MATCHING "wulfhilde has no module named flight; its modules are: core, world"
)
wulfhilde_add_consumer_test("a game builds the engine as a subproject" subproject
    OPTIONS "-DWULFHILDE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
)
