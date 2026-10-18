# Checks the licence of every port in a vcpkg installed tree against the allowlist, and checks or
# writes the third-party notices of the ports that ship.
#
#   cmake -D INSTALLED_DIR=<dir> [-D LICENSES=<file>]
#         [-D MANIFEST=<file> -D TRIPLET=<triplet> -D HOST_TRIPLET=<triplet>
#          (-D CHECK_NOTICES=<file> | -D WRITE_NOTICES=<file>)]
#         -P cmake/check_licenses.cmake
#
#   INSTALLED_DIR  the vcpkg_installed directory, holding every triplet's ports
#   LICENSES       the licences file; cmake/licenses.json beside this script by default
#   MANIFEST       the vcpkg.json whose dependencies ship
#   TRIPLET        the triplet the shipped ports are installed for
#   HOST_TRIPLET   the triplet the build tools are installed for, which must differ from TRIPLET
#   CHECK_NOTICES  a notices file that must match the ports that ship
#   WRITE_NOTICES  a notices file to write for the ports that ship
#
# Each port's licence is the SPDX expression in its share/<port>/vcpkg.spdx.json. The licences file
# holds `allowed`, the licences that need no review, each an SPDX licence or `<licence> WITH
# <exception>`; and `overrides`, each giving a port's licence as a review found it, with the `port`,
# the `version` reviewed as vcpkg prints it, the `license` and the `reason`. Once the port's version
# changes, its override fails the check until it is reviewed again, and the licence an override
# gives must still be allowed. The check fails, naming every port it rejects.
#
# The ports that ship are those that the manifest's default dependencies name, except build tools,
# and every port they depend on for TRIPLET. vcpkg records a port's dependencies on build tools
# under HOST_TRIPLET, and only when it differs from TRIPLET can they be told apart.
cmake_minimum_required(VERSION 4.4)

# Stops with the heading, the problems in ARGN beneath it, sorted, and then the remedy, if any. Each
# problem is indented, so CMake prints it on one line rather than wrapping it, and sets it apart.
function(wulfhilde_fail heading remedy)
    set(problems ${ARGN})
    list(SORT problems)
    list(TRANSFORM problems PREPEND "  - ")
    list(JOIN problems "\n" problems)
    message(FATAL_ERROR "${heading}\n${problems}\n${remedy}")
endfunction()

# Sets `out` to 1 if the licences in `allowed` cover the SPDX licence expression, and to 0
# otherwise. An AND needs every side covered and an OR any one, with AND binding tighter. A licence
# with an exception, `<licence> WITH <exception>`, is covered only if the allowlist names it so. An
# expression that can't be read isn't covered.
function(wulfhilde_license_allowed out expression)
    string(REPLACE "(" " ( " expression "${expression}")
    string(REPLACE ")" " ) " expression "${expression}")
    string(STRIP "${expression}" expression)
    string(REGEX REPLACE "[ \t\n]+" ";" tokens "${expression}")

    # With each licence, and its exception, replaced by whether it is allowed, only AND, OR and
    # parentheses are left to evaluate.
    set(values "")
    list(LENGTH tokens count)
    set(index 0)
    while(index LESS count)
        list(GET tokens ${index} token)
        math(EXPR next "${index} + 1")
        if(token MATCHES "^(AND|OR|\\(|\\))$")
            list(APPEND values "${token}")
        elseif(token STREQUAL "WITH")
            set(${out} 0 PARENT_SCOPE)
            return()
        else()
            set(license "${token}")
            if(next LESS count)
                list(GET tokens ${next} following)
                if(following STREQUAL "WITH")
                    math(EXPR exception "${next} + 1")
                    if(exception GREATER_EQUAL count)
                        set(${out} 0 PARENT_SCOPE)
                        return()
                    endif()
                    list(GET tokens ${exception} exception)
                    string(APPEND license " WITH ${exception}")
                    math(EXPR next "${next} + 2")
                endif()
            endif()
            if(license IN_LIST allowed)
                list(APPEND values 1)
            else()
                list(APPEND values 0)
            endif()
        endif()
        set(index ${next})
    endwhile()
    list(JOIN values " " values)

    # CMake's regular expressions can't match nested parentheses, so the innermost groups go first.
    while(values MATCHES "\\(([^()]*)\\)")
        set(group "${CMAKE_MATCH_0}")
        wulfhilde_flat_license_value(value "${CMAKE_MATCH_1}")
        if(value STREQUAL "")
            set(${out} 0 PARENT_SCOPE)
            return()
        endif()
        string(REPLACE "${group}" "${value}" values "${values}")
    endwhile()
    wulfhilde_flat_license_value(value "${values}")
    if(value STREQUAL "")
        set(value 0)
    endif()
    set(${out} ${value} PARENT_SCOPE)
