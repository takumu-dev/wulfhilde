#include <wulfhilde/core/plane.hpp>

#include "math_printing.hpp" // IWYU pragma: keep
#include "random_inputs.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>

using wulfhilde::Angle;
using wulfhilde::Plane;
using wulfhilde::RigidTransform;
using wulfhilde::Rotation;
using wulfhilde::Vector3;

namespace {

// The plane z = 2, its normal pointing up.
constexpr Plane z_equals_two{.normal = {.z = 1.0F}, .distance = 2.0F};

} // namespace

TEST_CASE("a point's signed distance from a plane is positive on the side its normal points to",
          "[core]") {
    STATIC_CHECK(signed_distance(z_equals_two, Vector3{.x = 5.0F, .y = 7.0F, .z = 3.0F}) == 1.0F);
    STATIC_CHECK(signed_distance(z_equals_two, Vector3{}) == -2.0F);
    STATIC_CHECK(signed_distance(z_equals_two, Vector3{.x = -1.0F, .y = 1.0F, .z = 2.0F}) == 0.0F);
    STATIC_CHECK(signed_distance(Plane{.normal = {.x = -1.0F}, .distance = 0.5F},
                                 Vector3{.x = -3.0F}) == 2.5F);
}

TEST_CASE("a transformed plane moves with the transform", "[core]") {
    STATIC_CHECK(transform(RigidTransform{.translation = {.x = 9.0F, .z = 3.0F}}, z_equals_two) ==
                 Plane{.normal = {.z = 1.0F}, .distance = 5.0F});

    const RigidTransform tipped{
        .rotation = Rotation::about_axis({.x = 1.0F}, Angle::degrees(90.0F)),
    };
    const auto turned = transform(tipped, z_equals_two);
    CHECK(approx_equal(turned.normal, Vector3{.y = -1.0F}, 1e-6F));
    CHECK(turned.distance == Catch::Approx(2.0F));
}

TEST_CASE("a point keeps its signed distance from a plane when both are transformed", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto t = random.rigid_transform(100.0F);
        const Plane plane{
            .normal = random.unit_vector3(),
            .distance = random.number(-100.0F, 100.0F),
        };
        const auto p = random.vector3(100.0F);
        CAPTURE(t, plane, p);
        CHECK(signed_distance(transform(t, plane), transform_point(t, p)) ==
              Catch::Approx(signed_distance(plane, p)).margin(1e-4F));
    }
}
