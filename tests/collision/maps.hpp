// Maps built in code, and a body, for collision's tests to move through them.
#pragma once

#include <wulfhilde/collision/body_shape.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <initializer_list>
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
