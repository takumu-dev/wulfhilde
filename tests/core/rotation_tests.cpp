#include <wulfhilde/core/rotation.hpp>

#include "glm_reference.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include "random_inputs.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

using wulfhilde::Angle;
using wulfhilde::Rotation;
using wulfhilde::Vector3;
using wulfhilde::Vector4;
using wulfhilde::YawPitchRoll;

namespace {

constexpr Vector3 x_axis{.x = 1.0F};
constexpr Vector3 y_axis{.y = 1.0F};
constexpr Vector3 z_axis{.z = 1.0F};

constexpr float tolerance = 1e-6F;

Rotation yaw_pitch_roll(float yaw, float pitch, float roll) {
    return Rotation::from_yaw_pitch_roll(Angle::degrees(yaw), Angle::degrees(pitch),
                                         Angle::degrees(roll));
}

// The difference between two angles in degrees, a whole number of turns aside: from -180 to 180.
float degrees_apart(Angle a, Angle b) {
    return std::remainder(a.in_degrees() - b.in_degrees(), 360.0F);
}

bool in_ranges(const YawPitchRoll& angles) {
    const auto half_turn = Angle::degrees(180.0F);
    const auto quarter_turn = Angle::degrees(90.0F);
    return angles.yaw > -half_turn && angles.yaw <= half_turn && angles.pitch >= -quarter_turn &&
           angles.pitch <= quarter_turn && angles.roll > -half_turn && angles.roll <= half_turn;
}

// Checks that the yaw, pitch and roll of a rotation made from them come back in their ranges.
void check_angles_come_back(Angle yaw, Angle pitch, Angle roll) {
    CAPTURE(yaw, pitch, roll);
    const auto back = to_yaw_pitch_roll(Rotation::from_yaw_pitch_roll(yaw, pitch, roll));
    CAPTURE(back);
    CHECK(in_ranges(back));
    CHECK(std::abs(degrees_apart(back.yaw, yaw)) <= 1e-3F);
    CHECK(std::abs(degrees_apart(back.pitch, pitch)) <= 1e-3F);
    CHECK(std::abs(degrees_apart(back.roll, roll)) <= 1e-3F);
}

// Checks that a rotation's yaw, pitch and roll are in their ranges and make the same rotation.
void check_rotation_comes_back(const Rotation& r) {
    CAPTURE(r);
    const auto angles = to_yaw_pitch_roll(r);
    CAPTURE(angles);
    CHECK(in_ranges(angles));
    CHECK(approx_equal(Rotation::from_yaw_pitch_roll(angles.yaw, angles.pitch, angles.roll), r,
                       Angle::degrees(2e-3F)));
}

} // namespace

TEST_CASE("at the identity rotation the facing is +X, with +Y to the left and +Z up", "[core]") {
    STATIC_CHECK(Rotation::identity().forward() == x_axis);
    STATIC_CHECK(Rotation::identity().left() == y_axis);
    STATIC_CHECK(Rotation::identity().up() == z_axis);
    STATIC_CHECK(Rotation{}.forward() == x_axis);
    STATIC_CHECK(rotate(Rotation{}, Vector3{.x = 1.0F, .y = -2.0F, .z = 3.0F}) ==
                 Vector3{.x = 1.0F, .y = -2.0F, .z = 3.0F});
}

TEST_CASE("a rotation about an axis turns anticlockwise as seen from where the axis points",
          "[core]") {
    const auto quarter = Angle::degrees(90.0F);
    CHECK(approx_equal(rotate(Rotation::about_axis(z_axis, quarter), x_axis), y_axis, tolerance));
    CHECK(approx_equal(rotate(Rotation::about_axis(x_axis, quarter), y_axis), z_axis, tolerance));
    CHECK(approx_equal(rotate(Rotation::about_axis(y_axis, quarter), z_axis), x_axis, tolerance));
    CHECK(approx_equal(rotate(Rotation::about_axis(-z_axis, quarter), x_axis), -y_axis, tolerance));
}

TEST_CASE("a 90 degree yaw turns the facing left, taking +X to +Y", "[core]") {
    const auto r = yaw_pitch_roll(90.0F, 0.0F, 0.0F);
    CHECK(approx_equal(rotate(r, x_axis), y_axis, tolerance));
    CHECK(approx_equal(r.forward(), y_axis, tolerance));
    CHECK(approx_equal(r.left(), -x_axis, tolerance));
    CHECK(approx_equal(r.up(), z_axis, tolerance));
}

TEST_CASE("a positive pitch tilts the facing up", "[core]") {
    const auto thirty = yaw_pitch_roll(0.0F, 30.0F, 0.0F);
    CHECK(
        approx_equal(thirty.forward(), Vector3{.x = std::sqrt(3.0F) / 2.0F, .z = 0.5F}, tolerance));
    CHECK(approx_equal(thirty.left(), y_axis, tolerance));

    const auto straight_up = yaw_pitch_roll(0.0F, 90.0F, 0.0F);
    CHECK(approx_equal(straight_up.forward(), z_axis, tolerance));
    CHECK(approx_equal(straight_up.up(), -x_axis, tolerance));
}

