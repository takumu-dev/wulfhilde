#include <wulfhilde/platform/platform.hpp>

#include "sdl_error.hpp"
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/platform/event.hpp>
#include <wulfhilde/platform/key.hpp>
#include <wulfhilde/platform/platform_error.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace wulfhilde {

namespace {

// SDL keeps its state in globals of its own, so only one platform may own it. Like every module,
// the platform is used from one thread.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): SDL's state is global too
constinit bool platform_exists = false;

// A key's value is its usage ID, which is SDL's scancode for it.
static_assert(std::to_underlying(Key::a) == SDL_SCANCODE_A);
static_assert(std::to_underlying(Key::w) == SDL_SCANCODE_W);
static_assert(std::to_underlying(Key::digit_0) == SDL_SCANCODE_0);
static_assert(std::to_underlying(Key::escape) == SDL_SCANCODE_ESCAPE);
static_assert(std::to_underlying(Key::space) == SDL_SCANCODE_SPACE);
static_assert(std::to_underlying(Key::f12) == SDL_SCANCODE_F12);
static_assert(std::to_underlying(Key::up) == SDL_SCANCODE_UP);
static_assert(std::to_underlying(Key::left_shift) == SDL_SCANCODE_LSHIFT);
static_assert(std::to_underlying(Key::right_gui) == SDL_SCANCODE_RGUI);

// SDL's numbers for its mouse buttons are the engine's.
static_assert(std::to_underlying(MouseButton::left) == SDL_BUTTON_LEFT);
static_assert(std::to_underlying(MouseButton::forward) == SDL_BUTTON_X2);

// Ends the system's video, keyboard and mouse, for the platform that owns them.
void shut_down() noexcept {
    SDL_Quit();
    platform_exists = false;
}

// The engine's event for one of SDL's, or nothing for an event the platform doesn't report.
std::optional<Event> event_of(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return WindowClosed{};
    case SDL_EVENT_WINDOW_RESIZED:
        return WindowResized{
            .size =
                {
                    .width = narrow<std::size_t>(event.window.data1),
                    .height = narrow<std::size_t>(event.window.data2),
                },
        };
    case SDL_EVENT_KEY_DOWN:
        if (event.key.repeat) {
            return std::nullopt;
        }
        return KeyPressed{.key = static_cast<Key>(event.key.scancode)};
    case SDL_EVENT_KEY_UP:
        return KeyReleased{.key = static_cast<Key>(event.key.scancode)};
    case SDL_EVENT_MOUSE_MOTION:
        // Outside relative mouse mode, the motion is the pointer's, which nothing asked for.
        if (SDL_Window* const window = SDL_GetWindowFromID(event.motion.windowID);
            window == nullptr || !SDL_GetWindowRelativeMouseMode(window)) {
            return std::nullopt;
        }
        return MouseMoved{.motion = {.x = event.motion.xrel, .y = event.motion.yrel}};
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (event.button.button < SDL_BUTTON_LEFT || event.button.button > SDL_BUTTON_X2) {
            return std::nullopt;
        }
        const auto button = static_cast<MouseButton>(event.button.button);
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            return MouseButtonPressed{.button = button};
        }
        return MouseButtonReleased{.button = button};
    }
    default:
        return std::nullopt;
    }
}

} // namespace

std::expected<Platform, PlatformError> Platform::create() {
    WULFHILDE_ASSERT(!platform_exists, "only one platform exists at a time");
    // A library leaves the program's signals alone, so Ctrl+C still ends it.
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        auto error = detail::sdl_error(PlatformErrorCode::no_video, "video couldn't start");
        SDL_Quit();
        return std::unexpected(std::move(error));
    }
    platform_exists = true;
    return Platform();
}

Platform::Platform(Platform&& other) noexcept : active_(std::exchange(other.active_, false)) {}

Platform& Platform::operator=(Platform&& other) noexcept {
    if (this != &other) {
        if (active_) {
            shut_down();
        }
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}

Platform::~Platform() {
    if (active_) {
        shut_down();
    }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): only a live platform reaches SDL
std::vector<Event> Platform::poll_events() {
    std::vector<Event> events;
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (auto reported = event_of(event)) {
            events.push_back(*reported);
        }
    }
    return events;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): only a live platform reaches SDL
bool Platform::is_key_down(Key key) const noexcept {
    int count = 0;
    const bool* const state = SDL_GetKeyboardState(&count);
    const auto keys = std::span(state, narrow<std::size_t>(count));
    const std::size_t index = std::to_underlying(key);
    return index < keys.size() && keys.subspan(index).front();
}

} // namespace wulfhilde
