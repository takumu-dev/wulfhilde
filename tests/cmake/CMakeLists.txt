# The configuration's own behaviour, tested by running its checks as scripts.

# Adds a test that runs the compiler floor check on one compiler. A compiler below its floor must
# stop the configuration with a message naming the floor (FAILS_WITH); one at its floor must pass;
# one the engine doesn't support must pass with a warning (WARNS_WITH).
function(wulfhilde_add_floor_test name id version)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "FAILS_WITH;WARNS_WITH" "")
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
    if(DEFINED arg_WARNS_WITH)
        set_tests_properties("${test_name}" PROPERTIES
            PASS_REGULAR_EXPRESSION "CMake Warning.*${arg_WARNS_WITH}"
            FAIL_REGULAR_EXPRESSION "CMake Error"
        )
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
wulfhilde_add_floor_test("configuring with an unsupported compiler warns, naming the supported ones"
    AppleClang 21.0.0
    WARNS_WITH "wulfhilde supports GCC, Clang and MSVC; AppleClang 21\\.0\\.0 is untested"
)

# The licence check, run as a script on vcpkg installed trees: some written here into the build
# tree, and one in licenses/notices/ shaped as vcpkg writes it.
set(WULFHILDE_LICENSE_CHECK "${PROJECT_SOURCE_DIR}/cmake/check_licenses.cmake")

# Adds a test that runs the licence check with the options in ARGN, against the licences file
# `licenses`, or the engine's own if that is empty. The check must pass, or, if `fails_with` isn't
# empty, fail with output matching it.
function(wulfhilde_add_license_check_test name licenses fails_with)
    set(options ${ARGN})
    if(licenses)
        cmake_path(ABSOLUTE_PATH licenses)
        list(APPEND options "-DLICENSES=${licenses}")
    endif()
    set(test_name "build: ${name}")
    add_test(NAME "${test_name}"
        COMMAND "${CMAKE_COMMAND}" ${options} -P "${WULFHILDE_LICENSE_CHECK}"
    )
    set_tests_properties("${test_name}" PROPERTIES LABELS unit)
    if(fails_with)
        set_tests_properties("${test_name}" PROPERTIES PASS_REGULAR_EXPRESSION "${fails_with}")
    endif()
endfunction()

# Adds a test that runs the licence check on an installed tree holding PORTS, each written
# "<name>:<triplet> <version> <licence expression>" with the version as vcpkg prints it. A port
# that declares no licence has the expression NOASSERTION, as vcpkg writes it. The check must pass,
# or fail with output matching FAILS_WITH.
#
#   LICENSES  the licences file to check against; the engine's own by default
function(wulfhilde_add_license_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "LICENSES;FAILS_WITH" "PORTS")
    string(MAKE_C_IDENTIFIER "${name}" id)
    set(installed "${CMAKE_CURRENT_BINARY_DIR}/licenses/${id}")
    file(REMOVE_RECURSE "${installed}")
    file(MAKE_DIRECTORY "${installed}/vcpkg")

    set(status "")
    foreach(port IN LISTS arg_PORTS)
        if(NOT port MATCHES "^([^:]+):([^ ]+) ([^ ]+) (.+)$")
            message(FATAL_ERROR
                "A test port reads \"<name>:<triplet> <version> <licence>\", not: ${port}"
            )
        endif()
        set(port_name "${CMAKE_MATCH_1}")
        set(triplet "${CMAKE_MATCH_2}")
        set(version "${CMAKE_MATCH_3}")
        set(license "${CMAKE_MATCH_4}")
        string(APPEND status
            "Package: ${port_name}\nArchitecture: ${triplet}\nStatus: install ok installed\n\n"
        )
        string(CONFIGURE [[
{
  "packages": [
    {
      "name": "@port_name@",
      "SPDXID": "SPDXRef-port",
      "versionInfo": "@version@",
      "licenseConcluded": "@license@"
    }
  ]
}
]] spdx @ONLY)
        file(WRITE "${installed}/${triplet}/share/${port_name}/vcpkg.spdx.json" "${spdx}")
    endforeach()
    if(status)
        file(WRITE "${installed}/vcpkg/status" "${status}")
    endif()

    wulfhilde_add_license_check_test("${name}" "${arg_LICENSES}" "${arg_FAILS_WITH}"
        "-DINSTALLED_DIR=${installed}"
    )
