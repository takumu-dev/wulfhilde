#include "pacing.hpp"

#include <wulfhilde/platform/event.hpp>

#include <span>
#include <variant>

namespace viewer {

Pace Pacing::follow(std::span<const wulfhilde::Event> events) noexcept {
    for (const auto& event : events) {
        if (std::holds_alternative<wulfhilde::WindowClosed>(event)) {
            return Pace::quit;
        }
        if (std::holds_alternative<wulfhilde::WindowMinimized>(event)) {
            minimized_ = true;
        } else if (std::holds_alternative<wulfhilde::WindowRestored>(event)) {
            minimized_ = false;
        }
    }
    return minimized_ ? Pace::rest : Pace::draw;
}

} // namespace viewer
