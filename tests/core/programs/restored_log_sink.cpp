// Replaces the log sink and then restores the default, which the test reads from stderr.
#include <wulfhilde/core/log_sink.hpp>

#include <string_view>

namespace {

class SilentSink final : public wulfhilde::LogSink {
  public:
    void write(wulfhilde::LogSeverity /*severity*/, std::string_view /*message*/) override {}
};

} // namespace

int main() {
    SilentSink sink;
    wulfhilde::set_log_sink(&sink);
    wulfhilde::log(wulfhilde::LogSeverity::info, "to the replaced sink");
    wulfhilde::set_log_sink(nullptr);
    wulfhilde::log(wulfhilde::LogSeverity::info, "to the default sink");
}
