// The system's window behind a `Window`, as SDL has it.
#pragma once

#include <wulfhilde/platform/window.hpp>

#include <SDL3/SDL_video.h>

namespace wulfhilde::detail {

/// Owns one of SDL's windows, which it destroys.
class NativeWindow {
  public:
    /// Takes ownership of a window that SDL made.
    explicit NativeWindow(SDL_Window* window) noexcept : window_(window) {}

    NativeWindow(const NativeWindow&) = delete;
    NativeWindow(NativeWindow&&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;
    NativeWindow& operator=(NativeWindow&&) = delete;

    ~NativeWindow() { SDL_DestroyWindow(window_); }

    /// Returns the window, still owned.
    [[nodiscard]] SDL_Window* get() const noexcept { return window_; }

  private:
    SDL_Window* window_;
};

/// Reaches the SDL window behind a `Window`, which only the platform's own sources do.
struct WindowAccess {
    /// Returns a window's SDL window, still owned by it.
    [[nodiscard]] static SDL_Window* get(const Window& window) noexcept {
        return window.native_->get();
    }
};

} // namespace wulfhilde::detail
