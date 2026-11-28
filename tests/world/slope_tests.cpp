// Sloped floors and ceilings, hinged on a wall of their sector: their heights, the openings beside
// them, the traces that meet them, and the rules a slope keeps.
#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/opening.hpp>
#include <wulfhilde/world/sector.hpp>
#include <wulfhilde/world/surface_ref.hpp>
#include <wulfhilde/world/wall_face.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using wulfhilde::Location;
using wulfhilde::MapProblemCode;
using wulfhilde::Opening;
using wulfhilde::SectorCeiling;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
using wulfhilde::Slope;
using wulfhilde::SlopeDescription;
using wulfhilde::Vector3;
using wulfhilde::WallId;

namespace {

constexpr SectorId hall{0};

// How near a height or a point worked out through a slope is to its exact value.
constexpr float tolerance = 1e-5F;

// The hall with its floor sloped up from its south wall, the wall from (0, 0) to (4, 0), at a
// gradient.
wulfhilde::MapDescription hall_sloped_from_south(float gradient) {
    auto description = hall_map();
    auto& hall_sector = description.sectors.at(0);
    hall_sector.loops.at(0).at(0).id = "hall-south";
    hall_sector.floor.slope = SlopeDescription{.hinge = "hall-south", .gradient = gradient};
    return description;
}

// The doorway map with the gallery's floor sloped up eastward from its west wall, the wall from
// (0, 8) to (0, 4), from a height there at a gradient, while the hall's floor stays at 0 m. The
// hall's door, wall #3, runs west from (3, 4) to (1, 4), and the gallery's, wall #7, back east.
wulfhilde::MapDescription doorway_with_sloped_gallery(float height, float gradient) {
    auto description = doorway_map();
    auto& gallery = description.sectors.at(1);
    gallery.loops.at(0).at(5).id = "gallery-west";
    gallery.floor.height = height;
    gallery.floor.slope = SlopeDescription{.hinge = "gallery-west", .gradient = gradient};
    return description;
}

// Tells whether two polygons have the same corners in the same order, starting anywhere.
bool same_polygon(const std::vector<Vector3>& actual, const std::vector<Vector3>& expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    for (std::size_t first = 0; first < actual.size(); ++first) {
        bool same = true;
        for (std::size_t i = 0; i < expected.size() && same; ++i) {
            same = approx_equal(actual.at((first + i) % actual.size()), expected.at(i), tolerance);
        }
        if (same) {
            return true;
        }
    }
    return expected.empty();
}

} // namespace

TEST_CASE("a sector keeps the slopes of its floor and its ceiling, each hinged on a wall by its ID",
          "[world]") {
    auto description = hall_sloped_from_south(0.5F);
    description.sectors.at(0).loops.at(0).at(1).id = "hall-east";
    description.sectors.at(0).ceiling.slope =
        SlopeDescription{.hinge = "hall-east", .gradient = -0.25F};
    const auto map = make_map(description);

    const auto& sector = map.sector(hall);
    CHECK(sector.floor.slope == Slope{.hinge = WallId{0}, .gradient = 0.5F});
    CHECK(sector.ceiling.slope == Slope{.hinge = WallId{1}, .gradient = -0.25F});
    CHECK(map.sector(hall).floor.height == 0.0F);
}

TEST_CASE("a flat floor or ceiling has no slope", "[world]") {
    const auto map = make_map(hall_map());
    CHECK(map.sector(hall).floor.slope == std::nullopt);
    CHECK(map.sector(hall).ceiling.slope == std::nullopt);
}

