// Each rule a map's description must keep, broken once in a valid description.
#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <vector>

using wulfhilde::MapDescription;
using wulfhilde::MapProblemCode;

TEST_CASE("the hall the rules' tests break is a valid map", "[world]") {
    CHECK(problems_in(hall_map()).empty());
}

TEST_CASE("a sector with the id of an earlier one is a problem at its id", "[world]") {
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    description.sectors.push_back(description.sectors.at(0));
    description.sectors.push_back(description.sectors.at(1));

    CHECK(problems_in(description) == std::vector<Problem>{
                                          {MapProblemCode::duplicate_sector_id, "/sectors/2/id"},
                                          {MapProblemCode::duplicate_sector_id, "/sectors/3/id"},
                                      });
    CHECK(only_problem_in([] {
              auto twice = hall_map();
              twice.sectors.push_back(twice.sectors.at(0));
              return twice;
          }())
              .message == R"(sector /sectors/0 already has the id "hall")");
}

TEST_CASE("a start position in a sector that doesn't exist is a problem at its sector", "[world]") {
    auto description = hall_map();
    description.start.sector = "yard";

    const auto problem = only_problem_in(description);
    CHECK(problem.code == MapProblemCode::unknown_sector);
    CHECK(problem.pointer == "/start/sector");
    CHECK(problem.message == R"(no sector has the id "yard")");
}

TEST_CASE("a coordinate beyond 4,096 m of the origin, or not a number, is a problem at it",
          "[world]") {
    struct Case {
        std::string pointer;
        std::function<void(MapDescription&)> change;
    };

    const std::vector<Case> cases{
        {
            .pointer = "/sectors/0/loops/0/2/start/0",
            .change =
                [](MapDescription& d) { d.sectors.at(0).loops.at(0).at(2).start.x = 4096.5F; },
        },
        {
            .pointer = "/sectors/0/loops/0/2/start/1",
            .change =
                [](MapDescription& d) { d.sectors.at(0).loops.at(0).at(2).start.y = 5000.0F; },
        },
        {
            .pointer = "/sectors/0/loops/0/0/start/0",
            .change =
                [](MapDescription& d) { d.sectors.at(0).loops.at(0).at(0).start.x = -5000.0F; },
        },
        {
            .pointer = "/sectors/0/loops/0/1/start/1",
            .change =
                [](MapDescription& d) {
                    d.sectors.at(0).loops.at(0).at(1).start.y =
                        std::numeric_limits<float>::quiet_NaN();
                },
        },
        {
            .pointer = "/sectors/0/floor/height",
            .change = [](MapDescription& d) { d.sectors.at(0).floor.height = -4097.0F; },
        },
        {
            .pointer = "/sectors/0/ceiling/height",
            .change =
                [](MapDescription& d) {
                    d.sectors.at(0).ceiling.height = std::numeric_limits<float>::infinity();
                },
        },
        {
            .pointer = "/start/point/0",
            .change = [](MapDescription& d) { d.start.point.x = 4100.0F; },
        },
        {
            .pointer = "/start/point/1",
            .change = [](MapDescription& d) { d.start.point.y = -4100.0F; },
        },
        {
            .pointer = "/start/point/2",
            .change = [](MapDescription& d) { d.start.point.z = 4100.0F; },
        },
    };
    for (const auto& [pointer, change] : cases) {
        auto description = hall_map();
        change(description);
        CAPTURE(pointer);
        CHECK(problems_in(description) ==
              std::vector<Problem>{{MapProblemCode::coordinate_out_of_range, pointer}});
    }

    auto description = hall_map();
    description.sectors.at(0).floor.height = 5000.0F;
    CHECK(only_problem_in(description).message ==
          "5000 is beyond the map's bounds, 4,096 m either side of the origin");
}

TEST_CASE("coordinates 4,096 m from the origin are within the map", "[world]") {
    auto description = hall_map();
    description.sectors.at(0) = sector_through("hall", {
                                                           {.x = -4096.0F, .y = -4096.0F},
                                                           {.x = 4096.0F, .y = -4096.0F},
                                                           {.x = 4096.0F, .y = 4096.0F},
                                                           {.x = -4096.0F, .y = 4096.0F},
                                                       });
    description.sectors.at(0).floor.height = -4096.0F;
    description.sectors.at(0).ceiling.height = 4096.0F;
    description.start.point = {.x = 4096.0F, .y = -4096.0F, .z = 4096.0F};
    CHECK(problems_in(description).empty());
}

