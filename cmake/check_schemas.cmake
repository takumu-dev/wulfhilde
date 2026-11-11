# Checks the schemas of maps and materials against the files of those kinds that the engine's tests
# hold, with check-jsonschema, so that the schema an editor checks a file against and the engine's
# loader agree.
#
#   cmake -DCHECK_JSONSCHEMA=<program> -DSOURCE_DIR=<the engine's source tree> -P check_schemas.cmake
#
# Each schema must itself be a valid schema. It must accept every file of its kind in the test
# content and every valid one of the assets tests. It must reject every invalid one of the assets
# tests, except those in a loader_only/ directory, which hold the problems a schema can't state,
# such as a key written twice, a loop wound the wrong way or an image that isn't there, and which it
# must accept, since only those problems are wrong with them.
cmake_minimum_required(VERSION 4.4)

foreach(variable IN ITEMS CHECK_JSONSCHEMA SOURCE_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "check_schemas.cmake needs ${variable}.")
    endif()
endforeach()

set(failures "")

# Runs check-jsonschema, setting `passed` to whether it passed and `output` to what it printed.
function(check_jsonschema)
    execute_process(
        COMMAND "${CHECK_JSONSCHEMA}" ${ARGN}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    if(result EQUAL 0)
        set(passed TRUE PARENT_SCOPE)
    else()
        set(passed FALSE PARENT_SCOPE)
    endif()
    set(output "${output}" PARENT_SCOPE)
endfunction()

# Checks the schema of one kind of asset, adding what is wrong to `failures`.
#
#   KIND       what the files are, for messages, such as map
#   SCHEMA     the schema, relative to the source tree
#   EXTENSION  the extension of the files, such as .map.json
#   VALID      the directories of files the schema must accept, searched through
#   INVALID    the directory of files it must reject, with a loader_only/ directory within it
function(check_schema)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "KIND;SCHEMA;EXTENSION;INVALID" "VALID")
    set(schema "${SOURCE_DIR}/${arg_SCHEMA}")
    set(invalid_dir "${SOURCE_DIR}/${arg_INVALID}")

    check_jsonschema(--check-metaschema "${schema}")
    if(NOT passed)
        message(FATAL_ERROR "${arg_SCHEMA} isn't a valid JSON Schema:\n${output}")
    endif()

    set(valid_files "")
    foreach(directory IN LISTS arg_VALID)
        file(GLOB_RECURSE files "${SOURCE_DIR}/${directory}/*${arg_EXTENSION}")
        list(APPEND valid_files ${files})
    endforeach()
    file(GLOB_RECURSE loader_only_files "${invalid_dir}/loader_only/*${arg_EXTENSION}")
    check_jsonschema(--schemafile "${schema}" ${valid_files} ${loader_only_files})
    if(NOT passed)
        list(APPEND failures "the ${arg_KIND} schema rejects ${arg_KIND}s that the loader accepts, or that hold only problems a schema can't state:\n${output}")
    endif()

    file(GLOB_RECURSE invalid_files "${invalid_dir}/*${arg_EXTENSION}")
    list(REMOVE_ITEM invalid_files ${loader_only_files})
    foreach(file IN LISTS invalid_files)
        check_jsonschema(--schemafile "${schema}" "${file}")
        cmake_path(RELATIVE_PATH file BASE_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE name)
        # Anything else that stops check-jsonschema, such as a crash, isn't a rejection.
        if(passed)
            list(APPEND failures "the ${arg_KIND} schema accepts ${name}, which the loader rejects")
        elseif(NOT output MATCHES "Schema validation errors were encountered|Failed to parse")
            list(APPEND failures "check-jsonschema failed on ${name} without rejecting it:\n${output}")
        endif()
    endforeach()

    list(LENGTH valid_files valid_count)
    list(LENGTH loader_only_files loader_only_count)
    list(LENGTH invalid_files invalid_count)
    if(valid_count EQUAL 0 OR loader_only_count EQUAL 0 OR invalid_count EQUAL 0)
        list(APPEND failures "some of the ${arg_KIND}s to check are missing")
    endif()
    set(failures "${failures}" PARENT_SCOPE)
    message(STATUS
        "The ${arg_KIND} schema accepts ${valid_count} valid ${arg_KIND}s and ${loader_only_count} that only the loader rejects, and rejects ${invalid_count} invalid ${arg_KIND}s."
    )
endfunction()

check_schema(
    KIND map
    SCHEMA schema/map.schema.json
    EXTENSION .map.json
    VALID tests/content tests/assets/valid
    INVALID tests/assets/invalid
)
check_schema(
    KIND material
    SCHEMA schema/material.schema.json
    EXTENSION .material.json
    VALID tests/content tests/assets/materials/valid
    INVALID tests/assets/materials/invalid
)

if(failures)
    list(JOIN failures "\n" message)
    message(FATAL_ERROR "${message}")
endif()
