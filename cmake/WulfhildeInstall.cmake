# Installs the engine as a CMake package, so that a game can call
# `find_package(wulfhilde COMPONENTS ...)` for a subset of the modules, or `find_package(wulfhilde)`
# for all of them and the umbrella target.
#
# Each module gets an export set of its own, so asking for one loads only its targets and those of
# the modules it links.

include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(WULFHILDE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/wulfhilde")

get_property(WULFHILDE_MODULES GLOBAL PROPERTY WULFHILDE_MODULES)
set(WULFHILDE_MODULE_DEPENDENCIES "")
foreach(module IN LISTS WULFHILDE_MODULES)
    install(TARGETS wulfhilde_${module}
        EXPORT wulfhilde-${module}-targets
        ARCHIVE
        FILE_SET HEADERS
    )
    install(EXPORT wulfhilde-${module}-targets
        NAMESPACE wulfhilde::
        DESTINATION "${WULFHILDE_INSTALL_CMAKEDIR}"
    )
    get_property(dependencies GLOBAL PROPERTY WULFHILDE_${module}_DEPENDS)
    string(APPEND WULFHILDE_MODULE_DEPENDENCIES
        "set(_wulfhilde_${module}_dependencies ${dependencies})\n"
    )
endforeach()

install(TARGETS wulfhilde EXPORT wulfhilde-targets)
install(EXPORT wulfhilde-targets
    NAMESPACE wulfhilde::
    DESTINATION "${WULFHILDE_INSTALL_CMAKEDIR}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/wulfhilde-config.cmake.in"
    "${PROJECT_BINARY_DIR}/wulfhilde-config.cmake"
    INSTALL_DESTINATION "${WULFHILDE_INSTALL_CMAKEDIR}"
)
# Before 1.0, a minor release may break the interface.
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/wulfhilde-config-version.cmake"
    COMPATIBILITY SameMinorVersion
)
install(FILES
    "${PROJECT_BINARY_DIR}/wulfhilde-config.cmake"
    "${PROJECT_BINARY_DIR}/wulfhilde-config-version.cmake"
    DESTINATION "${WULFHILDE_INSTALL_CMAKEDIR}"
)

install(FILES LICENSE README.md THIRD_PARTY_NOTICES.md DESTINATION "${CMAKE_INSTALL_DOCDIR}")
