# Writes a header of static_asserts that hold C++ structs to the layout of the shaders' structs they
# twin, from slangc's reflection of one file.
#
#   cmake -D REFLECTION=<json> -D OUTPUT=<header> -D SOURCE=<name> -P cmake/shader_layout.cmake
#
#   REFLECTION  what slangc's -reflection-json wrote
#   OUTPUT      the header to write
#   SOURCE      the Slang file's name, for the header's comment and the asserts' messages
#
# Every struct that the file's global parameters reach, through push constants, buffers or
# pointers, is laid out in scalar layout and needs a C++ twin of the same name, with fields of the
# same names. For each struct the header asserts its size, and each field's size and offset. It
# names the twins unqualified and includes nothing, so a source includes <cstddef> and then this
# where the twins are in scope, inside their namespace if they have one.
cmake_minimum_required(VERSION 4.4)

# Records every struct in a reflected value, and in the values it holds, in the global properties
# wulfhilde_structs (the names, in the order met) and wulfhilde_struct_<name> (its asserts).
function(wulfhilde_collect_structs value)
    string(JSON type TYPE "${value}")
    if(type STREQUAL "ARRAY")
        string(JSON count LENGTH "${value}")
        if(count GREATER 0)
            math(EXPR last "${count} - 1")
            foreach(index RANGE ${last})
                string(JSON element_type TYPE "${value}" ${index})
                if(element_type MATCHES "^(OBJECT|ARRAY)$")
                    string(JSON element GET "${value}" ${index})
                    wulfhilde_collect_structs("${element}")
                endif()
            endforeach()
        endif()
        return()
    endif()
    if(NOT type STREQUAL "OBJECT")
        return()
    endif()

    string(JSON kind ERROR_VARIABLE no_kind GET "${value}" kind)
    string(JSON name ERROR_VARIABLE no_name GET "${value}" name)
    string(JSON fields ERROR_VARIABLE no_fields GET "${value}" fields)
    if(NOT no_kind AND kind STREQUAL "struct" AND NOT no_name AND NOT no_fields)
        get_property(known GLOBAL PROPERTY wulfhilde_structs)
        if(NOT name IN_LIST known)
            set_property(GLOBAL APPEND PROPERTY wulfhilde_structs "${name}")
            wulfhilde_uniform_size(size "${value}")
            set(asserts
                "static_assert(sizeof(${name}) == ${size}, \"${name} is ${size} bytes in ${SOURCE}\");\n"
            )
            string(JSON field_count LENGTH "${fields}")
            math(EXPR last "${field_count} - 1")
            foreach(index RANGE ${last})
                string(JSON field_name GET "${fields}" ${index} name)
                string(JSON offset GET "${fields}" ${index} binding offset)
                string(JSON field_size GET "${fields}" ${index} binding size)
                string(APPEND asserts
                    "static_assert(sizeof(${name}::${field_name}) == ${field_size}, "
                    "\"${name}::${field_name} is ${field_size} bytes in ${SOURCE}\");\n"
                    "static_assert(offsetof(${name}, ${field_name}) == ${offset}, "
                    "\"${name}::${field_name} is at byte ${offset} in ${SOURCE}\");\n"
                )
            endforeach()
            set_property(GLOBAL PROPERTY wulfhilde_struct_${name} "${asserts}")
        endif()
    endif()

    string(JSON count LENGTH "${value}")
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE ${last})
            string(JSON member MEMBER "${value}" ${index})
            string(JSON child_type TYPE "${value}" "${member}")
            if(child_type MATCHES "^(OBJECT|ARRAY)$")
                string(JSON child GET "${value}" "${member}")
                wulfhilde_collect_structs("${child}")
            endif()
        endforeach()
    endif()
endfunction()

# Sets `out` to the size in bytes of a reflected type, from its uniform entry in `sizes`.
function(wulfhilde_uniform_size out type)
    string(JSON count LENGTH "${type}" sizes)
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
        string(JSON kind GET "${type}" sizes ${index} kind)
        if(kind STREQUAL "uniform")
            string(JSON size GET "${type}" sizes ${index} value)
            set(${out} ${size} PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "${SOURCE}: a struct has no size in its reflection")
endfunction()

file(READ "${REFLECTION}" reflection)
string(JSON parameters ERROR_VARIABLE no_parameters GET "${reflection}" parameters)
set_property(GLOBAL PROPERTY wulfhilde_structs "")
if(NOT no_parameters)
    wulfhilde_collect_structs("${parameters}")
endif()

string(CONCAT header
    "// Written by cmake/shader_layout.cmake from the reflection of ${SOURCE}, never by hand.\n"
    "// Each struct the shaders share with C++ is laid out alike on both sides.\n"
    "#pragma once\n\n"
)
get_property(structs GLOBAL PROPERTY wulfhilde_structs)
foreach(name IN LISTS structs)
    get_property(asserts GLOBAL PROPERTY wulfhilde_struct_${name})
    string(APPEND header "${asserts}")
endforeach()
file(WRITE "${OUTPUT}" "${header}")
