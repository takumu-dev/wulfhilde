// Traces through sectors, which stop at their walls, floors and ceilings, and cross openings.
#include <wulfhilde/world/trace.hpp>

#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using wulfhilde::Angle;
using wulfhilde::Location;
using wulfhilde::Map;
using wulfhilde::RigidTransform;
using wulfhilde::SectorCeiling;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
using wulfhilde::SkyDescription;
using wulfhilde::SurfaceRef;
using wulfhilde::Trace;
using wulfhilde::Vector3;
using wulfhilde::WallId;

namespace {

constexpr SectorId hall{0};

// The hall's walls, which run anticlockwise from the corner at the origin.
constexpr WallId hall_south{0};
constexpr WallId hall_east{1};

// The trace of a motion from a point in the hall, which must end in the hall having crossed no
// portal.
Trace trace_in_hall(const Map& map, const Vector3& from, const Vector3& motion) {
    const auto trace = map.trace({.sector = hall, .point = from}, motion);
    CHECK(trace.end.sector == hall);
    CHECK(approx_equal(trace.transform, RigidTransform::identity(), Angle{}, 0.0F));
    return trace;
}

} // namespace

TEST_CASE("a trace that meets nothing ends where its motion takes it", "[world]") {
    const auto map = make_map(hall_map());
    const auto trace =
        trace_in_hall(map, {.x = 1.0F, .y = 1.0F, .z = 1.0F}, {.x = 1.0F, .y = 1.5F, .z = 0.5F});
    CHECK(trace.end.point == Vector3{.x = 2.0F, .y = 2.5F, .z = 1.5F});
    CHECK(trace.surface == std::nullopt);
}