endfunction()

# Sets `out` to the value of a run of 1s and 0s joined by AND and OR, with no parentheses, or to
# nothing if the run doesn't read as one.
function(wulfhilde_flat_license_value out values)
    string(STRIP "${values}" values)
    if(NOT values MATCHES "^[01]( (AND|OR) [01])*$")
        set(${out} "" PARENT_SCOPE)
        return()
    endif()
    string(REPLACE " OR " ";" alternatives "${values}")
    foreach(alternative IN LISTS alternatives)
        if(NOT alternative MATCHES "0")
            set(${out} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${out} 0 PARENT_SCOPE)
endfunction()

# Appends to the list `out` the names in a manifest's array of dependencies or features, where each
# is a name or an object with one, except dependencies on build tools, which are marked `host`.
function(wulfhilde_manifest_names out array)
    set(names "${${out}}")
    string(JSON count LENGTH "${array}")
    set(index 0)
    while(index LESS count)
        string(JSON type TYPE "${array}" ${index})
        if(type STREQUAL "STRING")
            string(JSON name GET "${array}" ${index})
            list(APPEND names "${name}")
        else()
            string(JSON name GET "${array}" ${index} name)
            string(JSON host ERROR_VARIABLE error GET "${array}" ${index} host)
            if(NOT host)
                list(APPEND names "${name}")
            endif()
        endif()
        math(EXPR index "${index} + 1")
    endwhile()
    set(${out} "${names}" PARENT_SCOPE)
endfunction()

if(NOT DEFINED LICENSES)
    set(LICENSES "${CMAKE_CURRENT_LIST_DIR}/licenses.json")
endif()

file(READ "${LICENSES}" licenses)
cmake_path(GET LICENSES FILENAME licenses_name)
set(allowed "")
string(JSON count LENGTH "${licenses}" allowed)
set(index 0)
while(index LESS count)
    string(JSON license GET "${licenses}" allowed ${index})
    list(APPEND allowed "${license}")
    math(EXPR index "${index} + 1")
endwhile()

# CMake has no maps, so each override is looked up by its port, in override_license_<port> and
# override_version_<port>.
string(JSON count LENGTH "${licenses}" overrides)
set(index 0)
while(index LESS count)
    string(JSON override GET "${licenses}" overrides ${index})
    string(JSON port ERROR_VARIABLE error GET "${override}" port)
    if(error)
        message(FATAL_ERROR "Override ${index} in ${licenses_name} names no port.")
    endif()
    foreach(field IN ITEMS version license reason)
        string(JSON ${field} ERROR_VARIABLE error GET "${override}" ${field})
        if(error OR ${field} STREQUAL "")
            message(FATAL_ERROR "The override for ${port} in ${licenses_name} lacks its ${field}.")
        endif()
    endforeach()
    set(override_version_${port} "${version}")
    set(override_license_${port} "${license}")
    math(EXPR index "${index} + 1")
endwhile()

# Lists the installed ports as <name>/<triplet>, and the ports each depends on as
# depends_<name>/<triplet>, its features' dependencies included. Only the fields read here matter,
# so any `;`, `[` or `]` in the others, which would split a CMake list, is blanked first. vcpkg
# writes no status file until it installs a port.
if(NOT IS_DIRECTORY "${INSTALLED_DIR}/vcpkg")
    message(FATAL_ERROR "No vcpkg installed tree is at ${INSTALLED_DIR}.")
