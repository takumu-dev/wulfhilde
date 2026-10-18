wulfhilde_add_tests(viewer
    SOURCES
        command_line_tests.cpp
    LIBRARIES
        wulfhilde_viewer_lib
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