TEST_CASE("a trace stops at the wall it meets", "[world]") {
    const auto map = make_map(hall_map());
    auto trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 4.0F, .y = 1.0F, .z = 0.0F});
    CHECK(approx_equal(trace.end.point, {.x = 4.0F, .y = 2.5F, .z = 1.0F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{hall_east});

    trace =
        trace_in_hall(map, {.x = 1.0F, .y = 3.0F, .z = 2.0F}, {.x = 0.0F, .y = -6.0F, .z = 0.0F});
    CHECK(approx_equal(trace.end.point, {.x = 1.0F, .y = 0.0F, .z = 2.0F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{hall_south});
}

TEST_CASE("a trace stops at the floor moving down, and at the ceiling moving up", "[world]") {
    const auto map = make_map(hall_map());
    auto trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 0.0F, .y = 0.0F, .z = -5.0F});
    CHECK(trace.end.point == Vector3{.x = 2.0F, .y = 2.0F, .z = 0.0F});
    CHECK(trace.surface == SurfaceRef{SectorFloor{hall}});

    trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 1.0F, .y = 0.0F, .z = 5.0F});
    CHECK(approx_equal(trace.end.point, {.x = 2.4F, .y = 2.0F, .z = 3.0F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{SectorCeiling{hall}});
}

TEST_CASE("a trace stops at a sky below moving down, and at a sky above moving up", "[world]") {
    auto description = hall_map();
    description.sectors.at(0).floor.sky = SkyDescription{.backdrop = "test/below"};
    description.sectors.at(0).ceiling.sky = SkyDescription{.backdrop = "test/above"};
    const auto map = make_map(description);
    auto trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 0.0F, .y = 0.0F, .z = -5.0F});
    CHECK(trace.end.point == Vector3{.x = 2.0F, .y = 2.0F, .z = 0.0F});
    CHECK(trace.surface == SurfaceRef{SectorFloor{hall}});

    trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 1.0F, .y = 0.0F, .z = 5.0F});
    CHECK(approx_equal(trace.end.point, {.x = 2.4F, .y = 2.0F, .z = 3.0F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{SectorCeiling{hall}});
}

TEST_CASE("a trace towards two surfaces stops at the one it meets first", "[world]") {
    const auto map = make_map(hall_map());
    // Down towards the floor and the east wall at once, reaching the floor first.
    auto trace =
        trace_in_hall(map, {.x = 3.0F, .y = 2.0F, .z = 0.5F}, {.x = 2.0F, .y = 0.0F, .z = -2.0F});
    CHECK(approx_equal(trace.end.point, {.x = 3.5F, .y = 2.0F, .z = 0.0F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{SectorFloor{hall}});

    // Up towards the ceiling and the east wall at once, reaching the wall first.
    trace =
        trace_in_hall(map, {.x = 3.0F, .y = 2.0F, .z = 2.0F}, {.x = 2.0F, .y = 0.0F, .z = 1.0F});
    CHECK(approx_equal(trace.end.point, {.x = 4.0F, .y = 2.0F, .z = 2.5F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{hall_east});
}

TEST_CASE("a trace across the notch of a concave sector stops at the notch's wall", "[world]") {
    const auto map = make_map(l_shaped_hall_map());
    // From one arm of the L towards the other, whose end is in the sector again.
    const auto trace =
        trace_in_hall(map, {.x = 3.5F, .y = 0.5F, .z = 1.0F}, {.x = -3.0F, .y = 3.0F, .z = 0.0F});
    CHECK(approx_equal(trace.end.point, {.x = 3.0F, .y = 1.0F, .z = 1.0F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{WallId{2}});
}

TEST_CASE("a trace through a corner stops at one of the corner's walls", "[world]") {
    const auto map = make_map(hall_map());
    const auto trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 4.0F, .y = 4.0F, .z = 0.0F});
    CHECK(approx_equal(trace.end.point, {.x = 4.0F, .y = 4.0F, .z = 1.0F}, 1e-6F));
    CHECK((trace.surface == SurfaceRef{WallId{1}} || trace.surface == SurfaceRef{WallId{2}}));
}

TEST_CASE("a trace from a surface stops at once moving into it, and not moving away", "[world]") {
    const auto map = make_map(hall_map());
    constexpr Vector3 on_floor{.x = 2.0F, .y = 2.0F, .z = 0.0F};
    auto trace = trace_in_hall(map, on_floor, {.x = 0.0F, .y = 0.0F, .z = -1.0F});
    CHECK(trace.end.point == on_floor);
    CHECK(trace.surface == SurfaceRef{SectorFloor{hall}});
    trace = trace_in_hall(map, on_floor, {.x = 0.0F, .y = 0.0F, .z = 1.0F});
    CHECK(trace.end.point == Vector3{.x = 2.0F, .y = 2.0F, .z = 1.0F});
    CHECK(trace.surface == std::nullopt);
    trace = trace_in_hall(map, on_floor, {.x = 1.0F, .y = 0.5F, .z = 0.0F});
    CHECK(trace.end.point == Vector3{.x = 3.0F, .y = 2.5F, .z = 0.0F});
    CHECK(trace.surface == std::nullopt);

    constexpr Vector3 on_east_wall{.x = 4.0F, .y = 2.0F, .z = 1.0F};
    trace = trace_in_hall(map, on_east_wall, {.x = 1.0F, .y = 0.0F, .z = 0.0F});
    CHECK(trace.end.point == on_east_wall);
    CHECK(trace.surface == SurfaceRef{hall_east});
    trace = trace_in_hall(map, on_east_wall, {.x = -1.0F, .y = 0.0F, .z = 0.0F});
    CHECK(trace.end.point == Vector3{.x = 3.0F, .y = 2.0F, .z = 1.0F});
    CHECK(trace.surface == std::nullopt);
}

TEST_CASE("a trace that ends exactly on a surface has met it", "[world]") {
    const auto map = make_map(hall_map());
    auto trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 0.0F, .y = 0.0F, .z = 2.0F});
    CHECK(trace.end.point == Vector3{.x = 2.0F, .y = 2.0F, .z = 3.0F});
    CHECK(trace.surface == SurfaceRef{SectorCeiling{hall}});

    trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 2.0F, .y = 0.0F, .z = 0.0F});
    CHECK(trace.end.point == Vector3{.x = 4.0F, .y = 2.0F, .z = 1.0F});
    CHECK(trace.surface == SurfaceRef{hall_east});

    trace =
        trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {.x = 2.0F, .y = 2.0F, .z = 0.0F});
    CHECK(trace.end.point == Vector3{.x = 4.0F, .y = 4.0F, .z = 1.0F});
    CHECK((trace.surface == SurfaceRef{WallId{1}} || trace.surface == SurfaceRef{WallId{2}}));
}

TEST_CASE("a trace along a wall isn't stopped by it", "[world]") {
    const auto map = make_map(hall_map());
    const auto trace =
        trace_in_hall(map, {.x = 4.0F, .y = 1.0F, .z = 1.0F}, {.x = 0.0F, .y = 2.0F, .z = 0.0F});
    CHECK(trace.end.point == Vector3{.x = 4.0F, .y = 3.0F, .z = 1.0F});
    CHECK(trace.surface == std::nullopt);
}

TEST_CASE("a trace with no motion stays where it is", "[world]") {
    const auto map = make_map(hall_map());
    const auto trace = trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = 1.0F}, {});
    CHECK(trace.end == Location{.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}});
    CHECK(trace.surface == std::nullopt);
}

