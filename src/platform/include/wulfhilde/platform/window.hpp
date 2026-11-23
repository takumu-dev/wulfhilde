/// \file
/// A window on the desktop, which the renderer draws into.

#pragma once

#include <wulfhilde/platform/platform.hpp>
#include <wulfhilde/platform/platform_error.hpp>
#include <wulfhilde/platform/window_size.hpp>

#include <expected>
#include <memory>
#include <string>

namespace wulfhilde {

namespace detail {

/// The system's window behind a `Window`.
class NativeWindow;

/// The platform's way to the system's window behind a `Window`, for the renderer's surface.
struct WindowAccess;

} // namespace detail

/// How a window opens.
struct WindowSettings {
    /// The title, in UTF-8, which the desktop shows on its title bar and in its task list.
    std::string title{};

    /// The size of its drawable area, neither side of it zero. A fullscreen window takes the
    /// display's size instead.
    WindowSize size{.width = 1280, .height = 720};

    /// Whether it covers the display it opens on, with no border, instead of opening at its size.
    bool fullscreen = false;
};

/// A window on the desktop, which the person can resize, and which the renderer draws into.
///
/// It must not outlive the platform it was made with.
class Window {
  public:
    /// Opens a window.
    ///
    /// \param platform The platform it belongs to.
    /// \param settings How it opens.
    /// \return The window, or why it couldn't open, such as when Vulkan can't draw into it.
    [[nodiscard]] static std::expected<Window, PlatformError>
    create(Platform& platform, const WindowSettings& settings);

    /// Takes over another window, leaving it empty, fit only to be destroyed or assigned to.
    ///
    /// \param other The window to take over.
    Window(Window&& other) noexcept;

    /// Takes over another window, closing this one first.
    ///
    /// \param other The window to take over.
    /// \return This window.
    Window& operator=(Window&& other) noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// Closes the window.
    ~Window();

    /// Returns the size of the window's drawable area now.
    [[nodiscard]] WindowSize size() const;

    /// Turns relative mouse mode on or off.
    ///
    /// While it is on and the window has the keyboard's focus, the pointer is hidden and held in
    /// the window, and the mouse's movements arrive as `MouseMoved` events, however far it goes.
    ///
    /// \param enabled Whether it is on.
    /// \return Nothing, or why the system refused.
    [[nodiscard]] std::expected<void, PlatformError> set_relative_mouse_mode(bool enabled);

    /// Answers whether relative mouse mode is on.
    [[nodiscard]] bool relative_mouse_mode() const;

  private:
    friend detail::WindowAccess;

    /// Takes the system's window that `create` opened.
    ///
    /// \param native The system's window.
    explicit Window(std::unique_ptr<detail::NativeWindow> native) noexcept;

    /// The system's window.
    std::unique_ptr<detail::NativeWindow> native_;
};

} // namespace wulfhilde