TEST_CASE("a sloped floor lies at its height along its hinge, and rises square to it at its "
          "gradient, across the sector and beyond it",
          "[world]") {
    const auto map = make_map(hall_sloped_from_south(0.5F));
    for (const auto& [point, height] : {
             std::pair{Vector3{.x = 2.0F, .y = 0.0F, .z = 5.0F}, 0.0F},
             std::pair{Vector3{.x = 0.0F, .y = 2.0F, .z = 0.0F}, 1.0F},
             std::pair{Vector3{.x = 3.0F, .y = 4.0F, .z = 0.0F}, 2.0F},
             std::pair{Vector3{.x = 9.0F, .y = -2.0F, .z = 0.0F}, -1.0F},
         }) {
        const Location location{.sector = hall, .point = point};
        CAPTURE(location);
        CHECK(std::abs(map.floor_height(location) - height) < tolerance);
        CHECK(map.ceiling_height(location) == 3.0F);
    }
}

TEST_CASE("a ceiling sloped by a negative gradient falls from its hinge, which may be any wall of "
          "its sector",
          "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.at(0).at(1).id = "hall-east";
    description.sectors.at(0).ceiling.slope =
        SlopeDescription{.hinge = "hall-east", .gradient = -0.25F};
    const auto map = make_map(description);

    CHECK(std::abs(map.ceiling_height({.sector = hall, .point = {.x = 4.0F, .y = 1.0F}}) - 3.0F) <
          tolerance);
    CHECK(std::abs(map.ceiling_height({.sector = hall, .point = {.x = 2.0F, .y = 3.0F}}) - 2.5F) <
          tolerance);
    CHECK(std::abs(map.ceiling_height({.sector = hall, .point = {.x = 0.0F, .y = 0.0F}}) - 2.0F) <
          tolerance);
    CHECK(map.floor_height({.sector = hall, .point = {.x = 0.0F, .y = 0.0F}}) == 0.0F);
}

TEST_CASE("a slope hinged on a diagonal wall rises square to the wall", "[world]") {
    auto description = hall_map();
    description.sectors.at(0) = sector_through("closet", {
                                                             {.x = 10.0F, .y = 0.0F},
                                                             {.x = 12.0F, .y = 0.0F},
                                                             {.x = 12.0F, .y = 1.0F},
                                                         });
    description.sectors.at(0).loops.at(0).at(2).id = "closet-diagonal";
    // The corner at (12, 0) lies 2 / sqrt(5) m from the diagonal from (12, 1) to (10, 0).
    description.sectors.at(0).floor.slope =
        SlopeDescription{.hinge = "closet-diagonal", .gradient = std::sqrt(5.0F)};
    description.start = {.sector = "closet", .point = {.x = 11.5F, .y = 0.25F}};
    const auto map = make_map(description);

    CHECK(std::abs(map.floor_height({.sector = hall, .point = {.x = 12.0F, .y = 0.0F}}) - 2.0F) <
          tolerance);
    CHECK(std::abs(map.floor_height({.sector = hall, .point = {.x = 11.0F, .y = 0.5F}})) <
          tolerance);
    CHECK(std::abs(map.floor_height({.sector = hall, .point = {.x = 11.0F, .y = 0.0F}}) - 1.0F) <
          tolerance);
}

TEST_CASE("a point is in a sloped sector from the floor to the ceiling above and below it",
          "[world]") {
    const auto map = make_map(hall_sloped_from_south(0.5F));
    CHECK(map.contains({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}}));
    CHECK(map.contains({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 3.0F}}));
    CHECK(!map.contains({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 0.99F}}));
    CHECK(map.contains({.sector = hall, .point = {.x = 2.0F, .y = 0.5F, .z = 0.25F}}));
    CHECK(!map.contains({.sector = hall, .point = {.x = 2.0F, .y = 3.5F, .z = 1.5F}}));
}

