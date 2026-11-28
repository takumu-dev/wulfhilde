// Each rule a map's description must keep, broken once in a valid description.
#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/exposure.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using wulfhilde::AmbientLight;
using wulfhilde::MapDescription;
using wulfhilde::MapProblemCode;
using wulfhilde::SkyDescription;
using wulfhilde::SurfaceDescription;

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
    description.sectors.at(0).loops.at(0).at(2).upper = {.material = "test/Upper"};
    description.sectors.at(0).loops.at(0).at(2).lower = {.material = "test/lower strip"};
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::invalid_asset_path, "/sectors/0/ceiling/material"},
              {MapProblemCode::invalid_asset_path, "/sectors/0/loops/0/1/surface/material"},
              {MapProblemCode::invalid_asset_path, "/sectors/0/loops/0/2/upper/material"},
              {MapProblemCode::invalid_asset_path, "/sectors/0/loops/0/2/lower/material"},
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

TEST_CASE("a sky's backdrop that isn't an asset path is a problem at it", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).floor.sky = SkyDescription{.backdrop = "Test/Abyss.png"};
    description.sectors.at(0).ceiling.sky = SkyDescription{.backdrop = "../day"};

    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::invalid_asset_path, "/sectors/0/floor/sky/backdrop"},
              {MapProblemCode::invalid_asset_path, "/sectors/0/ceiling/sky/backdrop"},
          });
}

TEST_CASE("a sky needs no material, and whatever material its floor or ceiling names is ignored",
          "[world]") {
    auto description = hall_map();
    description.sectors.at(0).floor = {.height = 0.0F, .sky = SkyDescription{.backdrop = "abyss"}};
    description.sectors.at(0).ceiling.material = "Not An Asset Path";
    description.sectors.at(0).ceiling.sky = SkyDescription{.backdrop = "day"};

    CHECK(problems_in(description).empty());
}

TEST_CASE("a sky's nits are a problem below 0, or if they aren't a finite number", "[world]") {
    for (const float nits :
         {-0.5F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        CAPTURE(nits);
        auto description = hall_map();
        description.sectors.at(0).ceiling.sky = SkyDescription{.backdrop = "day", .nits = nits};

        CHECK(problems_in(description) ==
              std::vector<Problem>{
                  {MapProblemCode::light_level_out_of_range, "/sectors/0/ceiling/sky/nits"},
              });
    }
    auto description = hall_map();
    description.sectors.at(0).floor.sky = SkyDescription{.backdrop = "abyss", .nits = -2.0F};
    CHECK(only_problem_in(description).message ==
          "-2 nits isn't a light level, which is 0 or more");
}

TEST_CASE("a sky may be of no nits at all", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).ceiling.sky = SkyDescription{.backdrop = "night", .nits = 0.0F};

    CHECK(problems_in(description).empty());
}

TEST_CASE("a surface's nits are a problem below 0, or if they aren't a finite number",
          "[world]") {
    for (const float nits :
         {-0.5F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        CAPTURE(nits);
        auto description = doorway_map();
        description.sectors.at(0).floor.nits = nits;
        auto& door = description.sectors.at(0).loops.at(0).at(3);
        door.surface.nits = nits;
        door.upper = SurfaceDescription{.material = "test/lintel", .nits = nits};
        door.lower = SurfaceDescription{.material = "test/sill", .nits = nits};

        CHECK(problems_in(description) ==
              std::vector<Problem>{
                  {MapProblemCode::light_level_out_of_range, "/sectors/0/floor/nits"},
                  {MapProblemCode::light_level_out_of_range,
                   "/sectors/0/loops/0/3/surface/nits"},
                  {MapProblemCode::light_level_out_of_range, "/sectors/0/loops/0/3/upper/nits"},
                  {MapProblemCode::light_level_out_of_range, "/sectors/0/loops/0/3/lower/nits"},
              });
    }
    auto description = hall_map();
    description.sectors.at(0).ceiling.nits = -3.0F;
    CHECK(only_problem_in(description).message ==
          "-3 nits isn't a light level, which is 0 or more");
}

TEST_CASE("a surface may be of no nits at all", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.at(0).at(0).surface.nits = 0.0F;

    CHECK(problems_in(description).empty());
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

TEST_CASE("an ambient light's colour is a problem at each component outside 0 to 1", "[world]") {
    auto description = hall_map();
    description.ambient = AmbientLight{.color = {.r = 1.5F, .g = 0.5F, .b = -0.25F}, .lux = 10.0F};
    description.sectors.at(0).ambient = AmbientLight{
        .color = {.r = 0.0F, .g = std::numeric_limits<float>::quiet_NaN(), .b = 1.0F},
        .lux = 10.0F,
    };

    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::color_out_of_range, "/ambient/color/0"},
              {MapProblemCode::color_out_of_range, "/ambient/color/2"},
              {MapProblemCode::color_out_of_range, "/sectors/0/ambient/color/1"},
          });
    description.sectors.at(0).ambient.reset();
    description.ambient->color.b = 1.0F;
    CHECK(only_problem_in(description).message ==
          "1.5 is outside a colour component's range, 0 to 1");
}

