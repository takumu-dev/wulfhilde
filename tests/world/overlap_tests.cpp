// The sectors a map finds sharing space with each sector, worked out when it is made.
#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using wulfhilde::SectorId;
using wulfhilde::Vector2;

namespace {

// A sector bounded by a loop of walls through the points, between a floor and a ceiling.
wulfhilde::SectorDescription sector_between(std::string id, float floor, float ceiling,
                                            std::initializer_list<Vector2> points) {
    auto sector = sector_through(std::move(id), points);
    sector.floor.height = floor;
    sector.ceiling.height = ceiling;
    return sector;
}

// A rectangle from one corner to the other, between a floor and a ceiling.
wulfhilde::SectorDescription rectangle(std::string id, Vector2 lowest, Vector2 highest,
                                       float floor = 0.0F, float ceiling = 3.0F) {
    return sector_between(std::move(id), floor, ceiling,
                          {
                              lowest,
                              {.x = highest.x, .y = lowest.y},
                              highest,
                              {.x = lowest.x, .y = highest.y},
                          });
}

// Which of a rectangle's walls a slope is hinged on: they run anticlockwise from its lowest corner.
enum class Side : std::uint8_t { south, east, north, west };

// A sector with its ceiling, or its floor, sloped from one of its walls at a gradient.
wulfhilde::SectorDescription sloped(wulfhilde::SectorDescription sector, Side hinge, bool ceiling,
                                    float gradient) {
    auto& wall = sector.loops.at(0).at(std::to_underlying(hinge));
    wall.id = sector.id + "-hinge";
    (ceiling ? sector.ceiling : sector.floor).slope =
        wulfhilde::SlopeDescription{.hinge = *wall.id, .gradient = gradient};
    return sector;
}

// A map of the sectors, where the player starts at the first point of the first's loop.
wulfhilde::Map map_of(std::vector<wulfhilde::SectorDescription> sectors) {
    const auto& first = sectors.front();
    const auto start = first.loops.front().front().start;
    wulfhilde::MapDescription description{
        .start = {.sector = first.id, .point = {.x = start.x, .y = start.y}},
    };
    description.sectors = std::move(sectors);
    return make_map(std::move(description));
}

// The sectors a map lists as overlapping one of its sectors.
std::vector<SectorId> overlapping(const wulfhilde::Map& map, SectorId sector) {
    const auto found = map.overlapping(sector);
    return {found.begin(), found.end()};
}

// Whether a map lists each of its two sectors as overlapping the other.
bool overlap(const wulfhilde::Map& map) {
    return overlapping(map, SectorId{0}) == std::vector{SectorId{1}} &&
           overlapping(map, SectorId{1}) == std::vector{SectorId{0}};
}

// Whether a map lists neither of its two sectors as overlapping the other.
bool apart(const wulfhilde::Map& map) {
    return overlapping(map, SectorId{0}).empty() && overlapping(map, SectorId{1}).empty();
}

} // namespace

TEST_CASE("a sector within another's walls, between the same heights, overlaps it", "[world]") {
    const auto map = map_of({
        rectangle("yard", {.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 10.0F}),
        rectangle("hall", {.x = 2.0F, .y = 3.0F}, {.x = 6.0F, .y = 8.0F}),
    });

    CHECK(overlap(map));
}

TEST_CASE("sectors that meet only at a wall or a corner, or not at all, don't overlap", "[world]") {
    CHECK(apart(make_map(doorway_map())));
    CHECK(apart(map_of({
        rectangle("west", {.x = 0.0F, .y = 0.0F}, {.x = 4.0F, .y = 4.0F}),
        rectangle("east", {.x = 4.0F, .y = 1.0F}, {.x = 8.0F, .y = 3.0F}),
    })));
    CHECK(apart(map_of({
        rectangle("south-west", {.x = 0.0F, .y = 0.0F}, {.x = 4.0F, .y = 4.0F}),
        rectangle("north-east", {.x = 4.0F, .y = 4.0F}, {.x = 8.0F, .y = 8.0F}),
    })));
    CHECK(apart(map_of({
        rectangle("here", {.x = 0.0F, .y = 0.0F}, {.x = 4.0F, .y = 4.0F}),
        rectangle("there", {.x = 20.0F, .y = 0.0F}, {.x = 24.0F, .y = 4.0F}),
    })));
}

