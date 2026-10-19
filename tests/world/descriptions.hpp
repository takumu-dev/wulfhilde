// Small, valid descriptions of maps for the world's tests to build on. A test of a rule breaks one
// thing in one of them, so the problem it expects is the only one there is.
#pragma once

#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>

#include <format>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

// A loop of walls starting at each point in turn, each wall's surface the test wall material.
inline std::vector<wulfhilde::WallDescription>
walls_through(std::initializer_list<wulfhilde::Vector2> points) {
    std::vector<wulfhilde::WallDescription> walls;
    for (const auto& point : points) {
        walls.push_back({.start = point, .surface = {.material = "test/wall"}});
    }
    return walls;
}

// A sector bounded by a loop of walls through the points, with its floor at 0 m and its ceiling at
// 3 m.
inline wulfhilde::SectorDescription
sector_through(std::string id, std::initializer_list<wulfhilde::Vector2> points) {
    return {
        .id = std::move(id),
        .floor = {.height = 0.0F, .material = "test/floor"},
        .ceiling = {.height = 3.0F, .material = "test/ceiling"},
        .loops = {walls_through(points)},
    };
}

// The hall: a square sector 4 m on a side from (0, 0) to (4, 4), with its floor at 0 m and its
// ceiling at 3 m, where the player starts at the centre of the floor, facing +X.
inline wulfhilde::MapDescription hall_map() {
    return {
        .sectors =
            {
                sector_through("hall",
                               {
                                   {.x = 0.0F, .y = 0.0F},
                                   {.x = 4.0F, .y = 0.0F},
                                   {.x = 4.0F, .y = 4.0F},
                                   {.x = 0.0F, .y = 4.0F},
                               }),
            },
        .start = {.sector = "hall", .point = {.x = 2.0F, .y = 2.0F, .z = 0.0F}},
    };
}

// The closet: a triangular sector beside the hall, from (10, 0) to (12, 1).
inline wulfhilde::SectorDescription closet_sector() {
    return sector_through("closet", {
                                        {.x = 10.0F, .y = 0.0F},
                                        {.x = 12.0F, .y = 0.0F},
                                        {.x = 12.0F, .y = 1.0F},
                                    });
}

// The hall as an L instead, which is concave: two arms 1 m wide and 4 m long along X and Y from
// the corner at the origin, where the player starts.
inline wulfhilde::MapDescription l_shaped_hall_map() {
    auto description = hall_map();
    description.sectors.at(0) = sector_through("hall", {
                                                           {.x = 0.0F, .y = 0.0F},
                                                           {.x = 4.0F, .y = 0.0F},
                                                           {.x = 4.0F, .y = 1.0F},
                                                           {.x = 1.0F, .y = 1.0F},
                                                           {.x = 1.0F, .y = 4.0F},
                                                           {.x = 0.0F, .y = 4.0F},
                                                       });
    description.start.point = {.x = 0.5F, .y = 0.5F};
    return description;
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

// A problem as the tests of the rules check it: what kind it is and where it lies. The message is
// for people to read, and fewer tests pin it.
struct Problem {
    wulfhilde::MapProblemCode code{};
    std::string pointer{};

    bool operator==(const Problem&) const = default;
};

template <> struct Catch::StringMaker<Problem> {
    static std::string convert(const Problem& problem) {
        return std::format("code {} at {}", std::to_underlying(problem.code), problem.pointer);
    }
};

// The problems Map::create finds in a description, in its order, or none if it makes a map.
inline std::vector<Problem> problems_in(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    std::vector<Problem> problems;
    if (!map) {
        for (const auto& problem : map.error().problems) {
            problems.push_back({.code = problem.code, .pointer = problem.pointer});
        }
    }
    return problems;
}

// The one problem Map::create finds in a description that must have exactly one.
inline wulfhilde::MapProblem only_problem_in(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    REQUIRE(!map.has_value());
    REQUIRE(map.error().problems.size() == 1);
    return map.error().problems.front();
}
