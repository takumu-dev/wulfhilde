if (EXISTS ${CURRENT_INSTALLED_DIR}/share/lodepng-c/copyright)
    message(FATAL_ERROR "${PORT} conflict with lodepng-c, please remove lodepng-c before install ${PORT}.")
endif()

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO lvandeve/lodepng
    REF 7b2893e35891c0c85bee5595caafaf3c2044b409
    SHA512 57d49f0530d16bfe1ffd378333fc1514e09eb6a581be2a27715e2a7c405b7458a53ac15de10eb63c550794384f448fcd7b077605f65f2557e80b8acac883ede8
    HEAD_REF master
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")
configure_file("${SOURCE_PATH}/lodepng.cpp" "${SOURCE_PATH}/lodepng.c" COPYONLY)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS_DEBUG
        -DDISABLE_INSTALL_HEADERS=ON
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()

vcpkg_cmake_config_fixup()
vcpkg_cmake_config_fixup(PACKAGE_NAME lodepng-c)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
