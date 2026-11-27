wulfhilde_add_tests(collision
    SOURCES
        move_tests.cpp
        portal_tests.cpp
        settle_tests.cpp
        sky_tests.cpp
    LIBRARIES
        wulfhilde::collision
)
# An empty optional, such as a body that stands on nothing, prints as such rather than as {?}.
target_compile_definitions(wulfhilde_collision_tests PRIVATE CATCH_CONFIG_ENABLE_OPTIONAL_STRINGMAKER)

wulfhilde_add_precondition_test(collision "a body shorter than it is wide fires its precondition"
    SOURCE programs/body_shorter_than_its_width.cpp
    LIBRARIES wulfhilde::collision
)
