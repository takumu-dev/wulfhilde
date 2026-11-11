wulfhilde_add_tests(assets
    SOURCES
        content_root_tests.cpp
        load_map_assets_tests.cpp
        load_map_tests.cpp
    LIBRARIES
        wulfhilde::assets
        lodepng
)
target_compile_definitions(wulfhilde_assets_tests PRIVATE
    "WULFHILDE_ASSETS_TEST_FILES=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    "WULFHILDE_ASSETS_SCRATCH=\"${CMAKE_CURRENT_BINARY_DIR}/scratch\""
)