namespace {

constexpr SectorId gallery{1};

// The walls of the doorway map that traces through the doorway meet: the hall's door and the
// solid wall west of it, and the gallery's north wall.
constexpr WallId hall_door{3};
constexpr WallId hall_north_west{4};
constexpr WallId gallery_north{10};

} // namespace

TEST_CASE("a trace through an opening carries on into the sector beyond", "[world]") {
    const auto map = make_map(doorway_map());
    const auto trace = map.trace({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 1.5F}},
                                 {.x = 0.5F, .y = 4.0F});

    CHECK(trace.end == Location{.sector = gallery, .point = {.x = 2.5F, .y = 6.0F, .z = 1.5F}});
    CHECK(trace.surface == std::nullopt);
    CHECK(approx_equal(trace.transform, RigidTransform::identity(), Angle{}, 0.0F));
}

TEST_CASE("a trace through an opening stops at what it meets in the sector beyond", "[world]") {
    auto description = doorway_map();
    description.sectors.at(1).floor.height = 0.5F;
    const auto map = make_map(description);

    const auto wall =
        map.trace({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 1.5F}}, {.y = 10.0F});
    CHECK(wall.end == Location{.sector = gallery, .point = {.x = 2.0F, .y = 8.0F, .z = 1.5F}});
    CHECK(wall.surface == SurfaceRef{gallery_north});

    // It crosses at y = 4 m, 0.6 m high, and reaches the gallery's floor at y = 4.5 m.
    const auto floor = map.trace({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}},
                                 {.y = 4.0F, .z = -0.8F});
    CHECK(floor.end.sector == gallery);
    CHECK(approx_equal(floor.end.point, {.x = 2.0F, .y = 4.5F, .z = 0.5F}, 1e-6F));
    CHECK(floor.surface == SurfaceRef{SectorFloor{gallery}});
}

namespace {

// The doorway map with the gallery's floor at 0.5 m and its ceiling at 2.5 m, so that the hall's
// door has a lower strip and an upper strip.
Map strips_map() {
    auto description = doorway_map();
    description.sectors.at(1).floor.height = 0.5F;
    description.sectors.at(1).ceiling.height = 2.5F;
    return make_map(description);
}

} // namespace

TEST_CASE("a trace that reaches a wall portal above or below its opening meets the strip there",
          "[world]") {
    const auto map = strips_map();
    for (const auto height : {0.25F, 2.75F}) {
        CAPTURE(height);
        const auto trace = trace_in_hall(map, {.x = 2.0F, .y = 2.0F, .z = height}, {.y = 4.0F});
        CHECK(trace.end.point == Vector3{.x = 2.0F, .y = 4.0F, .z = height});
        CHECK(trace.surface == SurfaceRef{hall_door});
    }
}

TEST_CASE("a trace that reaches an opening at its bottom or its top crosses it", "[world]") {
    const auto map = strips_map();
    for (const auto height : {0.5F, 2.5F}) {
        CAPTURE(height);
        const auto trace =
            map.trace({.sector = hall, .point = {.x = 2.0F, .y = 2.0F, .z = height}}, {.y = 4.0F});
        CHECK(trace.end.sector == gallery);
        CHECK(trace.surface == std::nullopt);
    }
}

TEST_CASE("a trace beside an opening stops at the solid wall beside it", "[world]") {
    const auto map = make_map(doorway_map());
    const auto trace = trace_in_hall(map, {.x = 0.5F, .y = 2.0F, .z = 1.5F}, {.y = 4.0F});
    CHECK(trace.end.point == Vector3{.x = 0.5F, .y = 4.0F, .z = 1.5F});
    CHECK(trace.surface == SurfaceRef{hall_north_west});
}

TEST_CASE("a trace through the corner where an opening meets a solid wall stops at the solid wall",
          "[world]") {
    const auto map = make_map(doorway_map());
    const auto trace =
        trace_in_hall(map, {.x = 0.0F, .y = 3.0F, .z = 1.5F}, {.x = 2.0F, .y = 2.0F});
    CHECK(approx_equal(trace.end.point, {.x = 1.0F, .y = 4.0F, .z = 1.5F}, 1e-6F));
    CHECK(trace.surface == SurfaceRef{hall_north_west});
}

TEST_CASE("a trace through an opening and back returns to the sector it started in", "[world]") {
    const auto map = make_map(doorway_map());
    const auto there =
        map.trace({.sector = hall, .point = {.x = 2.0F, .y = 3.0F, .z = 1.5F}}, {.y = 2.0F});
    REQUIRE(there.end.sector == gallery);

    const auto back = map.trace(there.end, {.y = -2.0F});
    CHECK(back.end == Location{.sector = hall, .point = {.x = 2.0F, .y = 3.0F, .z = 1.5F}});
    CHECK(back.surface == std::nullopt);
}
