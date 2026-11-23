wulfhilde_add_tests(viewer
    SOURCES
        cameras_tests.cpp
        command_line_tests.cpp
        test_maps_tests.cpp
    LIBRARIES
        wulfhilde_viewer_lib
)
target_compile_definitions(wulfhilde_viewer_tests PRIVATE
    "WULFHILDE_TEST_CONTENT=\"${PROJECT_SOURCE_DIR}/tests/content\""
)

wulfhilde_add_tests(viewer
    LABEL image
    SOURCES
        test_map_image_tests.cpp
    LIBRARIES
        wulfhilde::wulfhilde
        wulfhilde_image_comparison
)
target_compile_definitions(wulfhilde_viewer_image_tests PRIVATE
    "WULFHILDE_TEST_CONTENT=\"${PROJECT_SOURCE_DIR}/tests/content\""
    "WULFHILDE_VIEWER_REFERENCES=\"${CMAKE_CURRENT_SOURCE_DIR}/references\""
    "WULFHILDE_IMAGE_FAILURES=\"${PROJECT_BINARY_DIR}/image-failures/viewer\""
)

wulfhilde_add_program_test(viewer "with --help, the viewer prints the usage and succeeds"
    PROGRAM wulfhilde-viewer
    ARGUMENTS --help
    STDOUT_MATCHES "^usage: wulfhilde-viewer "
)
wulfhilde_add_program_test(viewer "with nothing to do, the viewer prints the usage and fails"
    PROGRAM wulfhilde-viewer
    EXPECTED_RESULT 1
    STDERR_MATCHES "^usage: wulfhilde-viewer "
)
wulfhilde_add_program_test(viewer "a usage error is reported before the usage"
    PROGRAM wulfhilde-viewer
    ARGUMENTS --fly
    EXPECTED_RESULT 1
    STDERR_MATCHES "^wulfhilde-viewer: unknown argument: --fly.+usage: wulfhilde-viewer "
)
wulfhilde_add_program_test(viewer "video that can't start is reported, and the viewer fails"
    PROGRAM wulfhilde-viewer
    ARGUMENTS "${PROJECT_SOURCE_DIR}/tests/content/sectors.map.json"
    EXPECTED_RESULT 1
    STDERR_MATCHES "^wulfhilde-viewer: video couldn't start: "
)
set_tests_properties("viewer: video that can't start is reported, and the viewer fails"
    PROPERTIES ENVIRONMENT "SDL_VIDEO_DRIVER=nowhere"
)
wulfhilde_add_program_test(viewer "a sample count the device lacks is reported, naming those it has, and the viewer fails"
    PROGRAM wulfhilde-viewer
    ARGUMENTS "${PROJECT_SOURCE_DIR}/tests/content/sectors.map.json" --samples 64
    EXPECTED_RESULT 1
    STDERR_MATCHES "wulfhilde-viewer: the device can't take 64 samples a pixel, only 1, "
)
set_tests_properties("viewer: a sample count the device lacks is reported, naming those it has, and the viewer fails"
    PROPERTIES
        LABELS gpu
        ENVIRONMENT "${WULFHILDE_TEST_ENVIRONMENT}"
        ENVIRONMENT_MODIFICATION "${WULFHILDE_GPU_TEST_ENVIRONMENT}"
)
wulfhilde_add_program_test(viewer "a camera that isn't in the map is a usage error"
    PROGRAM wulfhilde-viewer
    ARGUMENTS "${PROJECT_SOURCE_DIR}/tests/content/sectors.map.json" --camera yard,1,1,1,0,0
    EXPECTED_RESULT 1
    STDERR_MATCHES "^wulfhilde-viewer: --camera names no sector of the map: yard.+usage: wulfhilde-viewer "
)
wulfhilde_add_program_test(viewer "the viewer reports each problem of a map at its file, line and column, and fails"
    PROGRAM wulfhilde-viewer
    ARGUMENTS "${PROJECT_SOURCE_DIR}/tests/assets/invalid/several_problems.map.json"
    EXPECTED_RESULT 1
    STDERR_MATCHES "/tests/assets/invalid/several_problems\\.map\\.json:4:12: the start position has no \"yaw\", which it must have \\(/start\\).+/several_problems\\.map\\.json:12:22: expected an array, not a string \\(/sectors/0/loops/0/0/start\\)"
)
wulfhilde_add_program_test(viewer "the viewer reports each problem of a map's assets at its file, and fails"
    PROGRAM wulfhilde-viewer
    ARGUMENTS "${PROJECT_SOURCE_DIR}/tests/assets/valid/hall.map.json"
    EXPECTED_RESULT 1
    STDERR_MATCHES "/tests/assets/valid/hall\\.map\\.json: there is no file at \"test/floor\\.material\\.json\" \\(/sectors/0/floor/material\\).+/tests/assets/valid/hall\\.map\\.json: there is no file at \"test/wall\\.material\\.json\" \\(/sectors/0/loops/0/0/surface/material\\)"
)
wulfhilde_add_program_test(viewer "the viewer names the map's file as given, with its directory, when its content root is named too"
    PROGRAM wulfhilde-viewer
    ARGUMENTS
        "${PROJECT_SOURCE_DIR}/tests/assets/invalid/loader_only/unknown_sector.map.json"
        --content "${PROJECT_SOURCE_DIR}/tests/assets"
    EXPECTED_RESULT 1
    STDERR_MATCHES "/tests/assets/invalid/loader_only/unknown_sector\\.map\\.json:19:24: no sector has the id \"yard\" \\(/start/sector\\)"
)
wulfhilde_add_program_test(viewer "a content root that isn't there is an error, which the viewer reports"
    PROGRAM wulfhilde-viewer
    ARGUMENTS nowhere/sectors.map.json
    EXPECTED_RESULT 1
    STDERR_MATCHES "^wulfhilde-viewer: no directory is at \"nowhere\""
)
wulfhilde_add_program_test(viewer "a map outside its content root is a usage error"
    PROGRAM wulfhilde-viewer
    ARGUMENTS
        "${PROJECT_SOURCE_DIR}/tests/content/sectors.map.json"
        --content "${PROJECT_SOURCE_DIR}/tests/assets"
    EXPECTED_RESULT 1
    STDERR_MATCHES "^wulfhilde-viewer: [^ ]*/sectors\\.map\\.json isn't in the content root, .+usage: wulfhilde-viewer "
)
