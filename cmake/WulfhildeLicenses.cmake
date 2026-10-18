# The licences of the engine's own dependencies, for a build that takes them from the vcpkg
# manifest. Configuring checks every port vcpkg installed, build tools included, and stops on any
# whose licence the allowlist in cmake/licenses.json doesn't cover.
#
#   notices  writes THIRD_PARTY_NOTICES.md for the ports that ship, from this build's tree
#
# WULFHILDE_NOTICES_OPTIONS holds the options that check_licenses.cmake takes to work out the
# notices, for the test that the committed file is up to date.

include_guard(GLOBAL)

set(WULFHILDE_LICENSE_CHECK "${CMAKE_CURRENT_LIST_DIR}/check_licenses.cmake")
set(WULFHILDE_NOTICES_OPTIONS
    "-DINSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
    "-DMANIFEST=${PROJECT_SOURCE_DIR}/vcpkg.json"
    "-DTRIPLET=${VCPKG_TARGET_TRIPLET}"
    "-DHOST_TRIPLET=${VCPKG_HOST_TRIPLET}"
)

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DINSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
        -P "${WULFHILDE_LICENSE_CHECK}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The licence check above failed.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/licenses.json"
    "${WULFHILDE_LICENSE_CHECK}"
)

add_custom_target(notices
    COMMAND "${CMAKE_COMMAND}" ${WULFHILDE_NOTICES_OPTIONS}
        "-DWRITE_NOTICES=${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        -P "${WULFHILDE_LICENSE_CHECK}"
    COMMENT "Writing THIRD_PARTY_NOTICES.md"
    VERBATIM
)
