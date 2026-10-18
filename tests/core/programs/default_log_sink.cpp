// Logs a message of each severity to the default sink, which the test reads from stderr.
#include <wulfhilde/core/log_sink.hpp>

int main() {
    using enum wulfhilde::LogSeverity;
    wulfhilde::log(trace, "first");
    wulfhilde::log(debug, "second");
    wulfhilde::log(info, "third");
    wulfhilde::log(warning, "fourth");
    wulfhilde::log(error, "fifth");
}