TEST_CASE("an ambient light's lux is a problem below 0, or if it isn't a finite number",
          "[world]") {
    for (const float lux :
         {-0.5F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        CAPTURE(lux);
        auto description = hall_map();
        description.sectors.at(0).ambient = AmbientLight{.color = {.r = 1.0F}, .lux = lux};

        CHECK(problems_in(description) ==
              std::vector<Problem>{
                  {MapProblemCode::light_level_out_of_range, "/sectors/0/ambient/lux"},
              });
    }
    auto description = hall_map();
    description.ambient = AmbientLight{.color = {.r = 1.0F}, .lux = -0.5F};
    CHECK(only_problem_in(description).message ==
          "-0.5 lux isn't a light level, which is 0 or more");
}

TEST_CASE("an ambient light may be black, white, and of no lux at all", "[world]") {
    auto description = hall_map();
    description.ambient = AmbientLight{.color = {.r = 1.0F, .g = 1.0F, .b = 1.0F}, .lux = 0.0F};
    description.sectors.at(0).ambient = AmbientLight{.lux = 1e5F};

    CHECK(problems_in(description).empty());
}

TEST_CASE("an exposure that isn't a finite number is a problem at it", "[world]") {
    for (const float bad : {
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity(),
             std::numeric_limits<float>::quiet_NaN(),
         }) {
        CAPTURE(bad);
        auto description = hall_map();
        description.exposure = {.min = bad, .max = 16.0F, .compensation = bad};

        CHECK(problems_in(description) ==
              std::vector<Problem>{
                  {MapProblemCode::exposure_out_of_range, "/exposure/min"},
                  {MapProblemCode::exposure_out_of_range, "/exposure/compensation"},
              });
    }
    auto description = hall_map();
    description.exposure.max = std::numeric_limits<float>::infinity();
    CHECK(only_problem_in(description).message ==
          "inf isn't an exposure, which is a finite number");
}

TEST_CASE("an exposure range whose minimum is above its maximum is a problem at its minimum",
          "[world]") {
    auto description = hall_map();
    description.exposure = {.min = 9.0F, .max = 4.0F};

    const auto problem = only_problem_in(description);
    CHECK(problem.code == MapProblemCode::exposure_out_of_range);
    CHECK(problem.pointer == "/exposure/min");
    CHECK(problem.message ==
          "the exposure range's minimum, 9 EV100, is above its maximum, 4 EV100");
}

TEST_CASE("an exposure range may be a single exposure, below 0 EV100, and compensated either way",
          "[world]") {
    auto description = hall_map();
    description.exposure = {.min = -3.0F, .max = -3.0F, .compensation = -2.0F};
    CHECK(problems_in(description).empty());

    description.exposure = {.min = -6.0F, .max = 20.0F, .compensation = 4.5F};
    CHECK(problems_in(description).empty());
}

TEST_CASE("the hall and the gallery joined by a doorway are a valid map", "[world]") {
    CHECK(problems_in(doorway_map()).empty());
}

TEST_CASE("a wall with the id of an earlier one is a problem at its id", "[world]") {
    auto description = doorway_map();
    description.sectors.at(1).loops.at(0).at(4).id = "hall-door";
    description.sectors.at(1).loops.at(0).at(5).id = "hall-door";

    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::duplicate_wall_id, "/sectors/1/loops/0/4/id"},
              {MapProblemCode::duplicate_wall_id, "/sectors/1/loops/0/5/id"},
          });
    description.sectors.at(1).loops.at(0).at(5).id.reset();
    CHECK(only_problem_in(description).message ==
          R"(wall /sectors/0/loops/0/3 already has the id "hall-door")");
}

TEST_CASE("a portal's side that names no wall, or no sector, is a problem at the name", "[world]") {
    auto description = doorway_map();
    description.portals.at(0).b.id = "cellar-door";

    const auto problem = only_problem_in(description);
    CHECK(problem.code == MapProblemCode::unknown_wall);
    CHECK(problem.pointer == "/portals/0/b/wall");
    CHECK(problem.message == R"(no wall has the id "cellar-door")");

    description = doorway_map();
    description.portals.push_back({
        .a = {.kind = wulfhilde::PortalSideKind::floor, .id = "cellar"},
        .b = {.kind = wulfhilde::PortalSideKind::ceiling, .id = "hall"},
    });
    CHECK(problems_in(description) == std::vector<Problem>{
                                          {MapProblemCode::unknown_sector, "/portals/1/a/floor"},
                                          {MapProblemCode::unsupported_portal, "/portals/1"},
                                      });
}

