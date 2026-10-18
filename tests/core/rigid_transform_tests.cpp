#include <wulfhilde/core/rigid_transform.hpp>

#include "glm_reference.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include "random_inputs.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

#include <cstddef>

using wulfhilde::Angle;
using wulfhilde::RigidTransform;
using wulfhilde::Rotation;
using wulfhilde::Vector3;
using wulfhilde::Vector4;

namespace {

constexpr Vector3 x_axis{.x = 1.0F};
constexpr Vector3 y_axis{.y = 1.0F};
constexpr Vector3 z_axis{.z = 1.0F};

constexpr float tolerance = 1e-5F;

// A turn left by a quarter turn about the origin.
RigidTransform quarter_turn_left() {
    return {.rotation = Rotation::about_axis(z_axis, Angle::degrees(90.0F))};
}

} // namespace

TEST_CASE("the identity transform leaves points and directions as they are", "[core]") {
    constexpr Vector3 v{.x = 1.5F, .y = -2.0F, .z = 3.25F};
    STATIC_CHECK(transform_point(RigidTransform::identity(), v) == v);
    STATIC_CHECK(transform_direction(RigidTransform::identity(), v) == v);
    STATIC_CHECK(transform_point(RigidTransform{}, v) == v);
}

TEST_CASE("a transform turns a point and then moves it, and only turns a direction", "[core]") {
    auto t = quarter_turn_left();
    t.translation = {.x = 10.0F, .z = 2.0F};
    CHECK(approx_equal(transform_point(t, x_axis), Vector3{.x = 10.0F, .y = 1.0F, .z = 2.0F},
                       tolerance));
    CHECK(approx_equal(transform_direction(t, x_axis), y_axis, tolerance));
}

TEST_CASE("crossing one portal and then another composes as the second times the first", "[core]") {
    const RigidTransform first{.translation = {.x = 1.0F}};
    const auto second = quarter_turn_left();
    CHECK(approx_equal(transform_point(second * first, Vector3{}), y_axis, tolerance));
    CHECK(approx_equal(transform_point(first * second, Vector3{}), x_axis, tolerance));

    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto a = random.rigid_transform(100.0F);
        const auto b = random.rigid_transform(100.0F);
        const auto p = random.vector3(100.0F);
        CAPTURE(a, b, p);
        const auto margin = 4e-6F * (length(a.translation) + length(b.translation) + length(p));
        CHECK(approx_equal(transform_point(a * b, p), transform_point(a, transform_point(b, p)),
                           margin));
        CHECK(approx_equal(transform_direction(a * b, p),
                           transform_direction(a, transform_direction(b, p)), margin));
    }
}

TEST_CASE("the inverse of a transform undoes it", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto t = random.rigid_transform(100.0F);
        const auto p = random.vector3(100.0F);
        CAPTURE(t, p);
        const auto margin = 4e-6F * (length(t.translation) + length(p));
        CHECK(approx_equal(transform_point(inverse(t), transform_point(t, p)), p, margin));
        CHECK(approx_equal(inverse(t) * t, RigidTransform{}, Angle::degrees(1e-3F), margin));
        CHECK(approx_equal(t * inverse(t), RigidTransform{}, Angle::degrees(1e-3F), margin));
    }
}

TEST_CASE("composing with the identity transform is exact", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto t = random.rigid_transform(100.0F) * random.rigid_transform(100.0F);
        CAPTURE(t);
        CHECK(to_matrix(RigidTransform{} * t) == to_matrix(t));
        CHECK(to_matrix(t * RigidTransform{}) == to_matrix(t));
    }
}

TEST_CASE("transforms are approximately equal within an angle and a distance", "[core]") {
    auto t = quarter_turn_left();
    t.translation = {.x = 1.0F, .y = 2.0F, .z = 3.0F};
    auto turned = t;
    turned.rotation = Rotation::about_axis(z_axis, Angle::degrees(91.0F));
    auto moved = t;
    moved.translation.z += 0.01F;

    const auto close_angle = Angle::degrees(1.1F);
    CHECK(approx_equal(t, turned, close_angle, 0.0F));
    CHECK_FALSE(approx_equal(t, turned, Angle::degrees(0.9F), 1.0F));
    CHECK(approx_equal(t, moved, Angle{}, 0.011F));
    CHECK_FALSE(approx_equal(t, moved, close_angle, 0.009F));
}

TEST_CASE("a transform's matrix applies it to points and directions in homogeneous coordinates",
          "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto t = random.rigid_transform(100.0F);
        const auto p = random.vector3(100.0F);
        CAPTURE(t, p);
        const auto m = to_matrix(t);
        const auto margin = 4e-6F * (length(t.translation) + length(p));
        const auto point = transform_point(t, p);
        const auto direction = transform_direction(t, p);
        CHECK(std::get<3>(m.columns) ==
              Vector4{.x = t.translation.x, .y = t.translation.y, .z = t.translation.z, .w = 1.0F});
        CHECK(approx_equal(m * Vector4{.x = p.x, .y = p.y, .z = p.z, .w = 1.0F},
                           Vector4{.x = point.x, .y = point.y, .z = point.z, .w = 1.0F}, margin));
        CHECK(approx_equal(m * Vector4{.x = p.x, .y = p.y, .z = p.z},
                           Vector4{.x = direction.x, .y = direction.y, .z = direction.z}, margin));
    }
}

TEST_CASE("transforms agree with GLM", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto axis = random.unit_vector3();
        const auto angle = random.angle();
        const auto translation = random.vector3(100.0F);
        CAPTURE(axis, angle, translation);
        const RigidTransform t{
            .rotation = Rotation::about_axis(axis, angle),
            .translation = translation,
        };
        const auto glm_matrix =
            glm::translate(glm::dmat4(1.0), to_glm(translation)) *
            glm::mat4_cast(glm::angleAxis(double{angle.in_radians()}, to_glm(axis)));
        const auto margin = 4e-6F * length(translation);
        CHECK(approx_equal(to_matrix(t), from_glm(glm_matrix), margin));
        CHECK(approx_equal(to_matrix(inverse(t)), from_glm(glm::inverse(glm_matrix)), margin));
    }
}
