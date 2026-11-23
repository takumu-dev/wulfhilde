#include <wulfhilde/platform/platform.hpp>

#include <wulfhilde/platform/key.hpp>

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <utility>

TEST_CASE("keys are named by their usage IDs on the USB HID keyboard page", "[platform]") {
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::a) == 0x04);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::w) == 0x1A);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::digit_1) == 0x1E);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::digit_0) == 0x27);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::escape) == 0x29);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::space) == 0x2C);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::f12) == 0x45);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::up) == 0x52);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::left_shift) == 0xE1);
    STATIC_CHECK(std::to_underlying(wulfhilde::Key::right_gui) == 0xE7);
}

TEST_CASE("a platform comes up with no key down and nothing to report", "[platform]") {
    auto platform = wulfhilde::Platform::create();
    if (!platform) {
        FAIL(std::format("{}", platform.error()));
    }

    CHECK(platform->poll_events().empty());
    CHECK(!platform->is_key_down(wulfhilde::Key::w));
    CHECK(!platform->is_key_down(wulfhilde::Key::left_shift));
}

TEST_CASE("a platform comes up again once the one before it is gone", "[platform]") {
    {
        auto first = wulfhilde::Platform::create();
        REQUIRE(first.has_value());
        // The platform moved to is the one that lives on, until the end of this scope.
        auto moved = std::move(*first);
        CHECK(moved.poll_events().empty());
    }

    const auto second = wulfhilde::Platform::create();

    if (!second) {
        FAIL(std::format("{}", second.error()));
    }
}
