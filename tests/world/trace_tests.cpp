// Traces within one sector, which stop at its walls, its floor or its ceiling.
#include <wulfhilde/world/trace.hpp>

#include "descriptions.hpp"
#include "world_printing.hpp" // IWYU pragma: keep
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>

using wulfhilde::Angle;
using wulfhilde::Location;
using wulfhilde::Map;
using wulfhilde::RigidTransform;
using wulfhilde::SectorCeiling;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
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