TEST_CASE("a positive roll tilts the right side down", "[core]") {
    const auto r = yaw_pitch_roll(0.0F, 0.0F, 90.0F);
    CHECK(approx_equal(r.forward(), x_axis, tolerance));
    // The right is -left: it was -Y, and is now -Z.
    CHECK(approx_equal(-r.left(), -z_axis, tolerance));
    CHECK(approx_equal(r.up(), -y_axis, tolerance));
}

TEST_CASE("yaw, pitch and roll apply in that order, each about the axes the last one left",
          "[core]") {
    const auto half = std::sqrt(0.5F);
    CHECK(approx_equal(yaw_pitch_roll(90.0F, 45.0F, 0.0F).forward(), Vector3{.y = half, .z = half},
                       tolerance));

    const auto r = yaw_pitch_roll(90.0F, 90.0F, 90.0F);
    CHECK(approx_equal(r.forward(), z_axis, tolerance));
    CHECK(approx_equal(r.left(), -y_axis, tolerance));
    CHECK(approx_equal(r.up(), x_axis, tolerance));
}

TEST_CASE("a rotation's matrix has its facing, left and up as its first three columns", "[core]") {
    const auto r = yaw_pitch_roll(30.0F, 20.0F, 10.0F);
    const auto [facing, left, up, translation] = to_matrix(r).columns;
    const auto direction = [](Vector3 v) { return Vector4{.x = v.x, .y = v.y, .z = v.z}; };
    CHECK(approx_equal(facing, direction(r.forward()), tolerance));
    CHECK(approx_equal(left, direction(r.left()), tolerance));
    CHECK(approx_equal(up, direction(r.up()), tolerance));
    CHECK(translation == Vector4{.w = 1.0F});
    STATIC_CHECK(to_matrix(Rotation{}) == wulfhilde::Matrix4::identity());
}

TEST_CASE("a composed rotation applies the right one first", "[core]") {
    const auto about_z = Rotation::about_axis(z_axis, Angle::degrees(90.0F));
    const auto about_x = Rotation::about_axis(x_axis, Angle::degrees(90.0F));
    CHECK(approx_equal(rotate(about_z * about_x, y_axis), z_axis, tolerance));
    CHECK(approx_equal(rotate(about_x * about_z, y_axis), -x_axis, tolerance));
}

TEST_CASE("the inverse of a rotation undoes it", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto r = random.rotation();
        const auto v = random.vector3(10.0F);
        CAPTURE(r, v);
        CHECK(approx_equal(rotate(inverse(r), rotate(r, v)), v, 4e-6F * length(v)));
        CHECK(approx_equal(r * inverse(r), Rotation{}, Angle::degrees(1e-3F)));
        CHECK(approx_equal(inverse(r) * r, Rotation{}, Angle::degrees(1e-3F)));
    }
}

TEST_CASE("a rotation keeps lengths and the angles between vectors", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto r = random.rotation();
        const auto a = random.vector3(10.0F);
        const auto b = random.vector3(10.0F);
        CAPTURE(r, a, b);
        CHECK(length(rotate(r, a)) == Catch::Approx(length(a)).epsilon(1e-6F));
        CHECK(dot(rotate(r, a), rotate(r, b)) == Catch::Approx(dot(a, b)).margin(1e-4F));
    }
}

TEST_CASE("rotations are approximately equal when one is within an angle of the other", "[core]") {
    const auto r = Rotation::about_axis(z_axis, Angle::degrees(90.0F));
    CHECK(
        approx_equal(r, Rotation::about_axis(z_axis, Angle::degrees(91.0F)), Angle::degrees(1.1F)));
    CHECK_FALSE(
        approx_equal(r, Rotation::about_axis(z_axis, Angle::degrees(91.0F)), Angle::degrees(0.9F)));
    CHECK_FALSE(approx_equal(r, Rotation::about_axis(x_axis, Angle::degrees(90.0F)),
                             Angle::degrees(45.0F)));
}

TEST_CASE("a rotation by a whole turn more is the same rotation", "[core]") {
    // Their quaternions have opposite signs.
    const auto r = Rotation::about_axis(z_axis, Angle::degrees(90.0F));
    CHECK(approx_equal(r, Rotation::about_axis(z_axis, Angle::degrees(-270.0F)),
                       Angle::degrees(1e-3F)));
    CHECK(approx_equal(r, Rotation::about_axis(z_axis, Angle::degrees(450.0F)),
                       Angle::degrees(1e-3F)));
}