TEST_CASE("a material that isn't an asset path is a problem at it", "[world]") {
    for (const auto* path : {
             "",
             "/test/floor",
             "test/floor/",
             "test//floor",
             "Test/floor",
             "test/floor.png",
             "../floor",
             "./floor",
             R"(test\floor)",
             "test/stone floor",
             "test/b\xC3\xBChne",
         }) {
        auto description = hall_map();
        description.sectors.at(0).floor.material = path;
        CAPTURE(path);
        CHECK(problems_in(description) == std::vector<Problem>{{MapProblemCode::invalid_asset_path,
                                                                "/sectors/0/floor/material"}});
    }

    auto description = hall_map();
    description.sectors.at(0).ceiling.material = "Test/Ceiling.png";
    description.sectors.at(0).loops.at(0).at(1).surface.material = "test/wall.png";
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::invalid_asset_path, "/sectors/0/ceiling/material"},
              {MapProblemCode::invalid_asset_path, "/sectors/0/loops/0/1/surface/material"},
          });

    description = hall_map();
    description.sectors.at(0).floor.material = "Test/Floor.png";
    CHECK(only_problem_in(description).message ==
          R"("Test/Floor.png" isn't an asset path, which is segments of a-z, 0-9, _ and - )"
          "separated by / and without an extension");
}

TEST_CASE("an asset path is segments of lowercase letters, digits, underscores and hyphens",
          "[world]") {
    for (const auto* path : {"floor", "test/floor", "stone_2/rough-floor", "a/b/c/d/e"}) {
        auto description = hall_map();
        description.sectors.at(0).floor.material = path;
        CAPTURE(path);
        CHECK(problems_in(description).empty());
    }
}

TEST_CASE("a sector without a loop is a problem at its loops", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.clear();
    CHECK(problems_in(description) ==
          std::vector<Problem>{{MapProblemCode::missing_outer_loop, "/sectors/0/loops"}});
}

TEST_CASE("a sector with an inner loop is a problem at the inner loop, until they are supported",
          "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.push_back(
        walls_through({{.x = 1.0F, .y = 1.0F}, {.x = 1.0F, .y = 2.0F}, {.x = 2.0F, .y = 2.0F}}));
    description.sectors.at(0).loops.push_back(description.sectors.at(0).loops.at(1));
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::unsupported_inner_loop, "/sectors/0/loops/1"},
              {MapProblemCode::unsupported_inner_loop, "/sectors/0/loops/2"},
          });
}

TEST_CASE("a loop of fewer than three walls is a problem at the loop", "[world]") {
    for (const auto& walls : {
             walls_through({}),
             walls_through({{.x = 0.0F, .y = 0.0F}}),
             walls_through({{.x = 0.0F, .y = 0.0F}, {.x = 4.0F, .y = 0.0F}}),
         }) {
        auto description = hall_map();
        description.sectors.at(0).loops.at(0) = walls;
        CAPTURE(walls.size());
        CHECK(problems_in(description) ==
              std::vector<Problem>{{MapProblemCode::too_few_walls, "/sectors/0/loops/0"}});
    }
}

TEST_CASE("an outer loop that runs clockwise seen from above is a problem at the loop", "[world]") {
    auto description = hall_map();
    std::ranges::reverse(description.sectors.at(0).loops.at(0));
    CHECK(problems_in(description) ==
          std::vector<Problem>{{MapProblemCode::wrong_winding, "/sectors/0/loops/0"}});

    // The L, which is concave, run clockwise.
    description = l_shaped_hall_map();
    std::ranges::reverse(description.sectors.at(0).loops.at(0));
    CHECK(problems_in(description) ==
          std::vector<Problem>{{MapProblemCode::wrong_winding, "/sectors/0/loops/0"}});
}

TEST_CASE("a concave sector is a valid one", "[world]") {
    CHECK(problems_in(l_shaped_hall_map()).empty());
}

