#include "tick_clock.hpp"

#include "controller.hpp"
#include <wulfhilde/core/narrow.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace viewer {
namespace {

/// How many ticks fall due a second, as many as the controller's ticks fill.
constexpr std::int64_t ticks_per_second = 240;
static_assert(Controller::tick_seconds == 1.0F / static_cast<float>(ticks_per_second));

/// How long a tick lasts, in 240ths of a nanosecond.
constexpr std::int64_t tick_length = std::chrono::nanoseconds(std::chrono::seconds(1)).count();

/// The longest time a frame counts.
constexpr std::chrono::nanoseconds longest_frame = std::chrono::milliseconds(250);

/// The most time owed, in 240ths of a nanosecond: the longest frame's.
constexpr std::int64_t most_owed = longest_frame.count() * ticks_per_second;

} // namespace

std::size_t TickClock::advance(std::chrono::nanoseconds elapsed) noexcept {
    // Clamping the frame before counting it keeps a frame of years from overflowing the count.
    const auto counted =
        std::clamp(elapsed, std::chrono::nanoseconds{0}, longest_frame).count() * ticks_per_second;
    owed_ = std::min(owed_ + counted, most_owed);
    const auto due = owed_ / tick_length;
    owed_ %= tick_length;
    return wulfhilde::narrow<std::size_t>(due);
}

} // namespace viewer
