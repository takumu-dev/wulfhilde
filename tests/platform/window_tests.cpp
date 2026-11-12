#include <wulfhilde/platform/window.hpp>

#include <wulfhilde/platform/platform.hpp>
#include <wulfhilde/platform/platform_error.hpp>
#include <wulfhilde/platform/window_size.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <expected>
#include <format>
#include <utility>

namespace {

// Requires a result from the platform to hold a value, failing with its error when it doesn't.
template <typename T>
void require_platform_value(const std::expected<T, wulfhilde::PlatformError>& result) {
    if (!result) {
        FAIL(std::format("{}", result.error()));
    }
}

} // namespace

TEST_CASE("a window opens at the size it is asked for", "[platform]") {
    auto platform = wulfhilde::Platform::create();
    require_platform_value(platform);

    const auto window = wulfhilde::Window::create(
        *platform, {.title = "a window", .size = {.width = 640, .height = 360}});

    require_platform_value(window);
    CHECK(window->size() == wulfhilde::WindowSize{.width = 640, .height = 360});
}

// The tests' windows open offscreen, where there is no mouse to hold, so the system refuses.
TEST_CASE("relative mouse mode the system can't give is refused, naming why, and stays off",
          "[platform]") {
    auto platform = wulfhilde::Platform::create();
    require_platform_value(platform);
    auto window = wulfhilde::Window::create(*platform, {.title = "a window"});
    require_platform_value(window);
    CHECK(!window->relative_mouse_mode());

    const auto turned_on = window->set_relative_mouse_mode(true);

    REQUIRE(!turned_on.has_value());
    CHECK(turned_on.error().code == wulfhilde::PlatformErrorCode::window_failure);
    CHECK_THAT(turned_on.error().message,
               Catch::Matchers::StartsWith("relative mouse mode couldn't turn on: ") &&
                   !Catch::Matchers::EndsWith(": "));
    CHECK(!window->relative_mouse_mode());
    require_platform_value(window->set_relative_mouse_mode(false));
}

TEST_CASE("a window moved from leaves the window open in the one it moved to", "[platform]") {
    auto platform = wulfhilde::Platform::create();
    require_platform_value(platform);
    auto window = wulfhilde::Window::create(
        *platform, {.title = "a window", .size = {.width = 320, .height = 240}});
    require_platform_value(window);

    const auto moved = std::move(*window);

    CHECK(moved.size() == wulfhilde::WindowSize{.width = 320, .height = 240});
}