TEST_CASE("a floor faces up into its sector, and a ceiling down, each tilted by its slope",
          "[world]") {
    auto description = hall_sloped_from_south(0.5F);
    description.sectors.at(0).loops.at(0).at(1).id = "hall-east";
    description.sectors.at(0).ceiling.slope =
        SlopeDescription{.hinge = "hall-east", .gradient = -0.25F};
    const auto sloped = make_map(description);
    const auto flat = make_map(hall_map());

    CHECK(flat.floor_normal(hall) == Vector3{.z = 1.0F});
    CHECK(flat.ceiling_normal(hall) == Vector3{.z = -1.0F});
    CHECK(approx_equal(sloped.floor_normal(hall), normalize(Vector3{.y = -0.5F, .z = 1.0F}),
                       tolerance));
    CHECK(approx_equal(sloped.ceiling_normal(hall), normalize(Vector3{.x = 0.25F, .z = -1.0F}),
                       tolerance));
}

TEST_CASE("a trace moving up a sloped floor's rise meets the floor where it comes up to the trace",
          "[world]") {
    const auto map = make_map(hall_sloped_from_south(0.5F));
    const auto trace =
        map.trace({.sector = hall, .point = {.x = 2.0F, .y = 0.5F, .z = 1.0F}}, {.y = 3.0F});
    CHECK(approx_equal(trace.end.point, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, tolerance));
    CHECK(trace.surface == wulfhilde::SurfaceRef{SectorFloor{hall}});
}

TEST_CASE("a trace down onto a sloped floor meets it at its height below the trace", "[world]") {
    const auto map = make_map(hall_sloped_from_south(0.5F));
    const auto trace =
        map.trace({.sector = hall, .point = {.x = 1.0F, .y = 3.0F, .z = 2.5F}}, {.z = -3.0F});
    CHECK(approx_equal(trace.end.point, {.x = 1.0F, .y = 3.0F, .z = 1.5F}, tolerance));
    CHECK(trace.surface == wulfhilde::SurfaceRef{SectorFloor{hall}});
}

TEST_CASE("a trace above a sloped floor and along its rise isn't stopped by it", "[world]") {
    const auto map = make_map(hall_sloped_from_south(0.5F));
    const auto trace = map.trace({.sector = hall, .point = {.x = 2.0F, .y = 0.5F, .z = 1.0F}},
                                 {.y = 2.0F, .z = 1.0F});
    CHECK(approx_equal(trace.end.point, {.x = 2.0F, .y = 2.5F, .z = 2.0F}, tolerance));
    CHECK(trace.surface == std::nullopt);
}

TEST_CASE("a trace rising toward a sloped ceiling meets it where the ceiling comes down to the "
          "trace",
          "[world]") {
    auto description = hall_map();
    description.sectors.at(0).loops.at(0).at(1).id = "hall-east";
    description.sectors.at(0).ceiling.slope =
        SlopeDescription{.hinge = "hall-east", .gradient = -0.25F};
    const auto map = make_map(description);

    // The ceiling falls from 3 m at x = 4 to 2 m at x = 0, and the trace rises from 1 m at x = 2.
    const auto trace = map.trace({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}},
                                 {.x = -2.0F, .z = 1.5F});
    CHECK(approx_equal(trace.end.point, {.x = 0.5F, .y = 2.0F, .z = 2.125F}, tolerance));
    CHECK(trace.surface == wulfhilde::SurfaceRef{SectorCeiling{hall}});
}

TEST_CASE("a wall portal's opening at a point of the wall runs from the higher floor there to the "
          "lower ceiling",
          "[world]") {
    // The gallery's floor rises from 0.25 m at the door's west end to 0.75 m at its east end.
    const auto map = make_map(doorway_with_sloped_gallery(0.0F, 0.25F));

    CHECK(map.opening(WallId{3}, {.x = 2.0F, .y = 4.0F}) == Opening{.bottom = 0.5F, .top = 3.0F});
    CHECK(map.opening(WallId{3}, {.x = 3.0F, .y = 4.0F}) == Opening{.bottom = 0.75F, .top = 3.0F});
    CHECK(map.opening(WallId{7}, {.x = 1.0F, .y = 4.0F}) == Opening{.bottom = 0.25F, .top = 3.0F});
    // A point off the wall stands for the point of the wall nearest it, its ends included.
    CHECK(map.opening(WallId{3}, {.x = 2.0F, .y = 3.0F}) == Opening{.bottom = 0.5F, .top = 3.0F});
    CHECK(map.opening(WallId{3}, {.x = 5.0F, .y = 6.0F}) == Opening{.bottom = 0.75F, .top = 3.0F});
}

