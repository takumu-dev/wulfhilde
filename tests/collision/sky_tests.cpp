// Bodies moved against skies, which are solid like any floor or ceiling, and which a body stands
// on, whatever its gravity.
#include "collision_printing.hpp" // IWYU pragma: keep
#include "maps.hpp"
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>

using wulfhilde::Angle;
using wulfhilde::Ground;
using wulfhilde::Map;
using wulfhilde::Rotation;
using wulfhilde::SectorCeiling;
using wulfhilde::SectorFloor;
using wulfhilde::SectorId;
using wulfhilde::SkyDescription;
using wulfhilde::Vector3;

namespace {

constexpr SectorId abyss{0};

// How near a body ends to where the geometry puts it.
constexpr float tolerance = 1e-4F;

// The abyss: a square sector 4 m on a side from (0, 0) to (4, 4), whose floor 10 m down and
// ceiling 3 m up are both skies.
Map abyss_map() {
    auto sector = sector_through("abyss", -10.0F, 3.0F,
                                 {
                                     {.x = 0.0F, .y = 0.0F},
                                     {.x = 4.0F, .y = 0.0F},
                                     {.x = 4.0F, .y = 4.0F},
                                     {.x = 0.0F, .y = 4.0F},
                                 });
    sector.floor.sky = SkyDescription{.backdrop = "test/below"};
    sector.ceiling.sky = SkyDescription{.backdrop = "test/above"};
    return one_sector_map(std::move(sector));
}

} // namespace

TEST_CASE("a body standing on a sky floor stays there, with the sky as its ground", "[collision]") {
    const auto map = abyss_map();
    const auto movement = wulfhilde::move(
        map, person, {.centre = {.sector = abyss, .point = {.x = 2.0F, .y = 2.0F, .z = -9.1F}}},
        {.z = -1.0F}, 0.1F);

    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 2.0F, .z = -9.1F}, tolerance));
    CHECK(approx_equal(movement.velocity, Vector3{}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorFloor{abyss}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body falling into an abyss lands on its sky floor rather than leaving the map",
          "[collision]") {
    const auto map = abyss_map();
    const auto movement = wulfhilde::move(
        map, person, {.centre = {.sector = abyss, .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}}},
        {.z = -20.0F}, 1.0F);

    CHECK(movement.pose.centre.sector == abyss);
    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 2.0F, .z = -9.1F}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorFloor{abyss}, .normal = {.z = 1.0F}});
}

TEST_CASE("a body moving up stops against a sky ceiling, which isn't its ground", "[collision]") {
    const auto map = abyss_map();
    const auto movement = wulfhilde::move(
        map, person, {.centre = {.sector = abyss, .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}}},
        {.z = 3.0F}, 1.0F);

    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 2.0F, .z = 2.1F}, tolerance));
    CHECK(movement.ground == std::nullopt);
}

TEST_CASE("a body whose gravity pulls it onto a sky ceiling stands on it", "[collision]") {
    const auto map = abyss_map();
    // Upside down, falling toward the ceiling.
    const auto movement =
        wulfhilde::move(map, person,
                        {
                            .centre = {.sector = abyss, .point = {.x = 2.0F, .y = 2.0F, .z = 1.0F}},
                            .tilt = Rotation::about_axis({.x = 1.0F}, Angle::degrees(180.0F)),
                            .gravity_up = {.z = -1.0F},
                        },
                        {.z = 3.0F}, 1.0F);

    CHECK(approx_equal(movement.pose.centre.point, {.x = 2.0F, .y = 2.0F, .z = 2.1F}, tolerance));
    CHECK(movement.ground == Ground{.surface = SectorCeiling{abyss}, .normal = {.z = -1.0F}});
}
