// When the viewer draws, rests or quits, as the events of its window say.
#include "pacing.hpp"

#include <wulfhilde/platform/event.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("a minimised window rests the viewer until it is restored", "[viewer]") {
    viewer::Pacing pacing;
    CHECK(pacing.follow({}) == viewer::Pace::draw);

    CHECK(pacing.follow(std::vector<wulfhilde::Event>{wulfhilde::WindowMinimized{}}) ==
          viewer::Pace::rest);
    CHECK(pacing.follow({}) == viewer::Pace::rest);
    CHECK(pacing.follow(std::vector<wulfhilde::Event>{wulfhilde::WindowRestored{}}) ==
          viewer::Pace::draw);
    CHECK(pacing.follow({}) == viewer::Pace::draw);
}

TEST_CASE("a closed window quits the viewer, even while it is minimised", "[viewer]") {
    viewer::Pacing drawing;
    viewer::Pacing resting;
    REQUIRE(resting.follow(std::vector<wulfhilde::Event>{wulfhilde::WindowMinimized{}}) ==
            viewer::Pace::rest);

    const std::vector<wulfhilde::Event> closed{wulfhilde::WindowClosed{}};

    CHECK(drawing.follow(closed) == viewer::Pace::quit);
    CHECK(resting.follow(closed) == viewer::Pace::quit);
}

TEST_CASE("of events that minimise and restore the window, the latest decides", "[viewer]") {
    viewer::Pacing pacing;

    CHECK(pacing.follow(std::vector<wulfhilde::Event>{
              wulfhilde::WindowMinimized{}, wulfhilde::WindowRestored{}}) == viewer::Pace::draw);
    CHECK(pacing.follow(std::vector<wulfhilde::Event>{
              wulfhilde::WindowRestored{}, wulfhilde::WindowMinimized{}}) == viewer::Pace::rest);
}
