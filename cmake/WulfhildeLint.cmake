# The checks that build nothing: formatting, documentation and the asset schemas. Each target exists
# only where its tool is found, so the lint preset fails at once, naming the target, on a machine
# that lacks one.
#
#   check-format  fails if any source differs from what clang-format would make of it
#   format        reformats every source in place
#   docs          runs Doxygen over the public headers, failing on any warning
#   check-schema  checks the map and material schemas against the tests' files, as
#                 cmake/check_schemas.cmake describes

include_guard(GLOBAL)

find_program(WULFHILDE_CLANG_FORMAT clang-format)
if(WULFHILDE_CLANG_FORMAT)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/apps/*.cpp"
        "${PROJECT_SOURCE_DIR}/apps/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )
    add_custom_target(check-format
        COMMAND "${WULFHILDE_CLANG_FORMAT}" --dry-run --Werror ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking the formatting"
        VERBATIM
    )
    add_custom_target(format
        COMMAND "${WULFHILDE_CLANG_FORMAT}" -i ${sources}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Formatting the sources"
        VERBATIM
    )
endif()

find_package(Doxygen QUIET)
if(TARGET Doxygen::doxygen)
    add_custom_target(docs
        COMMAND Doxygen::doxygen Doxyfile
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Checking the documentation of the public headers"
        VERBATIM
    )
endif()

find_program(WULFHILDE_CHECK_JSONSCHEMA check-jsonschema)
if(WULFHILDE_CHECK_JSONSCHEMA)
    add_custom_target(check-schema
        COMMAND "${CMAKE_COMMAND}"
            "-DCHECK_JSONSCHEMA=${WULFHILDE_CHECK_JSONSCHEMA}"
            "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/check_schemas.cmake"
        COMMENT "Checking the map and material schemas against the tests' files"
        VERBATIM
    )
endif()
