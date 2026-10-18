#include <wulfhilde/core/log_sink.hpp>

#include <catch2/catch_test_macros.hpp>

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Message {
    wulfhilde::LogSeverity severity;
    std::string text;

    bool operator==(const Message&) const = default;
};

// Keeps every message it receives, from any thread.
class RecordingSink final : public wulfhilde::LogSink {
  public:
    void write(wulfhilde::LogSeverity severity, std::string_view message) override {
        const std::scoped_lock lock(mutex_);
        messages_.push_back({.severity = severity, .text = std::string(message)});
    }

    std::vector<Message> messages() const {
        const std::scoped_lock lock(mutex_);
        return messages_;
    }

  private:
    mutable std::mutex mutex_;
    std::vector<Message> messages_;
};

// Routes the engine's diagnostics to a sink for the length of a test.
class ScopedLogSink {
  public:
    explicit ScopedLogSink(wulfhilde::LogSink& sink) noexcept { wulfhilde::set_log_sink(&sink); }

    ScopedLogSink(const ScopedLogSink&) = delete;
    ScopedLogSink(ScopedLogSink&&) = delete;
    ScopedLogSink& operator=(const ScopedLogSink&) = delete;
    ScopedLogSink& operator=(ScopedLogSink&&) = delete;

    ~ScopedLogSink() { wulfhilde::set_log_sink(nullptr); }
};

} // namespace

TEST_CASE("a replaced log sink receives each message with its severity", "[core]") {
    using enum wulfhilde::LogSeverity;
    RecordingSink sink;
    const ScopedLogSink scope(sink);

    wulfhilde::log(trace, "first");
    wulfhilde::log(debug, "second");
    wulfhilde::log(info, "third");
    wulfhilde::log(warning, "fourth");
    wulfhilde::log(error, "fifth");

    CHECK(sink.messages() == std::vector<Message>{
                                 {.severity = trace, .text = "first"},
                                 {.severity = debug, .text = "second"},
                                 {.severity = info, .text = "third"},
                                 {.severity = warning, .text = "fourth"},
                                 {.severity = error, .text = "fifth"},
                             });
}
