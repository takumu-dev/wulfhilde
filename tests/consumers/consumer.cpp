// A game's smallest use of the engine, built by each consumer project: a checked narrowing and a
// message to the default log sink.
#include <wulfhilde/core/log_sink.hpp>
#include <wulfhilde/core/narrow.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

int main() {
    const auto sectors = wulfhilde::narrow<std::uint32_t>(std::size_t{12});
    wulfhilde::log(wulfhilde::LogSeverity::info, "a game linked the engine");
    return sectors == 12 ? EXIT_SUCCESS : EXIT_FAILURE;
}