endfunction()

wulfhilde_add_license_test(
    "a dependency under a licence outside the allowlist fails the check, naming it"
    PORTS
        "zlib:x64-linux 1.3.2#2 Zlib"
        "readline:x64-linux 8.2 GPL-3.0-or-later"
    FAILS_WITH "readline 8\\.2 is under GPL-3\\.0-or-later, which the allowlist doesn't cover\\."
)
wulfhilde_add_license_test("a dependency under any licence the allowlist names passes the check"
    PORTS
        "zlib:x64-linux 1.3.2#2 Zlib"
        "fmt:x64-linux 12.1.0 MIT"
        "pcg:x64-linux 2022-04-09 BSD-2-Clause"
        "lz4:x64-linux 1.10.0 BSD-3-Clause"
        "catch2:x64-linux 3.16.0 BSL-1.0"
        "earcut-hpp:x64-linux 3.2.4 ISC"
        "spirv-tools:x64-linux 1.4.335.0 Apache-2.0"
        "shader-slang:x64-linux-release 2026.19 Apache-2.0 WITH LLVM-exception"
        "stb:x64-linux 2024-07-29 CC0-1.0"
        "parallel-hashmap:x64-linux 2.0.0 Unlicense"
)
wulfhilde_add_license_test(
    "the check names every dependency under a copyleft licence, not only the first"
    PORTS
        "readline:x64-linux 8.2 GPL-3.0-or-later"
        "zlib:x64-linux 1.3.2#2 Zlib"
        "libiconv:x64-linux 1.18 LGPL-2.1-or-later"
        "eigen3:x64-linux 3.4.1 MPL-2.0"
    FAILS_WITH "\n +- eigen3 3\\.4\\.1 [^\n]*\n +- libiconv 1\\.18 [^\n]*\n +- readline 8\\.2 "
)
wulfhilde_add_license_test(
    "a dependency under a choice of licences passes the check when the allowlist covers one"
    PORTS
        "highway:x64-linux 1.3.0 Apache-2.0 OR BSD-3-Clause"
        "mimalloc:x64-linux 3.1.5 GPL-3.0-only OR MIT"
)
wulfhilde_add_license_test(
    "a dependency under several licences at once passes the check when the allowlist covers each"
    PORTS "sdl3:x64-linux 3.4.18 Zlib AND MIT AND Apache-2.0 AND BSD-3-Clause"
)
wulfhilde_add_license_test(
    "a dependency under several licences at once fails the check when the allowlist misses one"
    PORTS "qhull:x64-linux 8.0.2 MIT AND GPL-3.0-only"
    FAILS_WITH "qhull 8\\.0\\.2 is under MIT AND GPL-3\\.0-only, which the allowlist doesn't cover"
)
wulfhilde_add_license_test(
    "a licence with an exception fails the check unless allowed with that exception"
    PORTS
        "shader-slang:x64-linux-release 2026.19 Apache-2.0 WITH LLVM-exception"
        "openjdk-headers:x64-linux 21 Apache-2.0 WITH Classpath-exception-2.0"
    FAILS_WITH "openjdk-headers 21 is under Apache-2\\.0 WITH Classpath-exception-2\\.0, which"
)
wulfhilde_add_license_test("in a licence expression, AND binds tighter than OR"
    PORTS "libfoo:x64-linux 1.0 Zlib OR MIT AND GPL-3.0-only"
)
wulfhilde_add_license_test("in a licence expression, parentheses group licences"
    PORTS "libbar:x64-linux 1.0 (Zlib OR MIT) AND GPL-3.0-only"
    FAILS_WITH "libbar 1\\.0 is under \\(Zlib OR MIT\\) AND GPL-3\\.0-only, which"
)
wulfhilde_add_license_test("a licence expression that can't be read fails the check"
    PORTS
        "unclosed:x64-linux 1.0 (MIT OR Zlib"
        "dangling:x64-linux 1.0 MIT AND"
        "bare-exception:x64-linux 1.0 WITH LLVM-exception"
    FAILS_WITH "\n +- bare-exception 1\\.0 [^\n]*\n +- dangling 1\\.0 [^\n]*\n +- unclosed 1\\.0 "
)
wulfhilde_add_license_test("a dependency that declares no licence fails the check, naming it"
    PORTS "lodepng:x64-linux 2021-12-04#1 NOASSERTION"
    FAILS_WITH "lodepng 2021-12-04#1 declares no licence\\. Review it, and add an override for"
)
wulfhilde_add_license_test(
    "a dependency whose port sets its licence to null fails the check, naming it"
    PORTS "shader-slang:x64-linux-release 2026.19 LicenseRef-vcpkg-null"
    FAILS_WITH "shader-slang 2026\\.19 declares no licence\\. Review it, and add an override for"
)
wulfhilde_add_license_test("an override gives the licence of a dependency that declares none"
    LICENSES licenses/overrides.json
    PORTS "lodepng:x64-linux 2021-12-04#1 NOASSERTION"
)
wulfhilde_add_license_test(
    "an override reviewed at another version of its dependency fails the check, naming it"
    LICENSES licenses/overrides.json
    PORTS "lodepng:x64-linux 2025-06-01 Zlib"
    FAILS_WITH
        "lodepng 2025-06-01 isn't the version its override was reviewed at, 2021-12-04#1\\."
)
wulfhilde_add_license_test(
    "a dependency whose override gives a licence outside the allowlist fails the check, naming it"
    LICENSES licenses/overrides.json
    PORTS "relicensed:x64-linux 1.0 MIT"
    FAILS_WITH "relicensed 1\\.0 is under GPL-3\\.0-only by its override, which the allowlist"
)
wulfhilde_add_license_test("an override for a dependency that isn't installed is left alone"
    LICENSES licenses/overrides.json
    PORTS "zlib:x64-linux 1.3.2#2 Zlib"
)
wulfhilde_add_license_test("an override without the version it was reviewed at fails the check"
    LICENSES licenses/unpinned-override.json
    FAILS_WITH "The override for lodepng in unpinned-override\\.json lacks its version\\."
)
wulfhilde_add_license_test(
    "a build tool under a licence outside the allowlist fails the check too, naming it"
    PORTS
        "zlib:x64-linux 1.3.2#2 Zlib"
        "bison:x64-linux-release 3.8.2 GPL-3.0-or-later"
    FAILS_WITH "bison 3\\.8\\.2 is under GPL-3\\.0-or-later, which the allowlist doesn't cover\\."
)
wulfhilde_add_license_test("a tree with nothing installed passes the check")

