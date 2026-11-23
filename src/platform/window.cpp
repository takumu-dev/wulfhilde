#include <wulfhilde/platform/window.hpp>

#include "native_window.hpp"
#include "sdl_error.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/platform/platform.hpp>
#include <wulfhilde/platform/platform_error.hpp>
#include <wulfhilde/platform/window_size.hpp>

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <cstddef>
#include <expected>
#include <memory>
#include <utility>

namespace wulfhilde {

std::expected<Window, PlatformError> Window::create(Platform& /*platform*/,
                                                    const WindowSettings& settings) {
    WULFHILDE_ASSERT(settings.size.width > 0 && settings.size.height > 0,
                     "a window's size has neither side zero");
    // Every window is one that Vulkan draws into, at its display's own pixels.
    SDL_WindowFlags flags =
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (settings.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }
    SDL_Window* const window =
        SDL_CreateWindow(settings.title.c_str(), narrow<int>(settings.size.width),
                         narrow<int>(settings.size.height), flags);
    if (window == nullptr) {
        return std::unexpected(
            detail::sdl_error(PlatformErrorCode::window_failure, "the window couldn't open"));
    }
    return Window(std::make_unique<detail::NativeWindow>(window));
}

Window::Window(std::unique_ptr<detail::NativeWindow> native) noexcept
    : native_(std::move(native)) {}

Window::Window(Window&& other) noexcept = default;

Window& Window::operator=(Window&& other) noexcept = default;

Window::~Window() = default;

WindowSize Window::size() const {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(native_->get(), &width, &height);
    return {.width = narrow<std::size_t>(width), .height = narrow<std::size_t>(height)};
}

std::expected<void, PlatformError> Window::set_relative_mouse_mode(bool enabled) {
    if (!SDL_SetWindowRelativeMouseMode(native_->get(), enabled)) {
        return std::unexpected(detail::sdl_error(
            PlatformErrorCode::window_failure, enabled ? "relative mouse mode couldn't turn on"
                                                       : "relative mouse mode couldn't turn off"));
    }
    return {};
}

bool Window::relative_mouse_mode() const { return SDL_GetWindowRelativeMouseMode(native_->get()); }

} // namespace wulfhilde
