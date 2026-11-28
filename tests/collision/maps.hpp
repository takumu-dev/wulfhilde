// Maps built in code, and a body, for collision's tests to move through them.
#pragma once

#include <wulfhilde/collision/body_shape.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <format>
#include <initializer_list>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// A body the size of a person: 0.3 m in radius and 1.8 m tall, stepping up 0.4 m and standing on
// slopes up to 45 degrees.
inline constexpr wulfhilde::BodyShape person{
    .radius = 0.3F,
    .height = 1.8F,
    .step_height = 0.4F,
    .steepest_slope = wulfhilde::Angle::degrees(45.0F),
};

// A sector bounded by a loop of walls through the points, between a floor and a ceiling.
inline wulfhilde::SectorDescription
sector_through(std::string id, float floor, float ceiling,
               std::initializer_list<wulfhilde::Vector2> points) {
    std::vector<wulfhilde::WallDescription> walls;
    for (const auto& point : points) {
        walls.push_back({.start = point, .surface = {.material = "test/wall"}});
    }
    return {
        .id = std::move(id),
        .floor = {.height = floor, .material = "test/floor"},
        .ceiling = {.height = ceiling, .material = "test/ceiling"},
        .loops = {std::move(walls)},
    };
}

// A map of one sector, where the player starts at the first point of its loop.
inline wulfhilde::Map one_sector_map(wulfhilde::SectorDescription sector) {
    const auto& first = sector.loops.front().front().start;
    wulfhilde::MapDescription description{
        .start =
            {
                .sector = sector.id,
                .point = {.x = first.x, .y = first.y, .z = sector.floor.height},
            },
    };
    description.sectors.push_back(std::move(sector));
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The hall: a square sector 4 m on a side from (0, 0) to (4, 4), with its floor at 0 m and its
// ceiling at 3 m. Its walls run anticlockwise from the corner at the origin: south, east, north,
// west.
inline wulfhilde::Map hall_map() {
    return one_sector_map(sector_through("hall", 0.0F, 3.0F,
                                         {
                                             {.x = 0.0F, .y = 0.0F},
                                             {.x = 4.0F, .y = 0.0F},
                                             {.x = 4.0F, .y = 4.0F},
                                             {.x = 0.0F, .y = 4.0F},
                                         }));
}

// The corridor: a sector 6 m long along X and 1 m wide, from (0, 0) to (6, 1), with its floor at
// 0 m and its ceiling at 3 m.
inline wulfhilde::Map corridor_map() {
    return one_sector_map(sector_through("corridor", 0.0F, 3.0F,
                                         {
                                             {.x = 0.0F, .y = 0.0F},
                                             {.x = 6.0F, .y = 0.0F},
                                             {.x = 6.0F, .y = 1.0F},
                                             {.x = 0.0F, .y = 1.0F},
                                         }));
}

// The description of the hall with a doorway 2 m wide in its north wall, from (1, 4) to (3, 4),
// into a sector beyond it, at heights of its own, whose loop runs from the door's corners on
// through the given points.
//
// The hall's walls run anticlockwise from the corner at the origin, its door the fourth, named
// "hall-door"; the sector beyond's run from the door's west corner, its door the first, named
// "beyond-door".
inline wulfhilde::MapDescription
doorway_description(float floor_beyond, float ceiling_beyond,
                    std::initializer_list<wulfhilde::Vector2> beyond) {
    auto hall = sector_through("hall", 0.0F, 3.0F,
                               {
                                   {.x = 0.0F, .y = 0.0F},
                                   {.x = 4.0F, .y = 0.0F},
                                   {.x = 4.0F, .y = 4.0F},
                                   {.x = 3.0F, .y = 4.0F},
                                   {.x = 1.0F, .y = 4.0F},
                                   {.x = 0.0F, .y = 4.0F},
                               });
    hall.loops.front().at(3).id = "hall-door";

    std::vector<wulfhilde::Vector2> points{{.x = 1.0F, .y = 4.0F}, {.x = 3.0F, .y = 4.0F}};
    points.insert(points.end(), beyond);
    auto far = sector_through("beyond", floor_beyond, ceiling_beyond, {});
    for (const auto& point : points) {
        far.loops.front().push_back({.start = point, .surface = {.material = "test/wall"}});
    }
    far.loops.front().front().id = "beyond-door";

    return {
        .sectors = {std::move(hall), std::move(far)},
        .portals = {{.a = {.id = "hall-door"}, .b = {.id = "beyond-door"}}},
        .start = {.sector = "hall", .point = {.x = 2.0F, .y = 2.0F}},
    };
}

// Makes a map from a description that must be valid, failing the test with its problems if it
// isn't.
inline wulfhilde::Map make_map(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The hall with a doorway 2 m wide in its north wall into a sector beyond it, as
// `doorway_description` describes it.
inline wulfhilde::Map doorway_map(float floor_beyond, float ceiling_beyond,
                                  std::initializer_list<wulfhilde::Vector2> beyond) {
    return make_map(doorway_description(floor_beyond, ceiling_beyond, beyond));
}

// The hall with a doorway into the gallery, a square sector 4 m on a side from (0, 4) to (4, 8),
// wider than the doorway, with the hall's floor and ceiling.
inline wulfhilde::Map gallery_map() {
    return doorway_map(0.0F, 3.0F,
                       {
                           {.x = 4.0F, .y = 4.0F},
                           {.x = 4.0F, .y = 8.0F},
                           {.x = 0.0F, .y = 8.0F},
                           {.x = 0.0F, .y = 4.0F},
                       });
}

// The hall with a doorway into a corridor as wide as the doorway, 4 m long, from (1, 4) to
// (3, 8), at heights of its own.
inline wulfhilde::Map corridor_beyond_map(float floor = 0.0F, float ceiling = 3.0F) {
    return doorway_map(floor, ceiling, {{.x = 3.0F, .y = 8.0F}, {.x = 1.0F, .y = 8.0F}});
}

// The hall with a doorway in its north wall, from (1, 4) to (3, 4), through a transformed portal
// into the turned gallery far off: a sector running back west from the door's far side, from
// x = 20 - depth to 20 and y = 0 to 4, with the hall's span from floor to ceiling raised by a lift.
// The portal turns 90 degrees left and moves 24 m along X and the lift up, so the hall's door,
// from (3, 4) to (1, 4), lands back along the gallery's, from (20, 1) to (20, 3), and walking north
// out of the hall walks west into the gallery.
//
// The hall's walls run anticlockwise from the corner at the origin, its door the fourth; the
// gallery's from its south-west corner, its door the third.
inline wulfhilde::Map turned_gallery_map(float depth = 4.0F, float lift = 0.0F) {
    auto hall = sector_through("hall", 0.0F, 3.0F,
                               {
                                   {.x = 0.0F, .y = 0.0F},
                                   {.x = 4.0F, .y = 0.0F},
                                   {.x = 4.0F, .y = 4.0F},
                                   {.x = 3.0F, .y = 4.0F},
                                   {.x = 1.0F, .y = 4.0F},
                                   {.x = 0.0F, .y = 4.0F},
                               });
    hall.loops.front().at(3).id = "hall-door";
    const auto west = 20.0F - depth;
    auto gallery = sector_through("gallery", lift, lift + 3.0F,
                                  {
                                      {.x = west, .y = 0.0F},
                                      {.x = 20.0F, .y = 0.0F},
                                      {.x = 20.0F, .y = 1.0F},
                                      {.x = 20.0F, .y = 3.0F},
                                      {.x = 20.0F, .y = 4.0F},
                                      {.x = west, .y = 4.0F},
                                  });
    gallery.loops.front().at(2).id = "gallery-door";

    const wulfhilde::PortalDescription door{
        .a = {.id = "hall-door"},
        .b = {.id = "gallery-door"},
        .transform =
            wulfhilde::PortalTransformDescription{
                .translation = {.x = 24.0F, .z = lift},
                .rotation = {.yaw = wulfhilde::Angle::degrees(90.0F)},
            },
    };
    wulfhilde::MapDescription description{
        .sectors = {std::move(hall), std::move(gallery)},
        .portals = {door},
        .start = {.sector = "hall", .point = {.x = 2.0F, .y = 2.0F}},
    };
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The great hall: a square sector 8 m on a side from (0, 0) to (8, 8), with its floor at 0 m and
// its ceiling at 3 m, around a hole whose inner loop runs clockwise through the points. Its outer
// loop's walls are #0 to #3, and its inner loop's run on from #4.
inline wulfhilde::SectorDescription great_hall(const std::vector<wulfhilde::Vector2>& hole) {
    auto hall = sector_through("hall", 0.0F, 3.0F,
                               {
                                   {.x = 0.0F, .y = 0.0F},
                                   {.x = 8.0F, .y = 0.0F},
                                   {.x = 8.0F, .y = 8.0F},
                                   {.x = 0.0F, .y = 8.0F},
                               });
    auto& inner = hall.loops.emplace_back();
    for (const auto& point : hole) {
        inner.push_back({.start = point, .surface = {.material = "test/wall"}});
    }
    return hall;
}

// The great hall with a pillar in its middle, a square from (3, 3) to (5, 5), whose walls run
// clockwise from the corner nearest the origin: west, north, east and south.
inline wulfhilde::Map pillar_map() {
    return one_sector_map(great_hall({
        {.x = 3.0F, .y = 3.0F},
        {.x = 3.0F, .y = 5.0F},
        {.x = 5.0F, .y = 5.0F},
        {.x = 5.0F, .y = 3.0F},
    }));
}

// The great hall around a raised sector, with its floor at a height and its ceiling at the hall's,
// whose loop runs anticlockwise through the hole's points. Each wall of the hall's inner loop is a
// side of a portal into it.
inline wulfhilde::Map raised_map(float floor, const std::vector<wulfhilde::Vector2>& hole) {
    auto hall = great_hall(hole);
    auto raised = sector_through("raised", floor, 3.0F, {});
    for (const auto& point : hole | std::views::reverse) {
        raised.loops.front().push_back({.start = point, .surface = {.material = "test/wall"}});
    }

    wulfhilde::MapDescription description{
        .start = {.sector = "hall", .point = {.x = 1.0F, .y = 1.0F}},
    };
    // Of n walls, the hall's from the hole's point i to the next runs back as the raised sector's
    // wall n - 2 - i, counted round its loop.
    const auto count = hole.size();
    for (std::size_t i = 0; i < count; ++i) {
        hall.loops.back().at(i).id = std::format("hall-{}", i);
        raised.loops.front().at(((2 * count) - 2 - i) % count).id = std::format("raised-{}", i);
        description.portals.push_back({
            .a = {.id = std::format("hall-{}", i)},
            .b = {.id = std::format("raised-{}", i)},
        });
    }
    description.sectors = {std::move(hall), std::move(raised)};
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The great hall with a dais in its middle, a square from (3, 3) to (5, 5) with its floor at a
// height.
inline wulfhilde::Map dais_map(float floor) {
    return raised_map(floor, {
                                 {.x = 3.0F, .y = 3.0F},
                                 {.x = 3.0F, .y = 5.0F},
                                 {.x = 5.0F, .y = 5.0F},
                                 {.x = 5.0F, .y = 3.0F},
                             });
}
