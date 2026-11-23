/// \file
/// What happens to the window, and on the keyboard and the mouse, as the platform reports it.

#pragma once

#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/platform/key.hpp>
#include <wulfhilde/platform/window_size.hpp>

#include <cstdint>
#include <variant>

namespace wulfhilde {

/// A button of a mouse.
enum class MouseButton : std::uint8_t {
    left = 1,    ///< The left button, or the primary one.
    middle = 2,  ///< The middle button, or a wheel pressed.
    right = 3,   ///< The right button, or the secondary one.
    back = 4,    ///< The first side button, which goes back in a browser.
    forward = 5, ///< The second side button, which goes forward in a browser.
};

/// The person asked to close the window, as with its close button.
///
/// The window stays open until it is destroyed, so a program may ask first.
struct WindowClosed {
    /// Compares two events exactly.
    constexpr bool operator==(const WindowClosed&) const noexcept = default;
};

/// The window's drawable area changed size.
struct WindowResized {
    /// The new size.
    WindowSize size{};

    /// Compares two events exactly.
    constexpr bool operator==(const WindowResized&) const noexcept = default;
};

/// A key went down. A key held until it repeats sends this only once.
struct KeyPressed {
    /// The key.
    Key key;

    /// Compares two events exactly.
    constexpr bool operator==(const KeyPressed&) const noexcept = default;
};

/// A key came up.
struct KeyReleased {
    /// The key.
    Key key;

    /// Compares two events exactly.
    constexpr bool operator==(const KeyReleased&) const noexcept = default;
};

/// The mouse moved while the window has relative mouse mode on.
struct MouseMoved {
    /// How far it moved, in the mouse's own counts, unscaled by the system's pointer speed: x to
    /// the right and y toward the person.
    Vector2 motion{};

    /// Compares two events exactly.
    constexpr bool operator==(const MouseMoved&) const noexcept = default;
};

/// A mouse button went down over the window.
struct MouseButtonPressed {
    /// The button.
    MouseButton button;

    /// Compares two events exactly.
    constexpr bool operator==(const MouseButtonPressed&) const noexcept = default;
};

/// A mouse button came up over the window.
struct MouseButtonReleased {
    /// The button.
    MouseButton button;

    /// Compares two events exactly.
    constexpr bool operator==(const MouseButtonReleased&) const noexcept = default;
};

/// One thing that happened, of those the platform reports.
using Event = std::variant<WindowClosed, WindowResized, KeyPressed, KeyReleased, MouseMoved,
                           MouseButtonPressed, MouseButtonReleased>;

} // namespace wulfhilde