TEST_CASE(
    "a wall's face is its whole wall, between its sector's floor and ceiling, and a side of a "
    "portal's is parted into its strips and its opening",
    "[world]") {
    const auto map = make_map(doorway_with_sloped_gallery(0.0F, 0.25F));

    // Seen from the hall, the hall's door starts on the right, at (3, 4).
    const auto door = map.face(WallId{3});
    CHECK(same_polygon(door.whole, {
                                       {.x = 3.0F, .y = 4.0F, .z = 0.0F},
                                       {.x = 3.0F, .y = 4.0F, .z = 3.0F},
                                       {.x = 1.0F, .y = 4.0F, .z = 3.0F},
                                       {.x = 1.0F, .y = 4.0F, .z = 0.0F},
                                   }));
    CHECK(same_polygon(door.lower, {
                                       {.x = 3.0F, .y = 4.0F, .z = 0.0F},
                                       {.x = 3.0F, .y = 4.0F, .z = 0.75F},
                                       {.x = 1.0F, .y = 4.0F, .z = 0.25F},
                                       {.x = 1.0F, .y = 4.0F, .z = 0.0F},
                                   }));
    CHECK(same_polygon(door.opening, {
                                         {.x = 3.0F, .y = 4.0F, .z = 0.75F},
                                         {.x = 3.0F, .y = 4.0F, .z = 3.0F},
                                         {.x = 1.0F, .y = 4.0F, .z = 3.0F},
                                         {.x = 1.0F, .y = 4.0F, .z = 0.25F},
                                     }));
    CHECK(door.upper.empty());

    // The gallery's side has the higher floor, so no lower strip.
    const auto gallery_door = map.face(WallId{7});
    CHECK(gallery_door.lower.empty());
    CHECK(same_polygon(gallery_door.opening, {
                                                 {.x = 1.0F, .y = 4.0F, .z = 0.25F},
                                                 {.x = 1.0F, .y = 4.0F, .z = 3.0F},
                                                 {.x = 3.0F, .y = 4.0F, .z = 3.0F},
                                                 {.x = 3.0F, .y = 4.0F, .z = 0.75F},
                                             }));
}

TEST_CASE("where the floors on either side of a wall portal cross, each side's lower strip is a "
          "triangle up to the crossing",
          "[world]") {
    // The gallery's floor rises from -0.5 m at the door's west end to 0.5 m at its east end, so it
    // crosses the hall's at (2, 4).
    const auto map = make_map(doorway_with_sloped_gallery(-1.0F, 0.5F));

    const auto door = map.face(WallId{3});
    CHECK(same_polygon(door.lower, {
                                       {.x = 3.0F, .y = 4.0F, .z = 0.0F},
                                       {.x = 3.0F, .y = 4.0F, .z = 0.5F},
                                       {.x = 2.0F, .y = 4.0F, .z = 0.0F},
                                   }));
    CHECK(same_polygon(door.opening, {
                                         {.x = 3.0F, .y = 4.0F, .z = 0.5F},
                                         {.x = 3.0F, .y = 4.0F, .z = 3.0F},
                                         {.x = 1.0F, .y = 4.0F, .z = 3.0F},
                                         {.x = 1.0F, .y = 4.0F, .z = 0.0F},
                                         {.x = 2.0F, .y = 4.0F, .z = 0.0F},
                                     }));
    const auto gallery_door = map.face(WallId{7});
    CHECK(same_polygon(gallery_door.lower, {
                                               {.x = 1.0F, .y = 4.0F, .z = -0.5F},
                                               {.x = 1.0F, .y = 4.0F, .z = 0.0F},
                                               {.x = 2.0F, .y = 4.0F, .z = 0.0F},
                                           }));
}

