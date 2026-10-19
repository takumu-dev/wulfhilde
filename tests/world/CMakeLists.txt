wulfhilde_add_tests(world
    SOURCES
        map_load_error_tests.cpp
        map_query_tests.cpp
        map_rules_tests.cpp
        map_tests.cpp
        trace_tests.cpp
    LIBRARIES
        wulfhilde::world
)
# An empty optional, such as a sector that isn't found, prints as such rather than as {?}.
target_compile_definitions(wulfhilde_world_tests PRIVATE CATCH_CONFIG_ENABLE_OPTIONAL_STRINGMAKER)

wulfhilde_add_precondition_test(world "a location in a sector the map doesn't have fires its precondition"
    SOURCE programs/location_beyond_the_map.cpp
    LIBRARIES wulfhilde::world
)