TEST_CASE("a bridge over a passage, from the passage's ceiling up, doesn't overlap it, though "
          "their walls cross seen from above",
          "[world]") {
    const auto map = map_of({
        rectangle("passage", {.x = 0.0F, .y = 2.0F}, {.x = 10.0F, .y = 4.0F}, 0.0F, 3.0F),
        rectangle("bridge", {.x = 4.0F, .y = 0.0F}, {.x = 6.0F, .y = 6.0F}, 3.0F, 6.0F),
    });

    CHECK(apart(map));
}

TEST_CASE("a bridge whose floor is below the passage's ceiling overlaps it", "[world]") {
    const auto map = map_of({
        rectangle("passage", {.x = 0.0F, .y = 2.0F}, {.x = 10.0F, .y = 4.0F}, 0.0F, 3.0F),
        rectangle("bridge", {.x = 4.0F, .y = 0.0F}, {.x = 6.0F, .y = 6.0F}, 2.9F, 6.0F),
    });

    CHECK(overlap(map));
}

TEST_CASE("sectors whose walls cross, with no corner of either inside the other, overlap",
          "[world]") {
    const auto map = map_of({
        rectangle("along-x", {.x = 0.0F, .y = 2.0F}, {.x = 6.0F, .y = 4.0F}),
        rectangle("along-y", {.x = 2.0F, .y = 0.0F}, {.x = 4.0F, .y = 6.0F}),
    });

    CHECK(overlap(map));
}

TEST_CASE("sectors of the same outline between the same heights overlap", "[world]") {
    const auto map = map_of({
        rectangle("one", {.x = 0.0F, .y = 0.0F}, {.x = 4.0F, .y = 4.0F}),
        rectangle("other", {.x = 0.0F, .y = 0.0F}, {.x = 4.0F, .y = 4.0F}),
    });

    CHECK(overlap(map));
}

TEST_CASE("a sector in the notch of a concave sector doesn't overlap it, though it lies within the "
          "concave sector's bounds",
          "[world]") {
    // The L's arms are 1 m wide and 4 m long, so its notch runs from (1, 1) to (4, 4).
    const auto l_shape = l_shaped_hall_map().sectors.at(0);

    CHECK(apart(
        map_of({l_shape, rectangle("notch", {.x = 1.0F, .y = 1.0F}, {.x = 4.0F, .y = 4.0F})})));
    CHECK(apart(
        map_of({l_shape, rectangle("inside", {.x = 2.0F, .y = 2.0F}, {.x = 3.0F, .y = 3.0F})})));
    CHECK(overlap(
        map_of({l_shape, rectangle("over", {.x = 0.5F, .y = 0.5F}, {.x = 3.0F, .y = 3.0F})})));
}

TEST_CASE("a map lists the sectors overlapping each of its sectors in order of their IDs, and none "
          "overlaps itself",
          "[world]") {
    const auto map = map_of({
        rectangle("yard", {.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 10.0F}),
        rectangle("hut", {.x = 2.0F, .y = 2.0F}, {.x = 4.0F, .y = 4.0F}),
        rectangle("lane", {.x = 20.0F, .y = 0.0F}, {.x = 22.0F, .y = 10.0F}),
        rectangle("shed", {.x = 6.0F, .y = 6.0F}, {.x = 8.0F, .y = 8.0F}),
    });

    CHECK(overlapping(map, SectorId{0}) == std::vector{SectorId{1}, SectorId{3}});
    CHECK(overlapping(map, SectorId{1}) == std::vector{SectorId{0}});
    CHECK(overlapping(map, SectorId{2}).empty());
    CHECK(overlapping(map, SectorId{3}) == std::vector{SectorId{0}});
}

