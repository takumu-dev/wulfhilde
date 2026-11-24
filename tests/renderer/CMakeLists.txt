wulfhilde_add_tests(renderer
    SOURCES
        device_error_tests.cpp
        image_comparison_tests.cpp
        image_tests.cpp
        renderer_settings_tests.cpp
    LIBRARIES
        wulfhilde::renderer
        wulfhilde_image_comparison
)

# The settings are checked before any device is sought, so these need no GPU.
wulfhilde_add_precondition_test(renderer "an anisotropy above 16 fires its precondition"
    SOURCE programs/anisotropy_above_16.cpp
    LIBRARIES wulfhilde::renderer
)
wulfhilde_add_precondition_test(renderer "an anisotropy of 0 fires its precondition"
    SOURCE programs/anisotropy_of_0.cpp
    LIBRARIES wulfhilde::renderer
)
wulfhilde_add_precondition_test(renderer "a negative exposure adaptation speed fires its precondition"
    SOURCE programs/negative_adaptation_speed.cpp
    LIBRARIES wulfhilde::renderer
)
wulfhilde_add_precondition_test(renderer "a pinned exposure that isn't a finite number fires its precondition"
    SOURCE programs/infinite_pinned_exposure.cpp
    LIBRARIES wulfhilde::renderer
)

# Each runs under validation, which fails it on any message, in every configuration.
wulfhilde_add_tests(renderer
    LABEL gpu
    SOURCES
        render_image_tests.cpp
        renderer_tests.cpp
        shader_function_tests.cpp
        test_pattern_tests.cpp
        textured_surface_tests.cpp
        window_tests.cpp
    LIBRARIES
        wulfhilde_renderer_testing
        wulfhilde::core
)
wulfhilde_add_shaders(wulfhilde_renderer_gpu_tests
    SOURCES
        shaders/agx_tests.slang
        shaders/exposure_tests.slang
        shaders/luminance_tests.slang
        shaders/matrix_layout_tests.slang
        shaders/srgb_tests.slang
        shaders/test_pattern.slang
    MODULE_DIRECTORIES
        "${WULFHILDE_SHADER_MODULES}"
)

wulfhilde_add_tests(renderer
    LABEL image
    SOURCES
        test_pattern_image_tests.cpp
    LIBRARIES
        wulfhilde_renderer_testing
        wulfhilde_image_comparison
)
wulfhilde_add_shaders(wulfhilde_renderer_image_tests
    SOURCES
        shaders/test_pattern.slang
    MODULE_DIRECTORIES
        "${WULFHILDE_SHADER_MODULES}"
)
target_compile_definitions(wulfhilde_renderer_image_tests PRIVATE
    "WULFHILDE_RENDERER_REFERENCES=\"${CMAKE_CURRENT_SOURCE_DIR}/references\""
    "WULFHILDE_IMAGE_FAILURES=\"${PROJECT_BINARY_DIR}/image-failures/renderer\""
)
target_compile_definitions(wulfhilde_renderer_tests PRIVATE
    "WULFHILDE_RENDERER_SCRATCH=\"${CMAKE_CURRENT_BINARY_DIR}/scratch\""
)
