# Checks the map schema against the maps that the engine's tests hold, with check-jsonschema, so
# that the schema an editor checks a map against and the engine's loader agree.
#
#   cmake -DCHECK_JSONSCHEMA=<program> -DSOURCE_DIR=<the engine's source tree> -P check_schemas.cmake
#
# The schema must itself be a valid schema. It must accept every map in the test content and every
# valid map of the assets tests. It must reject every invalid map of the assets tests, except those
# in tests/assets/invalid/loader_only/, which hold the problems a schema can't state, such as a key
# written twice or a loop wound the wrong way, and which it must accept, since only those problems
# are wrong with them.
cmake_minimum_required(VERSION 4.4)

foreach(variable IN ITEMS CHECK_JSONSCHEMA SOURCE_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "check_schemas.cmake needs ${variable}.")
    endif()
endforeach()

set(schema "${SOURCE_DIR}/schema/map.schema.json")
set(invalid_dir "${SOURCE_DIR}/tests/assets/invalid")
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

check_jsonschema(--check-metaschema "${schema}")
if(NOT passed)
    message(FATAL_ERROR "schema/map.schema.json isn't a valid JSON Schema:\n${output}")
endif()

file(GLOB valid_maps
    "${SOURCE_DIR}/tests/content/*.map.json"
    "${SOURCE_DIR}/tests/assets/valid/*.map.json"
)
file(GLOB_RECURSE loader_only_maps "${invalid_dir}/loader_only/*.map.json")
check_jsonschema(--schemafile "${schema}" ${valid_maps} ${loader_only_maps})
if(NOT passed)
    list(APPEND failures "the schema rejects maps that the loader accepts, or that hold only problems a schema can't state:\n${output}")
endif()

file(GLOB_RECURSE invalid_maps "${invalid_dir}/*.map.json")
list(REMOVE_ITEM invalid_maps ${loader_only_maps})
foreach(map IN LISTS invalid_maps)
    check_jsonschema(--schemafile "${schema}" "${map}")
    cmake_path(RELATIVE_PATH map BASE_DIRECTORY "${SOURCE_DIR}" OUTPUT_VARIABLE name)
    # Anything else that stops check-jsonschema, such as a crash, isn't a rejection.
    if(passed)
        list(APPEND failures "the schema accepts ${name}, which the loader rejects")
    elseif(NOT output MATCHES "Schema validation errors were encountered|Failed to parse")
        list(APPEND failures "check-jsonschema failed on ${name} without rejecting it:\n${output}")
    endif()
endforeach()

list(LENGTH valid_maps valid_count)
list(LENGTH loader_only_maps loader_only_count)
list(LENGTH invalid_maps invalid_count)
if(valid_count EQUAL 0 OR loader_only_count EQUAL 0 OR invalid_count EQUAL 0)
    list(APPEND failures "some of the maps to check are missing")
endif()

if(failures)
    list(JOIN failures "\n" message)
    message(FATAL_ERROR "${message}")
endif()
message(STATUS
    "The map schema accepts ${valid_count} valid maps and ${loader_only_count} that only the loader rejects, and rejects ${invalid_count} invalid maps."
)
