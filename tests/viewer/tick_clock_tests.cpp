// How many of the controller's ticks are due each frame, as time passes.
#include "tick_clock.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

TEST_CASE("ticks fall due at 240 a second, keeping what is left of one for the next frame",
          "[viewer]") {
    viewer::TickClock clock;

    CHECK(clock.advance(std::chrono::milliseconds(200)) == 48);
    // A 60 Hz frame is four ticks, and a 144 Hz frame one and two thirds.
    CHECK(clock.advance(std::chrono::nanoseconds(16'666'667)) == 4);
    CHECK(clock.advance(std::chrono::nanoseconds(6'944'444)) == 1);
    CHECK(clock.advance(std::chrono::nanoseconds(6'944'444)) == 2);
    CHECK(clock.advance(std::chrono::nanoseconds(6'944'445)) == 2);
    CHECK(clock.advance(std::chrono::nanoseconds(0)) == 0);
}

TEST_CASE("ticks fall due for at most a quarter of a second a frame, however long it took",
          "[viewer]") {
    viewer::TickClock clock;

    CHECK(clock.advance(std::chrono::minutes(10)) == 60);
    // Nothing is owed from the time that went beyond the quarter.
    CHECK(clock.advance(std::chrono::milliseconds(1)) == 0);
    CHECK(clock.advance(std::chrono::milliseconds(3)) == 0);
    CHECK(clock.advance(std::chrono::milliseconds(1)) == 1);
}
