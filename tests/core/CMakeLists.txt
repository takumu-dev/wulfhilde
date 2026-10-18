wulfhilde_add_tests(core
    SOURCES
        assertion_tests.cpp
        log_sink_tests.cpp
        narrow_tests.cpp
    LIBRARIES
        wulfhilde::core
)

wulfhilde_add_program_test(core "the default log sink writes each message to stderr with its severity"
    SOURCE programs/default_log_sink.cpp
    LIBRARIES wulfhilde::core
    EXPECTED_STDERR programs/default_log_sink.stderr
)
wulfhilde_add_program_test(core "a null log sink restores the default"
    SOURCE programs/restored_log_sink.cpp
    LIBRARIES wulfhilde::core
    EXPECTED_STDERR programs/restored_log_sink.stderr
)
wulfhilde_add_program_test(core "a failed assertion is logged as an error and then passed to the handler"
    SOURCE programs/replaced_assertion_handler.cpp
    LIBRARIES wulfhilde::core
)
wulfhilde_add_program_test(core "a failed assertion is reported on stderr and aborts by default"
    SOURCE programs/default_assertion_handler.cpp
    LIBRARIES wulfhilde::core
    EXPECTED_RESULT abort
    STDERR_MATCHES "^error: .*default_assertion_handler\\.cpp:[0-9]+: assertion failed: a square has four sides \\(sides == 4\\).$"
)
wulfhilde_add_program_test(core "the engine aborts when a replaced assertion handler returns"
    SOURCE programs/returning_assertion_handler.cpp
    LIBRARIES wulfhilde::core
    EXPECTED_RESULT abort
)
wulfhilde_add_program_test(core "a failed debug assertion fires only outside release builds"
    SOURCE programs/failed_debug_assertion.cpp
    LIBRARIES wulfhilde::core
)

wulfhilde_add_precondition_test(core "narrowing a value too large for its type fires its precondition"
    SOURCE programs/narrow_too_large.cpp
)
wulfhilde_add_precondition_test(core
    "narrowing a negative value to an unsigned type fires its precondition"
    SOURCE programs/narrow_negative_to_unsigned.cpp
)