TEST_CASE("a portal that links a wall to a floor or a ceiling is a problem at the portal",
          "[world]") {
    for (const auto kind : {wulfhilde::PortalSideKind::floor, wulfhilde::PortalSideKind::ceiling}) {
        auto description = doorway_map();
        description.portals.at(0).b = {.kind = kind, .id = "gallery"};
        CAPTURE(std::to_underlying(kind));

        CHECK(problems_in(description) ==
              std::vector<Problem>{{MapProblemCode::unlike_portal_sides, "/portals/0"}});
    }
    auto description = doorway_map();
    description.portals.at(0).a = {.kind = wulfhilde::PortalSideKind::ceiling, .id = "hall"};
    CHECK(only_problem_in(description).message ==
          "the portal links a ceiling to a wall, but walls link only to walls");
}

TEST_CASE("a portal between floors and ceilings is a problem at the portal, until they are "
          "supported",
          "[world]") {
    auto description = doorway_map();
    description.portals.push_back({
        .a = {.kind = wulfhilde::PortalSideKind::ceiling, .id = "hall"},
        .b = {.kind = wulfhilde::PortalSideKind::floor, .id = "gallery"},
    });

    const auto problem = only_problem_in(description);
    CHECK(problem.code == MapProblemCode::unsupported_portal);
    CHECK(problem.pointer == "/portals/1");
    CHECK(problem.message == "the portal links a ceiling to a floor, which this version of the "
                             "engine doesn't support");
}

TEST_CASE("a wall portal whose walls don't run between the same corners in opposite directions "
          "is a problem at the portal",
          "[world]") {
    struct Case {
        std::string name;
        std::function<void(MapDescription&)> change;
    };

    const std::vector<Case> cases{
        {
            .name = "a side shorter than the other",
            .change =
                [](MapDescription& d) {
                    d.sectors.at(1).loops.at(0).at(2).start = {.x = 2.5F, .y = 4.0F};
                },
        },
        {
            .name = "a corner a millimetre out",
            .change =
                [](MapDescription& d) {
                    d.sectors.at(1).loops.at(0).at(1).start = {.x = 1.001F, .y = 4.0F};
                },
        },
        {
            .name = "both walls running the same way",
            .change = [](MapDescription& d) { d.portals.at(0).b.id = "hall-door"; },
        },
    };
    for (const auto& [name, change] : cases) {
        auto description = doorway_map();
        change(description);
        CAPTURE(name);

        CHECK(problems_in(description) ==
              std::vector<Problem>{{MapProblemCode::mismatched_portal_walls, "/portals/0"}});
    }
    auto description = doorway_map();
    description.sectors.at(1).loops.at(0).at(2).start = {.x = 2.5F, .y = 4.0F};
    CHECK(only_problem_in(description).message ==
          "the portal's walls must run between the same corners in opposite directions, but "
          "/sectors/0/loops/0/3 runs from (3, 4) to (1, 4), and /sectors/1/loops/0/1 from (1, 4) "
          "to (2.5, 4)");
}

TEST_CASE("a wall portal isn't checked for its corners where a wall's corner is beyond the "
          "map's bounds",
          "[world]") {
    auto description = doorway_map();
    description.sectors.at(1).loops.at(0).at(2).start.x = 5000.0F;

    const auto problems = problems_in(description);
    CHECK(std::ranges::find(problems, MapProblemCode::mismatched_portal_walls, &Problem::code) ==
          problems.end());
}

TEST_CASE("a wall that is a side of an earlier portal is a problem at the later side", "[world]") {
    auto description = doorway_map();
    description.sectors.at(1).loops.at(0).at(0).id = "gallery-west";
    description.portals.push_back({.a = {.id = "gallery-door"}, .b = {.id = "gallery-west"}});

    // The second portal's walls don't meet in reverse either.
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::wall_in_two_portals, "/portals/1/a/wall"},
              {MapProblemCode::mismatched_portal_walls, "/portals/1"},
          });
    const auto map = wulfhilde::Map::create(description);
    REQUIRE(!map.has_value());
    CHECK(map.error().problems.at(0).message ==
          "the wall /sectors/1/loops/0/1 is already a side of /portals/0");
}

TEST_CASE("problems in portals come after the sectors' and before the start position's",
          "[world]") {
    auto description = doorway_map();
    description.sectors.at(1).ceiling.material = "test/Ceiling";
    description.portals.at(0).a.id = "front-door";
    description.start.sector = "yard";
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::invalid_asset_path, "/sectors/1/ceiling/material"},
              {MapProblemCode::unknown_wall, "/portals/0/a/wall"},
              {MapProblemCode::unknown_sector, "/start/sector"},
          });
}
