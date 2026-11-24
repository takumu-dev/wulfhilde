#include "input.hpp"

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/platform/event.hpp>
#include <wulfhilde/platform/key.hpp>

#include <algorithm>
#include <span>
#include <variant>

namespace viewer {
namespace {

/// Returns 1 or -1 for whichever of two opposite keys is held alone, or 0.
float axis(bool positive, bool negative) noexcept {
    return (positive ? 1.0F : 0.0F) - (negative ? 1.0F : 0.0F);
}

} // namespace

FrameInput Input::follow(std::span<const wulfhilde::Event> events) {
    using wulfhilde::Key;
    FrameInput frame;
    const auto capture = [this, &frame](bool captured) {
        if (captured != captured_) {
            captured_ = captured;
            frame.capture_mouse = captured;
        }
    };
    for (const auto& event : events) {
        if (const auto* const pressed = std::get_if<wulfhilde::KeyPressed>(&event)) {
            if (!held(pressed->key)) {
                held_.push_back(pressed->key);
            }
            frame.return_to_start = frame.return_to_start || pressed->key == Key::r;
            frame.print_eye = frame.print_eye || pressed->key == Key::c;
            if (pressed->key == Key::escape) {
                capture(false);
            }
        } else if (const auto* const released = std::get_if<wulfhilde::KeyReleased>(&event)) {
            std::erase(held_, released->key);
        } else if (const auto* const moved = std::get_if<wulfhilde::MouseMoved>(&event)) {
            // The mouse's x runs right and its y toward the player, and the eye turns left and up.
            if (captured_) {
                frame.yaw -= sensitivity_ * moved->motion.x;
                frame.pitch -= sensitivity_ * moved->motion.y;
            }
        } else if (std::holds_alternative<wulfhilde::MouseButtonPressed>(event)) {
            capture(true);
        }
    }
    frame.actions = {
        .forward = axis(held(Key::w), held(Key::s)),
        .strafe = axis(held(Key::a), held(Key::d)),
        .run = held(Key::left_shift) || held(Key::right_shift),
        .jump = held(Key::space),
    };
    return frame;
}

bool Input::held(wulfhilde::Key key) const { return std::ranges::contains(held_, key); }

} // namespace viewer
