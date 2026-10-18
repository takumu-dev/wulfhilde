// Fails a check under a replaced assertion handler. The handler ends the program successfully only
// if it receives the failure as written, after the failure was logged as an error.
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/log_sink.hpp>

#include <cstdint>
#include <cstdlib>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {

// What the handler checks. A handler is a plain function and can't capture, so it lives here.
struct Check {
    std::uint_least32_t failing_line = 0;
    std::vector<std::string> logged_errors;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the handler reads it.
Check check;

class ErrorSink final : public wulfhilde::LogSink {
  public:
    void write(wulfhilde::LogSeverity severity, std::string_view message) override {
        if (severity == wulfhilde::LogSeverity::error) {
            check.logged_errors.emplace_back(message);
        }
    }
};

[[noreturn]] void check_failure(const wulfhilde::AssertionFailure& failure) {
    const std::string_view file = std::source_location::current().file_name();
    const std::vector<std::string> expected_errors{
        std::format("{}:{}: assertion failed: a square has four sides (sides == 4)", file,
                    check.failing_line),
    };
    const bool as_written =
        failure.expression == "sides == 4" && failure.message == "a square has four sides" &&
        failure.location.file_name() == file && failure.location.line() == check.failing_line;
    if (!as_written || check.logged_errors != expected_errors) {
        std::println(stderr, "received: {} ({}) at {}:{}", failure.message, failure.expression,
                     failure.location.file_name(), failure.location.line());
        std::println(stderr, "logged: {}", check.logged_errors);
        std::_Exit(EXIT_FAILURE);
    }
    std::_Exit(EXIT_SUCCESS);
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): nothing is caught, so a failure ends the program.
int main() {
    ErrorSink sink;
    wulfhilde::set_log_sink(&sink);
    wulfhilde::set_assertion_handler(check_failure);

    const int sides = 3;
    check.failing_line = std::source_location::current().line() + 1;
    WULFHILDE_ASSERT(sides == 4, "a square has four sides");

    std::println(stderr, "the check didn't fail");
    return EXIT_FAILURE;
}