endif()
set(installed "")
if(EXISTS "${INSTALLED_DIR}/vcpkg/status")
    file(READ "${INSTALLED_DIR}/vcpkg/status" status)
    string(REPLACE "\r\n" "\n" status "${status}")
    string(REPLACE ";" " " status "${status}")
    string(REPLACE "[" " " status "${status}")
    string(REPLACE "]" " " status "${status}")
    string(REGEX REPLACE "\n\n+" ";" paragraphs "${status}")
    foreach(paragraph IN LISTS paragraphs)
        set(paragraph "\n${paragraph}\n")
        if(NOT paragraph MATCHES "\nStatus: install ok installed\n"
            OR NOT paragraph MATCHES "\nPackage: ([^\n]+)\n"
        )
            continue()
        endif()
        set(name "${CMAKE_MATCH_1}")
        string(REGEX MATCH "\nArchitecture: ([^\n]+)\n" _ "${paragraph}")
        set(triplet "${CMAKE_MATCH_1}")
        if(NOT paragraph MATCHES "\nFeature: ")
            list(APPEND installed "${name}/${triplet}")
        endif()
        # A dependency for another triplet is named <port>:<triplet>.
        if(paragraph MATCHES "\nDepends: ([^\n]+)\n")
            string(REPLACE ", " ";" dependencies "${CMAKE_MATCH_1}")
            foreach(dependency IN LISTS dependencies)
                if(dependency MATCHES "^([^:]+):(.+)$")
                    list(APPEND depends_${name}/${triplet} "${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
                else()
                    list(APPEND depends_${name}/${triplet} "${dependency}/${triplet}")
                endif()
            endforeach()
        endif()
    endforeach()
endif()

set(problems "")
foreach(package IN LISTS installed)
    string(REGEX REPLACE "/.*" "" name "${package}")
    string(REGEX REPLACE ".*/" "" triplet "${package}")
    file(READ "${INSTALLED_DIR}/${triplet}/share/${name}/vcpkg.spdx.json" spdx)
    # vcpkg lists the port first, before the binary package and the sources.
    string(JSON port GET "${spdx}" packages 0)
    string(JSON version GET "${port}" versionInfo)
    string(JSON license GET "${port}" licenseConcluded)
    set(by_override "")
    if(DEFINED override_license_${name})
        if(NOT version STREQUAL override_version_${name})
            string(CONCAT problem
                "${name} ${version} isn't the version its override was reviewed at, "
                "${override_version_${name}}. Review its licence again, and update the override."
            )
            list(APPEND problems "${problem}")
            continue()
        endif()
        set(license "${override_license_${name}}")
        set(by_override " by its override")
    endif()
    # vcpkg writes NOASSERTION for a port that leaves out its licence, and LicenseRef-vcpkg-null for
    # one that sets it to null.
    if(license MATCHES "^(NOASSERTION|LicenseRef-vcpkg-null)$")
        string(CONCAT problem
            "${name} ${version} declares no licence. "
            "Review it, and add an override for that version."
        )
        list(APPEND problems "${problem}")
        continue()
    endif()
    wulfhilde_license_allowed(covered "${license}")
    if(NOT covered)
        string(CONCAT problem "${name} ${version} is under ${license}${by_override}, "
            "which the allowlist doesn't cover."
        )
        list(APPEND problems "${problem}")
    endif()
    set(port_${package} "${port}")
    set(version_${package} "${version}")
    set(license_${package} "${license}")
endforeach()

if(problems)
    wulfhilde_fail("The dependencies' licences fail the check:" "" ${problems})
endif()

if(NOT DEFINED CHECK_NOTICES AND NOT DEFINED WRITE_NOTICES)
    return()
endif()
foreach(variable IN ITEMS MANIFEST TRIPLET HOST_TRIPLET)
    if(NOT ${variable})
        message(FATAL_ERROR "The notices need MANIFEST, TRIPLET and HOST_TRIPLET.")
    endif()
endforeach()
if(HOST_TRIPLET STREQUAL TRIPLET)
    wulfhilde_fail("The notices can't tell build tools from the ports that ship:" ""
        "HOST_TRIPLET and TRIPLET are both ${TRIPLET}. Give the build tools a triplet of their own."
    )
endif()

# The manifest's default dependencies are those it names outside its features, and those of the
# features it turns on by default.
file(READ "${MANIFEST}" manifest)
set(roots "")
string(JSON dependencies ERROR_VARIABLE error GET "${manifest}" dependencies)
if(NOT error)
    wulfhilde_manifest_names(roots "${dependencies}")
endif()
set(features "")
string(JSON default_features ERROR_VARIABLE error GET "${manifest}" default-features)
if(NOT error)
    wulfhilde_manifest_names(features "${default_features}")
endif()
foreach(feature IN LISTS features)
    string(JSON dependencies ERROR_VARIABLE error
        GET "${manifest}" features ${feature} dependencies
    )
    if(NOT error)
        wulfhilde_manifest_names(roots "${dependencies}")
    endif()
endforeach()

# A default dependency for other platforms only isn't installed here, but ships there, and the one
# notices file serves every platform.
set(queue "")
set(problems "")
foreach(root IN LISTS roots)
    if("${root}/${TRIPLET}" IN_LIST installed)
        list(APPEND queue "${root}/${TRIPLET}")
    else()
        list(APPEND problems
            "${root} isn't installed for ${TRIPLET}, so the notices would leave it out."
        )
    endif()
endforeach()
if(problems)
    wulfhilde_fail("The notices can't list every port that ships:" "" ${problems})
endif()
set(shipped "")
while(queue)
    list(POP_FRONT queue package)
    if(package IN_LIST shipped)
        continue()
    endif()
    list(APPEND shipped "${package}")
    foreach(dependency IN LISTS depends_${package})
        if(dependency MATCHES "/${TRIPLET}$")
            list(APPEND queue "${dependency}")
        endif()
    endforeach()
endwhile()
list(SORT shipped)

set(notices [[
# Third-party notices

A game that ships the wulfhilde libraries ships the third-party libraries they are built with too.
Each is listed here with its version, its licence as an SPDX expression, and the notice that its
licence asks to be passed on.

This file is generated from the dependencies that `vcpkg.json` asks for by default, and what they
depend on in turn. Build tools and the tests' dependencies don't ship, so they aren't listed. To
bring it up to date, build the `notices` target rather than editing it.
]])

# Each port's copyright file loses its trailing spaces and the blank lines around it, which a diff
# would flag, and its fence is longer than any run of backticks in it, which would close it early.
# The entry is kept as entry_<name> for the check below.
set(shipped_names "")
foreach(package IN LISTS shipped)
    string(REGEX REPLACE "/.*" "" name "${package}")
    list(APPEND shipped_names "${name}")
    set(entry "\n## ${name}\n\n")
    string(APPEND entry "- Version: ${version_${package}}\n- Licence: ${license_${package}}\n")
    string(JSON homepage ERROR_VARIABLE error GET "${port_${package}}" homepage)
    if(NOT error)
        string(APPEND entry "- Homepage: ${homepage}\n")
    endif()

    file(READ "${INSTALLED_DIR}/${TRIPLET}/share/${name}/copyright" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REGEX REPLACE "[ \t]+\n" "\n" text "${text}")
    string(REGEX REPLACE "^\n+" "" text "${text}")
    string(REGEX REPLACE "[ \t\n]+$" "" text "${text}")
    set(fence "```")
    string(REGEX MATCHALL "`+" runs "${text}")
    foreach(run IN LISTS runs)
        string(LENGTH "${run}" length)
        string(LENGTH "${fence}" fence_length)
        if(length GREATER_EQUAL fence_length)
            math(EXPR length "${length} + 1")
            string(REPEAT "`" ${length} fence)
        endif()
    endforeach()
    string(APPEND entry "\n${fence}text\n${text}\n${fence}\n")

    set(entry_${name} "${entry}")
    string(APPEND notices "${entry}")
endforeach()
if(NOT shipped)
    string(APPEND notices "\nThe engine has no third-party dependencies yet.\n")
endif()

if(DEFINED WRITE_NOTICES)
    file(WRITE "${WRITE_NOTICES}" "${notices}")
    return()
endif()

set(existing "")
if(EXISTS "${CHECK_NOTICES}")
    file(READ "${CHECK_NOTICES}" existing)
    string(REPLACE "\r\n" "\n" existing "${existing}")
endif()
if(existing STREQUAL notices)
    return()
endif()
cmake_path(GET CHECK_NOTICES FILENAME notices_name)
set(problems "")
foreach(package IN LISTS shipped)
    string(REGEX REPLACE "/.*" "" name "${package}")
    string(FIND "${existing}" "${entry_${name}}" found)
    if(NOT found EQUAL -1)
        continue()
    endif()
    string(FIND "${existing}" "\n## ${name}\n\n- Version: " found)
    if(found EQUAL -1)
        list(APPEND problems "${notices_name} lacks ${name} ${version_${package}}, which ships.")
    else()
        list(APPEND problems "${notices_name} is out of date for ${name} ${version_${package}}.")
    endif()
endforeach()
string(REGEX MATCHALL "\n## [^\n]+\n\n- Version: [^\n]+" headings "${existing}")
foreach(heading IN LISTS headings)
    string(REGEX MATCH "^\n## ([^\n]+)\n\n- Version: (.+)$" _ "${heading}")
    if(NOT CMAKE_MATCH_1 IN_LIST shipped_names)
        list(APPEND problems
            "${notices_name} lists ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}, which no longer ships."
        )
    endif()
endforeach()
if(NOT problems)
    list(APPEND problems "${notices_name} differs outside the dependencies' entries.")
endif()
wulfhilde_fail("${notices_name} is out of date:"
    "Build the notices target to bring it up to date." ${problems}
)
