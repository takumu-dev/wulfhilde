#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace viewer {

/// Counts how many of the controller's ticks fall due as frames pass.
///
/// A frame that took longer than a quarter of a second, such as one after the window was
/// minimised, counts only that quarter, so a stall doesn't leave a backlog of ticks that would make
/// every later frame slower still.
class TickClock {
  public:
    /// Takes in how long the last frame took.
    ///
    /// \param elapsed How long it took.
    /// \return How many ticks are due now.
    [[nodiscard]] std::size_t advance(std::chrono::nanoseconds elapsed) noexcept;

  private:
    /// The time owed toward the next tick, in 240ths of a nanosecond, so that a tick is a whole
    /// number of them.
    std::int64_t owed_ = 0;
};

} // namespace viewer