TEST_CASE("rotations agree with GLM", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto axis_a = random.unit_vector3();
        const auto angle_a = random.angle();
        const auto axis_b = random.unit_vector3();
        const auto angle_b = random.angle();
        const auto v = random.vector3(10.0F);
        CAPTURE(axis_a, angle_a, axis_b, angle_b, v);

        const auto a = Rotation::about_axis(axis_a, angle_a);
        const auto b = Rotation::about_axis(axis_b, angle_b);
        const auto glm_a = glm::angleAxis(double{angle_a.in_radians()}, to_glm(axis_a));
        const auto glm_b = glm::angleAxis(double{angle_b.in_radians()}, to_glm(axis_b));

        const auto margin = 4e-6F * length(v);
        CHECK(approx_equal(rotate(a, v), from_glm(glm_a * to_glm(v)), margin));
        CHECK(approx_equal(rotate(a * b, v), from_glm(glm_a * glm_b * to_glm(v)), margin));
        CHECK(
            approx_equal(rotate(inverse(a), v), from_glm(glm::inverse(glm_a) * to_glm(v)), margin));
        CHECK(approx_equal(to_matrix(a), from_glm(glm::mat4_cast(glm_a)), 1e-6F));
    }
}

TEST_CASE("yaw, pitch and roll come back from the rotation they make", "[core]") {
    const auto angles = to_yaw_pitch_roll(yaw_pitch_roll(30.0F, 20.0F, 10.0F));
    CHECK(angles.yaw.in_degrees() == Catch::Approx(30.0F));
    CHECK(angles.pitch.in_degrees() == Catch::Approx(20.0F));
    CHECK(angles.roll.in_degrees() == Catch::Approx(10.0F));

    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto yaw = random.angle();
        const auto pitch = Angle::degrees(random.number(-89.0F, 89.0F));
        check_angles_come_back(yaw, pitch, random.angle());
    }
}

TEST_CASE("a half-turn yaw comes back as 180 degrees rather than -180", "[core]") {
    // Facing up, after a half turn about X, the left is (0, -1, 0), and the arctangent that gives
    // the yaw comes out at exactly -180 degrees.
    const auto r = Rotation::about_axis(x_axis, Angle::degrees(180.0F)) *
                   Rotation::about_axis(y_axis, Angle::degrees(90.0F));
    const auto angles = to_yaw_pitch_roll(r);
    CHECK(angles.yaw == Angle::degrees(180.0F));
    CHECK(angles.pitch == Angle::degrees(90.0F));
    CHECK(angles.roll == Angle{});
}

TEST_CASE("at a pitch of 90 degrees the roll comes back as zero, folded into the yaw", "[core]") {
    const auto up = to_yaw_pitch_roll(yaw_pitch_roll(30.0F, 90.0F, 20.0F));
    CHECK(up.yaw.in_degrees() == Catch::Approx(50.0F));
    CHECK(up.pitch == Angle::degrees(90.0F));
    CHECK(up.roll == Angle{});

    const auto down = to_yaw_pitch_roll(yaw_pitch_roll(30.0F, -90.0F, 20.0F));
    CHECK(down.yaw.in_degrees() == Catch::Approx(10.0F));
    CHECK(down.pitch == Angle::degrees(-90.0F));
    CHECK(down.roll == Angle{});
}

TEST_CASE("every rotation comes back from its yaw, pitch and roll", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        check_rotation_comes_back(random.rotation());
    }

    // Close to straight up or down, where yaw and roll turn about nearly the same axis.
    for (const auto off : {1e-2F, 1e-3F, 1e-4F, 1e-5F, 0.0F}) {
        check_rotation_comes_back(yaw_pitch_roll(-120.0F, 90.0F - off, 75.0F));
        check_rotation_comes_back(yaw_pitch_roll(-120.0F, -90.0F + off, 75.0F));
    }
}

TEST_CASE("a long chain of rotations stays a rotation", "[core]") {
    // A million compositions, as of a view through portal after portal. The quaternion is kept of
    // unit length all along, so each turned axis keeps its length to within float rounding.
    RandomInputs random;
    std::vector<Rotation> steps(100);
    for (auto& step : steps) {
        step = random.rotation();
    }
    Rotation chain;
    float worst = 0.0F;
    for (std::size_t round = 0; round < 10'000; ++round) {
        for (const auto& step : steps) {
            chain = chain * step;
        }
        for (const auto& axis : {chain.forward(), chain.left(), chain.up()}) {
            worst = std::max(worst, std::abs(length(axis) - 1.0F));
        }
    }
    CHECK(worst <= 1e-6F);
}

TEST_CASE("composing with the identity rotation is exact", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto r = random.rotation() * random.rotation();
        CAPTURE(r);
        CHECK(to_matrix(Rotation{} * r) == to_matrix(r));
        CHECK(to_matrix(r * Rotation{}) == to_matrix(r));
    }
}