TEST_CASE("a bridge over a passage whose ceiling slopes overlaps it only where the ceiling rises "
          "above the bridge's floor within the area they share seen from above",
          "[world]") {
    const auto bridge =
        rectangle("bridge", {.x = 4.0F, .y = 0.0F}, {.x = 6.0F, .y = 6.0F}, 3.0F, 6.0F);
    const auto passage = [](float ceiling, Side hinge, float gradient) {
        return sloped(
            rectangle("passage", {.x = 0.0F, .y = 2.0F}, {.x = 10.0F, .y = 4.0F}, 0.0F, ceiling),
            hinge, true, gradient);
    };

    // Across the passage, the ceiling rises from 2 m to 2.9 m, under the bridge's floor.
    CHECK(apart(map_of({passage(2.0F, Side::south, 0.45F), bridge})));
    // Along it, the ceiling rises from 2.6 m to 3.4 m beneath the bridge, into it.
    CHECK(overlap(map_of({passage(1.0F, Side::west, 0.4F), bridge})));
    // Along it, the ceiling rises from 2.2 m to 2.8 m beneath the bridge, and above 3 m only
    // beyond it.
    CHECK(apart(map_of({passage(1.0F, Side::west, 0.3F), bridge})));
}

TEST_CASE("a bridge whose floor slopes down into a passage beneath it overlaps it", "[world]") {
    const auto passage =
        rectangle("passage", {.x = 0.0F, .y = 2.0F}, {.x = 10.0F, .y = 4.0F}, 0.0F, 3.0F);
    const auto bridge = [](float gradient) {
        return sloped(
            rectangle("bridge", {.x = 4.0F, .y = 0.0F}, {.x = 6.0F, .y = 6.0F}, 4.0F, 8.0F),
            Side::south, false, gradient);
    };

    // The bridge's floor falls from 3 m to 2 m over the passage.
    CHECK(overlap(map_of({passage, bridge(-0.5F)})));
    // It falls only to 3 m, the passage's ceiling, at the passage's far wall.
    CHECK(apart(map_of({passage, bridge(-0.25F)})));
}

namespace {

// The yard, a square 10 m on a side, around a hole from (2, 2) to (6, 6).
wulfhilde::SectorDescription yard_around_hole() {
    auto yard = rectangle("yard", {.x = 0.0F, .y = 0.0F}, {.x = 10.0F, .y = 10.0F});
    yard.loops.push_back(walls_through({
        {.x = 2.0F, .y = 2.0F},
        {.x = 2.0F, .y = 6.0F},
        {.x = 6.0F, .y = 6.0F},
        {.x = 6.0F, .y = 2.0F},
    }));
    return yard;
}

} // namespace

TEST_CASE("a sector in another's hole shares no space with it, whether it fills the hole or not",
          "[world]") {
    CHECK(apart(map_of({
        yard_around_hole(),
        rectangle("dais", {.x = 2.0F, .y = 2.0F}, {.x = 6.0F, .y = 6.0F}),
    })));
    CHECK(apart(map_of({
        yard_around_hole(),
        rectangle("shed", {.x = 3.0F, .y = 3.0F}, {.x = 5.0F, .y = 4.0F}),
    })));
}

TEST_CASE("a sector reaching out of another's hole, or round it, overlaps it", "[world]") {
    CHECK(overlap(map_of({
        yard_around_hole(),
        rectangle("shed", {.x = 5.0F, .y = 3.0F}, {.x = 8.0F, .y = 4.0F}),
    })));
    CHECK(overlap(map_of({
        yard_around_hole(),
        rectangle("hall", {.x = 1.0F, .y = 1.0F}, {.x = 7.0F, .y = 7.0F}),
    })));
}

TEST_CASE("a bridge over another sector's hole and its edge overlaps it only where they share "
          "height outside the hole",
          "[world]") {
    // The yard's ceiling falls northward from 4.5 m at y = 3, over the hole, to 3 m at the hole's
    // north edge and 2.5 m at the bridge's far end.
    auto yard = yard_around_hole();
    yard.loops.at(0).at(0).id = "yard-south";
    yard.ceiling.height = 6.0F;
    yard.ceiling.slope = wulfhilde::SlopeDescription{.hinge = "yard-south", .gradient = -0.5F};
    const auto bridge = [](float floor) {
        return rectangle("bridge", {.x = 3.0F, .y = 3.0F}, {.x = 5.0F, .y = 7.0F}, floor, 8.0F);
    };

    CHECK(apart(map_of({yard, bridge(3.5F)})));
    CHECK(overlap(map_of({yard, bridge(2.8F)})));
}