wulfhilde_add_license_check_test(
    "the check fails on a directory that holds no vcpkg installed tree"
    ""
    "No vcpkg installed tree is at[ \n]+.*/licenses/nothing\\."
    "-DINSTALLED_DIR=${CMAKE_CURRENT_BINARY_DIR}/licenses/nothing"
)

# The notices, tested on the fixture in licenses/notices/: ports for x64-linux, with their build
# tools for x64-linux-release. Its THIRD_PARTY_NOTICES.md is written by hand.
set(WULFHILDE_NOTICES_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/licenses/notices")

# Adds a test that runs the licence check on the fixture, and checks that the notices file NOTICES
# lists what ships of the dependencies in MANIFEST, each the fixture's own by default. The check
# must pass, or fail with output matching FAILS_WITH.
#
#   LICENSES      the licences file; the engine's own by default
#   HOST_TRIPLET  the triplet of the build tools; x64-linux-release by default
function(wulfhilde_add_notices_test name)
    cmake_parse_arguments(PARSE_ARGV 1 arg ""
        "MANIFEST;NOTICES;LICENSES;HOST_TRIPLET;FAILS_WITH" ""
    )
    if(NOT DEFINED arg_MANIFEST)
        set(arg_MANIFEST "${WULFHILDE_NOTICES_FIXTURE}/vcpkg.json")
    endif()
    if(NOT DEFINED arg_NOTICES)
        set(arg_NOTICES "${WULFHILDE_NOTICES_FIXTURE}/THIRD_PARTY_NOTICES.md")
    endif()
    if(NOT DEFINED arg_HOST_TRIPLET)
        set(arg_HOST_TRIPLET x64-linux-release)
    endif()
    cmake_path(ABSOLUTE_PATH arg_MANIFEST)
    cmake_path(ABSOLUTE_PATH arg_NOTICES)
    wulfhilde_add_license_check_test("${name}" "${arg_LICENSES}" "${arg_FAILS_WITH}"
        "-DINSTALLED_DIR=${WULFHILDE_NOTICES_FIXTURE}/installed"
        "-DMANIFEST=${arg_MANIFEST}"
        -DTRIPLET=x64-linux
        "-DHOST_TRIPLET=${arg_HOST_TRIPLET}"
        "-DCHECK_NOTICES=${arg_NOTICES}"
    )
endfunction()

wulfhilde_add_notices_test(
    "the notices list what the default dependencies and their features ship, and nothing else"
)
wulfhilde_add_notices_test("notices that lack a dependency which ships fail the check, naming it"
    MANIFEST licenses/notices/more-dependencies.json
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md lacks test-framework 3\\.16\\.0, which ships\\."
)
wulfhilde_add_notices_test(
    "notices that list a dependency which no longer ships fail the check, naming it"
    MANIFEST licenses/notices/fewer-dependencies.json
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md lists pngish 1\\.6\\.59, which no longer ships\\."
)
wulfhilde_add_notices_test(
    "the notices fail on a default dependency of another platform, which they would leave out"
    MANIFEST licenses/notices/other-platform.json
    FAILS_WITH "windows-only isn't installed for x64-linux, so the notices would leave it out\\."
)
wulfhilde_add_notices_test("notices with an out-of-date entry fail the check, naming its dependency"
    LICENSES licenses/notices/relicensed.json
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md is out of date for zlibish 1\\.3\\.2#2\\."
)

set(edited "${CMAKE_CURRENT_BINARY_DIR}/licenses/edited/THIRD_PARTY_NOTICES.md")
file(READ "${WULFHILDE_NOTICES_FIXTURE}/THIRD_PARTY_NOTICES.md" notices)
string(REPLACE "rather than editing it." "or edit it by hand." notices "${notices}")
file(WRITE "${edited}" "${notices}")
wulfhilde_add_notices_test("notices edited outside the dependencies' entries fail the check"
    NOTICES "${edited}"
    FAILS_WITH "THIRD_PARTY_NOTICES\\.md differs outside the dependencies' entries\\."
)

wulfhilde_add_notices_test("the notices fail when the build tools share the triplet of what ships"
    HOST_TRIPLET x64-linux
    FAILS_WITH "HOST_TRIPLET and TRIPLET are both x64-linux\\. Give the build tools a triplet"
)

set(written "${CMAKE_CURRENT_BINARY_DIR}/licenses/written/THIRD_PARTY_NOTICES.md")
set(writing "the notices are written for what the default dependencies ship")
wulfhilde_add_license_check_test("${writing}" "" ""
    "-DINSTALLED_DIR=${WULFHILDE_NOTICES_FIXTURE}/installed"
    "-DMANIFEST=${WULFHILDE_NOTICES_FIXTURE}/vcpkg.json"
    -DTRIPLET=x64-linux
    -DHOST_TRIPLET=x64-linux-release
    "-DWRITE_NOTICES=${written}"
)
set_tests_properties("build: ${writing}" PROPERTIES FIXTURES_SETUP wulfhilde_written_notices)
# file(WRITE) ends lines as the platform does, and git ends them with LF in what it commits.
set(comparing "build: the notices written are those checked")
add_test(NAME "${comparing}"
    COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol
        "${written}" "${WULFHILDE_NOTICES_FIXTURE}/THIRD_PARTY_NOTICES.md"
)
set_tests_properties("${comparing}"
    PROPERTIES LABELS unit FIXTURES_REQUIRED wulfhilde_written_notices
)

# The engine's own notices, in the configuration whose ports they list.
if(WULFHILDE_CHECK_NOTICES)
    wulfhilde_add_license_check_test("THIRD_PARTY_NOTICES.md lists the ports that ship" "" ""
        ${WULFHILDE_NOTICES_OPTIONS}
        "-DCHECK_NOTICES=${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    )
endif()
