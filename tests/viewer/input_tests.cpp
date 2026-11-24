// What the player asks of the viewer, as the keyboard and the mouse tell it.
#include "input.hpp"

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/platform/event.hpp>
#include <wulfhilde/platform/key.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <optional>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Key;
using wulfhilde::KeyPressed;
using wulfhilde::KeyReleased;

using Events = std::vector<wulfhilde::Event>;

// Tells whether two angles are within a millionth of a degree of each other.
bool near(Angle a, Angle b) { return std::abs((a - b).in_degrees()) < 1e-6F; }

} // namespace

TEST_CASE("W, S, A and D walk forward, back, left and right while they are held", "[viewer]") {
    viewer::Input input(Angle::degrees(0.1F));

    const auto forward = input.follow(Events{KeyPressed{Key::w}});
    CHECK(forward.actions.forward == 1.0F);
    CHECK(forward.actions.strafe == 0.0F);
    // Held, without another event.
    CHECK(input.follow({}).actions.forward == 1.0F);

    const auto forward_left = input.follow(Events{KeyPressed{Key::a}});
    CHECK(forward_left.actions.forward == 1.0F);
    CHECK(forward_left.actions.strafe == 1.0F);

    const auto left = input.follow(Events{KeyPressed{Key::s}});
    CHECK(left.actions.forward == 0.0F);
    CHECK(left.actions.strafe == 1.0F);

    const auto back_right =
        input.follow(Events{KeyReleased{Key::w}, KeyReleased{Key::a}, KeyPressed{Key::d}});
    CHECK(back_right.actions.forward == -1.0F);
    CHECK(back_right.actions.strafe == -1.0F);

    const auto still = input.follow(Events{KeyReleased{Key::s}, KeyReleased{Key::d}});
    CHECK(still.actions.forward == 0.0F);
    CHECK(still.actions.strafe == 0.0F);
}

TEST_CASE("either Shift runs and Space jumps while they are held", "[viewer]") {
    viewer::Input input(Angle::degrees(0.1F));

    CHECK(!input.follow({}).actions.run);
    CHECK(input.follow(Events{KeyPressed{Key::left_shift}}).actions.run);
    CHECK(input.follow(Events{KeyPressed{Key::right_shift}, KeyReleased{Key::left_shift}})
              .actions.run);
    CHECK(!input.follow(Events{KeyReleased{Key::right_shift}}).actions.run);

    CHECK(input.follow(Events{KeyPressed{Key::space}}).actions.jump);
    CHECK(input.follow({}).actions.jump);
    CHECK(!input.follow(Events{KeyReleased{Key::space}}).actions.jump);
}

TEST_CASE("the mouse turns the eye by the sensitivity for each count: right and toward the player "
          "turn it right and down",
          "[viewer]") {
    viewer::Input input(Angle::degrees(0.5F));

    const auto frame = input.follow(Events{
        wulfhilde::MouseMoved{.motion = {.x = 10.0F, .y = -4.0F}},
        wulfhilde::MouseMoved{.motion = {.x = 2.0F, .y = 6.0F}},
    });

    CHECK(near(frame.yaw, Angle::degrees(-6.0F)));
    CHECK(near(frame.pitch, Angle::degrees(-1.0F)));
    CHECK(near(input.follow({}).yaw, Angle{}));
}

TEST_CASE("R returns to the start and C prints the eye, once for each press", "[viewer]") {
    viewer::Input input(Angle::degrees(0.1F));

    const auto pressed = input.follow(Events{KeyPressed{Key::r}, KeyPressed{Key::c}});
    CHECK(pressed.return_to_start);
    CHECK(pressed.print_eye);

    const auto held = input.follow({});
    CHECK(!held.return_to_start);
    CHECK(!held.print_eye);
}

TEST_CASE("Esc lets the mouse go, so it no longer looks, and a click takes it back", "[viewer]") {
    viewer::Input input(Angle::degrees(1.0F));
    const wulfhilde::MouseMoved moved{.motion = {.x = 1.0F}};
    const wulfhilde::MouseButtonPressed clicked{wulfhilde::MouseButton::left};

    // A click while the window has the mouse changes nothing.
    CHECK(input.follow(Events{clicked}).capture_mouse == std::nullopt);

    const auto released = input.follow(Events{KeyPressed{Key::escape}, moved});
    CHECK(released.capture_mouse == false);
    CHECK(near(released.yaw, Angle{}));
    CHECK(input.follow(Events{KeyPressed{Key::escape}}).capture_mouse == std::nullopt);

    const auto recaptured = input.follow(Events{clicked, moved});
    CHECK(recaptured.capture_mouse == true);
    CHECK(near(recaptured.yaw, Angle::degrees(-1.0F)));
    CHECK(input.follow({}).capture_mouse == std::nullopt);
}
