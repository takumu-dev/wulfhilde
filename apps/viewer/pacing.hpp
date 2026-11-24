#pragma once

#include <wulfhilde/platform/event.hpp>

#include <cstdint>
#include <span>

namespace viewer {

/// What the viewer does next.
enum class Pace : std::uint8_t {
    draw, ///< Draw a frame, which the display's next refresh paces.
    rest, ///< Wait for something to happen, with nothing of the window to see.
    quit, ///< End, the window having been closed.
};

/// Follows the events of the viewer's window to tell what the viewer does next.
class Pacing {
  public:
    /// Takes in the events since the last ones, oldest first.
    ///
    /// \param events The events.
    /// \return What the viewer does next.
    [[nodiscard]] Pace follow(std::span<const wulfhilde::Event> events) noexcept;

  private:
    /// Whether the window is minimised, as of the events taken in last.
    bool minimized_ = false;
};

} // namespace viewer
