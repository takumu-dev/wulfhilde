// What every `gpu` test shares: it runs under Vulkan's validation layers, in every configuration,
// and any message they send fails it.
#pragma once

#include <wulfhilde/core/log_sink.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>

#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The settings a `gpu` test makes its renderer or device with.
inline constexpr wulfhilde::RendererSettings gpu_test_settings{.validation = true};

// Requires a result from the renderer to hold a value, failing with its error when it doesn't.
template <typename T> void require_value(const std::expected<T, wulfhilde::DeviceError>& result) {
    if (!result) {
        FAIL(result.error().message);
    }
}

// Keeps every warning and error logged to it, from any thread. The validation layers send their
// messages at one or the other.
class RecordingSink final : public wulfhilde::LogSink {
  public:
    void write(wulfhilde::LogSeverity severity, std::string_view message) override {
        if (severity >= wulfhilde::LogSeverity::warning) {
            const std::scoped_lock lock(mutex_);
            messages_.emplace_back(severity, message);
        }
    }

    std::vector<std::pair<wulfhilde::LogSeverity, std::string>> messages() const {
        const std::scoped_lock lock(mutex_);
        return messages_;
    }

  private:
    mutable std::mutex mutex_;
    std::vector<std::pair<wulfhilde::LogSeverity, std::string>> messages_;
};

// The fixture of every `gpu` test, through TEST_CASE_METHOD. It records what is logged while the
// test runs, and fails the test once for each warning or error, which is how a validation message
// fails it.
class GpuTest {
  public:
    GpuTest() noexcept { wulfhilde::set_log_sink(&sink_); }

    GpuTest(const GpuTest&) = delete;
    GpuTest(GpuTest&&) = delete;
    GpuTest& operator=(const GpuTest&) = delete;
    GpuTest& operator=(GpuTest&&) = delete;

    ~GpuTest() {
        wulfhilde::set_log_sink(nullptr);
        for (const auto& [severity, message] : sink_.messages()) {
            FAIL_CHECK(message);
        }
    }

  protected:
    // Sends what is logged to another sink until the test ends, as a test of the messages
    // themselves does. Nothing logged from then on fails the test.
    static void log_to(wulfhilde::LogSink& sink) noexcept { wulfhilde::set_log_sink(&sink); }

  private:
    RecordingSink sink_;
};
