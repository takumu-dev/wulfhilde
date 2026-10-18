# The configuration's own behaviour, tested by running its checks as scripts.

# Adds a test that runs the compiler floor check on one compiler. A compiler below its floor must
# stop the configuration with a message naming the floor; one at its floor must pass.
function(wulfhilde_add_floor_test name id version)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "FAILS_WITH" "")
    set(test_name "build: ${name}")
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMPILER_ID=${id}"
            "-DCOMPILER_VERSION=${version}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_compiler_floor.cmake"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(DEFINED arg_FAILS_WITH)
        # The message is the behaviour under test: CTest then passes on it alone, whatever the
        # exit status, and only FATAL_ERROR prints it.
        set_tests_properties("${test_name}" PROPERTIES PASS_REGULAR_EXPRESSION "${arg_FAILS_WITH}")
    endif()
endfunction()

wulfhilde_add_floor_test("configuring with GCC 15 fails naming the floor" GNU 15.2.0
    FAILS_WITH "wulfhilde needs GCC 16 or newer; this is GCC 15\\.2\\.0"
)
wulfhilde_add_floor_test("configuring with GCC 16 passes" GNU 16.0.0)
wulfhilde_add_floor_test("configuring with Clang 22 fails naming the floor" Clang 22.1.8
    FAILS_WITH "wulfhilde needs Clang 23 or newer; this is Clang 22\\.1\\.8"
)
wulfhilde_add_floor_test("configuring with Clang 23 passes" Clang 23.0.0)
wulfhilde_add_floor_test("configuring with MSVC 14.51 fails naming the floor" MSVC 19.51.36231
    FAILS_WITH "wulfhilde needs MSVC 14\\.52 \\(cl 19\\.52\\) or newer; this is cl 19\\.51\\.36231"
)
wulfhilde_add_floor_test("configuring with MSVC 14.52 passes" MSVC 19.52.0)
wulfhilde_add_floor_test("configuring with an unsupported compiler fails naming the supported ones"
    AppleClang 21.0.0
    FAILS_WITH "wulfhilde supports GCC, Clang and MSVC; this compiler is AppleClang 21\\.0\\.0"
)