TEST_CASE("a loop whose walls cross or touch is a problem at the later wall of each pair",
          "[world]") {
    auto description = hall_map();
    // A bow tie, whose first and third walls cross at (2, 2).
    description.sectors.at(0).loops.at(0) = walls_through({
        {.x = 0.0F, .y = 0.0F},
        {.x = 4.0F, .y = 4.0F},
        {.x = 4.0F, .y = 0.0F},
        {.x = 0.0F, .y = 4.0F},
    });
    CHECK(problems_in(description) ==
          std::vector<Problem>{{MapProblemCode::self_intersecting_loop, "/sectors/0/loops/0/2"}});
    CHECK(only_problem_in(description).message ==
          "the wall crosses or touches /sectors/0/loops/0/0, another wall of its loop");

    // The fifth point lies on the first wall, which the walls on either side of it touch there.
    description.sectors.at(0).loops.at(0) = walls_through({
        {.x = 0.0F, .y = 0.0F},
        {.x = 6.0F, .y = 0.0F},
        {.x = 6.0F, .y = 4.0F},
        {.x = 4.0F, .y = 4.0F},
        {.x = 3.0F, .y = 0.0F},
        {.x = 0.0F, .y = 4.0F},
    });
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::self_intersecting_loop, "/sectors/0/loops/0/3"},
              {MapProblemCode::self_intersecting_loop, "/sectors/0/loops/0/4"},
          });

    // Three points in a line, so the second and third walls fold back over the first.
    description.sectors.at(0).loops.at(0) =
        walls_through({{.x = 0.0F, .y = 0.0F}, {.x = 4.0F, .y = 0.0F}, {.x = 2.0F, .y = 0.0F}});
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::self_intersecting_loop, "/sectors/0/loops/0/1"},
              {MapProblemCode::self_intersecting_loop, "/sectors/0/loops/0/2"},
          });
}

TEST_CASE("walls that carry on in a straight line are a valid loop", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.at(0) = walls_through({
        {.x = 0.0F, .y = 0.0F},
        {.x = 2.0F, .y = 0.0F},
        {.x = 4.0F, .y = 0.0F},
        {.x = 4.0F, .y = 4.0F},
        {.x = 0.0F, .y = 4.0F},
    });
    CHECK(problems_in(description).empty());
}

TEST_CASE("a wall that starts where the next one does is a problem at the wall", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.at(0) = walls_through({
        {.x = 0.0F, .y = 0.0F},
        {.x = 4.0F, .y = 0.0F},
        {.x = 4.0F, .y = 0.0F},
        {.x = 4.0F, .y = 4.0F},
        {.x = 0.0F, .y = 4.0F},
        {.x = 0.0F, .y = 0.0F},
    });
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::zero_length_wall, "/sectors/0/loops/0/1"},
              {MapProblemCode::zero_length_wall, "/sectors/0/loops/0/5"},
          });
}

TEST_CASE("a loop with a point beyond the map's bounds isn't also a problem for its shape",
          "[world]") {
    auto description = hall_map();
    // Within the bounds, this would make the second wall cross the fourth.
    description.sectors.at(0).loops.at(0).at(2).start.x = -5000.0F;
    CHECK(problems_in(description) == std::vector<Problem>{{MapProblemCode::coordinate_out_of_range,
                                                            "/sectors/0/loops/0/2/start/0"}});
}

TEST_CASE("a loop of too few walls is a problem even with a point beyond the map's bounds",
          "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.at(0) =
        walls_through({{.x = 0.0F, .y = 0.0F}, {.x = 5000.0F, .y = 0.0F}});
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::coordinate_out_of_range, "/sectors/0/loops/0/1/start/0"},
              {MapProblemCode::too_few_walls, "/sectors/0/loops/0"},
          });
}

TEST_CASE("every problem in a description is found at once, sector by sector", "[world]") {
    auto description = hall_map();
    description.sectors.push_back(closet_sector());
    std::ranges::reverse(description.sectors.at(1).loops.at(0));
    description.sectors.push_back(description.sectors.at(0));
    description.sectors.at(0).ceiling.material = "test/Ceiling";
    description.sectors.at(0).loops.at(0).at(3).start.y = 9000.0F;
    description.start = {.sector = "yard", .point = {.z = -5000.0F}};
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::invalid_asset_path, "/sectors/0/ceiling/material"},
              {MapProblemCode::coordinate_out_of_range, "/sectors/0/loops/0/3/start/1"},
              {MapProblemCode::wrong_winding, "/sectors/1/loops/0"},
              {MapProblemCode::duplicate_sector_id, "/sectors/2/id"},
              {MapProblemCode::unknown_sector, "/start/sector"},
              {MapProblemCode::coordinate_out_of_range, "/start/point/2"},
          });
}
