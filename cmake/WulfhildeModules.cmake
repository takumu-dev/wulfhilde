# How the engine's modules are declared, so that each one is shaped alike: a static library with
# its own public include tree, linked into the umbrella target and installed as a component of the
# package.

include_guard(GLOBAL)

# Adds the module `name` as the static library wulfhilde::<name>.
#
#   HEADERS  its public headers, under include/wulfhilde/<name>/
#   SOURCES  its sources and private headers
#   DEPENDS  the other modules it links, by name
#
# A module includes only the headers of the modules it links, since each public include tree comes
# only with its library.
function(wulfhilde_add_module name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "HEADERS;SOURCES;DEPENDS")
    set(target wulfhilde_${name})

    add_library(${target} STATIC)
    add_library(wulfhilde::${name} ALIAS ${target})
    target_sources(${target}
        PRIVATE ${arg_SOURCES}
        PUBLIC FILE_SET HEADERS BASE_DIRS include FILES ${arg_HEADERS}
    )
    target_compile_features(${target} PUBLIC cxx_std_23)
    foreach(dependency IN LISTS arg_DEPENDS)
        target_link_libraries(${target} PUBLIC wulfhilde::${dependency})
    endforeach()
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${name})
    wulfhilde_target_conventions(${target})

    target_link_libraries(wulfhilde INTERFACE ${target})
    set_property(GLOBAL APPEND PROPERTY WULFHILDE_MODULES ${name})
    set_property(GLOBAL PROPERTY WULFHILDE_${name}_DEPENDS ${arg_DEPENDS})
endfunction()
