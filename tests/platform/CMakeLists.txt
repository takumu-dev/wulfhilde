wulfhilde_add_tests(platform
    SOURCES
        platform_tests.cpp
    LIBRARIES
        wulfhilde::platform
)

# A window can't open without a Vulkan driver that draws into it.
wulfhilde_add_tests(platform
    LABEL gpu
    SOURCES
        window_tests.cpp
    LIBRARIES
        wulfhilde::platform
)

wulfhilde_add_precondition_test(platform "a second platform while one lives fires its precondition"
    SOURCE programs/second_platform.cpp
    LIBRARIES wulfhilde::platform
)
set_tests_properties("platform: a second platform while one lives fires its precondition"
    PROPERTIES ENVIRONMENT "${WULFHILDE_TEST_ENVIRONMENT}"
)
