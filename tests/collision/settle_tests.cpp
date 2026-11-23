// Bodies placed in one sector, which fit after a small push-out or don't fit at all.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <optional>

using wulfhilde::Angle;
using wulfhilde::Pose;
using wulfhilde::Rotation;
using wulfhilde::SectorId;
using wulfhilde::Vector3;

namespace {

// The only sector of each map.
constexpr SectorId only_sector{0};

// How near a settled body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// Checks that a body settled with its centre in the only sector, within the tolerance of a point.
void check_settled_at(const std::optional<Pose>& settled, const Vector3& point) {
    CAPTURE(settled);
    CHECK((settled.has_value() && settled->centre.sector == only_sector &&
           approx_equal(settled->centre.point, point, tolerance)));
}

// An upright pose of a body with its sector's gravity.
Pose upright_at(const Vector3& centre) {
    return {.centre = {.sector = only_sector, .point = centre}};
}

} // namespace

TEST_CASE("a body that fits where it is settles there", "[collision]") {
    const auto map = hall_map();
    const Vector3 centre{.x = 2.0F, .y = 2.0F, .z = 0.9F};
    check_settled_at(wulfhilde::settle(map, person, upright_at(centre)), centre);
}

TEST_CASE("a body a little way into a wall settles pushed out against it", "[collision]") {
    const auto map = hall_map();
    // Overlapping the east wall by a sixth of its radius.
    const auto settled =
        wulfhilde::settle(map, person, upright_at({.x = 3.75F, .y = 2.0F, .z = 0.9F}));
    check_settled_at(settled, {.x = 3.7F, .y = 2.0F, .z = 0.9F});
}

TEST_CASE("a body further into a wall than a third of its radius doesn't settle", "[collision]") {
    const auto map = hall_map();
    // Overlapping the east wall by half its radius.
    CHECK(wulfhilde::settle(map, person, upright_at({.x = 3.85F, .y = 2.0F, .z = 0.9F})) ==
          std::nullopt);
}

TEST_CASE("a body taller than the gap between floor and ceiling doesn't settle", "[collision]") {
    const auto map = hall_map();
    auto giant = person;
    giant.height = 3.2F;
    CHECK(wulfhilde::settle(map, giant, upright_at({.x = 2.0F, .y = 2.0F, .z = 1.5F})) ==
          std::nullopt);
}

TEST_CASE("a body lying along a corridor settles where one lying across it doesn't",
          "[collision]") {
    const auto map = corridor_map();
    const Vector3 centre{.x = 3.0F, .y = 0.5F, .z = 0.3F};
    const auto along = Rotation::about_axis({.y = 1.0F}, Angle::degrees(90.0F));
    const auto across = Rotation::about_axis({.x = 1.0F}, Angle::degrees(90.0F));

    const auto settled = wulfhilde::settle(
        map, person, {.centre = {.sector = only_sector, .point = centre}, .tilt = along});
    check_settled_at(settled, centre);
    CHECK(wulfhilde::settle(map, person,
                            {.centre = {.sector = only_sector, .point = centre}, .tilt = across}) ==
          std::nullopt);
}

TEST_CASE("a body beyond its sector's walls doesn't settle", "[collision]") {
    const auto map = hall_map();
    CHECK(wulfhilde::settle(map, person, upright_at({.x = 5.0F, .y = 2.0F, .z = 0.9F})) ==
          std::nullopt);
}