TEST_CASE("a strip thinner than single precision can tell from no strip has no corners",
          "[world]") {
    // The hall's floor rises from 0.1 m at its south wall to just over 0.4 m at the door, by less
    // than a step of a float, so the gallery's floor, at 0.4 m, lies just below it there.
    auto description = doorway_map();
    auto& hall_sector = description.sectors.at(0);
    hall_sector.loops.at(0).at(0).id = "hall-south";
    hall_sector.floor.height = 0.1F;
    hall_sector.floor.slope = SlopeDescription{.hinge = "hall-south", .gradient = 0.075F};
    description.sectors.at(1).floor.height = 0.4F;
    const auto map = make_map(std::move(description));

    CHECK(map.face(WallId{3}).lower.empty());
    CHECK(map.face(WallId{7}).lower.empty());
    CHECK(same_polygon(map.face(WallId{7}).opening, {
                                                        {.x = 1.0F, .y = 4.0F, .z = 0.4F},
                                                        {.x = 1.0F, .y = 4.0F, .z = 3.0F},
                                                        {.x = 3.0F, .y = 4.0F, .z = 3.0F},
                                                        {.x = 3.0F, .y = 4.0F, .z = 0.4F},
                                                    }));
}

TEST_CASE("a solid wall's face is all whole wall, which follows the slope along its bottom",
          "[world]") {
    const auto map = make_map(hall_sloped_from_south(0.5F));

    // The east wall runs north from (4, 0), where the floor is at 0 m, to (4, 4), where it is at
    // 2 m.
    const auto east = map.face(WallId{1});
    CHECK(same_polygon(east.whole, {
                                       {.x = 4.0F, .y = 0.0F, .z = 0.0F},
                                       {.x = 4.0F, .y = 0.0F, .z = 3.0F},
                                       {.x = 4.0F, .y = 4.0F, .z = 3.0F},
                                       {.x = 4.0F, .y = 4.0F, .z = 2.0F},
                                   }));
    CHECK(east.lower.empty());
    CHECK(east.opening.empty());
    CHECK(east.upper.empty());
    CHECK(map.opening(WallId{1}, {.x = 4.0F, .y = 2.0F}) == std::nullopt);
}

TEST_CASE("a slope whose hinge names no wall is a problem at the hinge", "[world]") {
    auto description = hall_sloped_from_south(0.5F);
    description.sectors.at(0).floor.slope =
        SlopeDescription{.hinge = "hall-north", .gradient = 0.5F};
    const auto problem = only_problem_in(description);
    CHECK(problem.code == MapProblemCode::unknown_wall);
    CHECK(problem.pointer == "/sectors/0/floor/slope/hinge");
    CHECK(problem.message == R"(no wall has the id "hall-north")");
}

TEST_CASE("a slope hinged on a wall of another sector is a problem at the hinge", "[world]") {
    auto description = doorway_map();
    description.sectors.at(0).ceiling.slope =
        SlopeDescription{.hinge = "gallery-door", .gradient = 0.25F};
    const auto problem = only_problem_in(description);
    CHECK(problem.code == MapProblemCode::hinge_outside_sector);
    CHECK(problem.pointer == "/sectors/0/ceiling/slope/hinge");
    CHECK(problem.message == "the hinge, wall /sectors/1/loops/0/1, isn't a wall of this sector, "
                             "but of /sectors/1");
}

TEST_CASE("a slope that takes its floor or ceiling beyond the map's bounds at a corner of its "
          "sector, or a gradient that isn't a number, is a problem at the gradient",
          "[world]") {
    for (const auto gradient : {
             1100.0F,
             -1100.0F,
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
         }) {
        CAPTURE(gradient);
        CHECK(problems_in(hall_sloped_from_south(gradient)) ==
              std::vector<Problem>{
                  {MapProblemCode::coordinate_out_of_range, "/sectors/0/floor/slope/gradient"},
              });
    }
}

TEST_CASE("a slope may take its floor to the map's bounds, and may be hinged on a side of a portal",
          "[world]") {
    // The hall's north corners lie 4 m from its south wall.
    CHECK(problems_in(hall_sloped_from_south(1024.0F)).empty());

    auto description = doorway_map();
    description.sectors.at(1).floor.slope =
        SlopeDescription{.hinge = "gallery-door", .gradient = 0.25F};
    CHECK(problems_in(description).empty());
}

TEST_CASE(
    "a slope isn't checked against its sector's corners where its hinge or a corner is beyond "
    "the map's bounds",
    "[world]") {
    auto description = hall_sloped_from_south(1100.0F);
    description.sectors.at(0).loops.at(0).at(2).start.x = 5000.0F;
    CHECK(problems_in(description) ==
          std::vector<Problem>{
              {MapProblemCode::coordinate_out_of_range, "/sectors/0/loops/0/2/start/0"},
          });
}

TEST_CASE("a slope may be hinged on a wall of an inner loop, and lies on one plane all round the "
          "hole",
          "[world]") {
    // Hinged on the pillar's north wall, wall #5 from (1, 2) to (2, 2), which faces north.
    auto description = pillared_hall_map();
    description.sectors.at(0).loops.at(1).at(1).id = "pillar-north";
    description.sectors.at(0).floor.slope =
        SlopeDescription{.hinge = "pillar-north", .gradient = 0.5F};
    const auto map = make_map(description);

    CHECK(map.sector(hall).floor.slope == Slope{.hinge = WallId{5}, .gradient = 0.5F});
    CHECK(std::abs(map.floor_height({.sector = hall, .point = {.x = 1.5F, .y = 3.0F}}) - 0.5F) <
          tolerance);
    CHECK(std::abs(map.floor_height({.sector = hall, .point = {.x = 3.0F, .y = 1.0F}}) + 0.5F) <
          tolerance);
    CHECK(!map.contains({.sector = hall, .point = {.x = 1.5F, .y = 3.0F, .z = 0.4F}}));
    CHECK(map.contains({.sector = hall, .point = {.x = 1.5F, .y = 3.0F, .z = 0.6F}}));
    CHECK(!map.contains({.sector = hall, .point = {.x = 1.5F, .y = 1.5F, .z = 1.0F}}));
    // The pillar's west wall runs north from (1, 1), where the floor is at -0.5 m, to (1, 2).
    CHECK(same_polygon(map.face(WallId{4}).whole, {
                                                      {.x = 1.0F, .y = 1.0F, .z = -0.5F},
                                                      {.x = 1.0F, .y = 1.0F, .z = 3.0F},
                                                      {.x = 1.0F, .y = 2.0F, .z = 3.0F},
                                                      {.x = 1.0F, .y = 2.0F, .z = 0.0F},
                                                  }));
}

TEST_CASE("the openings into a dais whose floor slopes follow its slope along the inner loop",
          "[world]") {
    // The dais's floor rises northward from 0.5 m along its south wall.
    auto description = dais_map();
    description.sectors.at(1).floor.slope =
        SlopeDescription{.hinge = "dais-south", .gradient = 0.25F};
    const auto map = make_map(description);

    CHECK(map.opening(WallId{5}, {.x = 1.5F, .y = 2.0F}) == Opening{.bottom = 0.75F, .top = 3.0F});
    // The hall's side of the dais's west wall runs north from (1, 1), and its lower strip rises
    // with the dais's floor.
    CHECK(same_polygon(map.face(WallId{4}).lower, {
                                                      {.x = 1.0F, .y = 1.0F, .z = 0.0F},
                                                      {.x = 1.0F, .y = 1.0F, .z = 0.5F},
                                                      {.x = 1.0F, .y = 2.0F, .z = 0.75F},
                                                      {.x = 1.0F, .y = 2.0F, .z = 0.0F},
                                                  }));
}
