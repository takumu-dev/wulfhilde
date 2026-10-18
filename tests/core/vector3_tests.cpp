#include <wulfhilde/core/vector3.hpp>

#include "glm_reference.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include "random_inputs.hpp"
#include <wulfhilde/core/angle.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cstddef>
#include <type_traits>

using wulfhilde::Angle;
using wulfhilde::Vector3;

namespace {

constexpr Vector3 x_axis{.x = 1.0F};
constexpr Vector3 y_axis{.y = 1.0F};
constexpr Vector3 z_axis{.z = 1.0F};

} // namespace

TEST_CASE("a 3-vector is three packed floats, as a shader reads it", "[core]") {
    STATIC_CHECK(sizeof(Vector3) == 12);
    STATIC_CHECK(std::is_trivially_copyable_v<Vector3>);
    STATIC_CHECK(Vector3{} == Vector3{.x = 0.0F, .y = 0.0F, .z = 0.0F});
}

TEST_CASE("3-vectors add, subtract and scale component by component", "[core]") {
    constexpr Vector3 a{.x = 1.0F, .y = 2.0F, .z = 3.0F};
    constexpr Vector3 b{.x = 4.0F, .y = -5.0F, .z = 0.5F};
    STATIC_CHECK(a + b == Vector3{.x = 5.0F, .y = -3.0F, .z = 3.5F});
    STATIC_CHECK(a - b == Vector3{.x = -3.0F, .y = 7.0F, .z = 2.5F});
    STATIC_CHECK(-a == Vector3{.x = -1.0F, .y = -2.0F, .z = -3.0F});
    STATIC_CHECK(a * 2.0F == Vector3{.x = 2.0F, .y = 4.0F, .z = 6.0F});
    STATIC_CHECK(2.0F * a == Vector3{.x = 2.0F, .y = 4.0F, .z = 6.0F});
    STATIC_CHECK(a / 2.0F == Vector3{.x = 0.5F, .y = 1.0F, .z = 1.5F});

    auto v = a;
    v += b;
    CHECK(v == Vector3{.x = 5.0F, .y = -3.0F, .z = 3.5F});
    v -= a;
    CHECK(v == b);
    v *= 4.0F;
    CHECK(v == Vector3{.x = 16.0F, .y = -20.0F, .z = 2.0F});
    v /= 8.0F;
    CHECK(v == Vector3{.x = 2.0F, .y = -2.5F, .z = 0.25F});
}

TEST_CASE("the cross product of 3-vectors follows the right-hand rule", "[core]") {
    STATIC_CHECK(cross(x_axis, y_axis) == z_axis);
    STATIC_CHECK(cross(y_axis, z_axis) == x_axis);
    STATIC_CHECK(cross(z_axis, x_axis) == y_axis);
    STATIC_CHECK(cross(y_axis, x_axis) == -z_axis);
}

TEST_CASE("the dot product, length and distance of 3-vectors", "[core]") {
    constexpr Vector3 a{.x = 2.0F, .y = 3.0F, .z = 6.0F};
    STATIC_CHECK(dot(a, Vector3{.x = 1.0F, .y = -1.0F, .z = 0.5F}) == 2.0F);
    STATIC_CHECK(dot(x_axis, y_axis) == 0.0F);
    STATIC_CHECK(length_squared(a) == 49.0F);
    CHECK(length(a) == 7.0F);
    CHECK(distance(Vector3{.x = 1.0F, .y = 1.0F, .z = 1.0F},
                   a + Vector3{.x = 1.0F, .y = 1.0F, .z = 1.0F}) == 7.0F);
}

TEST_CASE("normalising a 3-vector keeps its direction at unit length", "[core]") {
    CHECK(approx_equal(normalize(Vector3{.y = 3.0F, .z = 4.0F}), Vector3{.y = 0.6F, .z = 0.8F},
                       1e-7F));
    CHECK(normalize(Vector3{.z = -0.25F}) == -z_axis);
}

TEST_CASE("interpolating between 3-vectors reaches each end exactly", "[core]") {
    constexpr Vector3 a{.x = 0.1F, .y = -7.3F, .z = 1e6F};
    constexpr Vector3 b{.x = 3.7F, .y = 0.3F, .z = -2e-3F};
    STATIC_CHECK(lerp(a, b, 0.0F) == a);
    STATIC_CHECK(lerp(a, b, 1.0F) == b);
    STATIC_CHECK(lerp(Vector3{}, Vector3{.x = 2.0F, .y = 4.0F, .z = -8.0F}, 0.25F) ==
                 Vector3{.x = 0.5F, .y = 1.0F, .z = -2.0F});
}

TEST_CASE("3-vectors are approximately equal when they lie within the tolerance of each other",
          "[core]") {
    constexpr Vector3 a{.x = 1.0F, .y = 2.0F, .z = 3.0F};
    STATIC_CHECK(approx_equal(a, a + Vector3{.x = 0.03F, .y = 0.04F}, 0.051F));
    STATIC_CHECK_FALSE(approx_equal(a, a + Vector3{.x = 0.03F, .y = 0.04F}, 0.049F));
    // Each component is within the tolerance, but the vectors aren't.
    STATIC_CHECK_FALSE(approx_equal(a, a + Vector3{.x = 0.04F, .y = 0.04F, .z = 0.04F}, 0.05F));
}

TEST_CASE("the angle between 3-vectors stays accurate near 0 and 180 degrees", "[core]") {
    CHECK(angle_between(x_axis, y_axis).in_degrees() == Catch::Approx(90.0F));
    CHECK(angle_between(x_axis, x_axis) == Angle{});
    CHECK(angle_between(x_axis, -x_axis) == Angle::degrees(180.0F));

    // Their dot product rounds to 1 and -1 in float, where an arccosine would give 0 and 180
    // degrees.
    const auto tiny = Angle::radians(1e-4F);
    const Vector3 near_x{.x = cos(tiny), .y = sin(tiny)};
    CHECK(angle_between(x_axis, near_x).in_radians() == Catch::Approx(1e-4F).epsilon(1e-3F));
    CHECK((Angle::degrees(180.0F) - angle_between(-x_axis, near_x)).in_radians() ==
          Catch::Approx(1e-4F).epsilon(1e-2F));
}

TEST_CASE("the angle between 3-vectors doesn't depend on their lengths", "[core]") {
    CHECK(angle_between(x_axis * 0.5F, Vector3{.x = 3.0F, .y = 3.0F}).in_degrees() ==
          Catch::Approx(45.0F));
}

namespace {

// Checks the generic operations on one set of inputs against GLM.
void check_against_glm(Vector3 a, Vector3 b, float t) {
    CAPTURE(a, b, t);
    const auto scale = length(a) * length(b);

    CHECK(dot(a, b) == Catch::Approx(glm::dot(to_glm(a), to_glm(b))).margin(1e-6F * scale));
    CHECK(approx_equal(cross(a, b), from_glm(glm::cross(to_glm(a), to_glm(b))), 1e-6F * scale));
    CHECK(length(a) == Catch::Approx(glm::length(to_glm(a))).epsilon(1e-6F));
    CHECK(distance(a, b) == Catch::Approx(glm::distance(to_glm(a), to_glm(b))).epsilon(1e-6F));
    CHECK(approx_equal(normalize(a), from_glm(glm::normalize(to_glm(a))), 1e-6F));
    CHECK(approx_equal(lerp(a, b, t), from_glm(glm::mix(to_glm(a), to_glm(b), double{t})),
                       1e-6F * (length(a) + length(b))));
}

} // namespace

TEST_CASE("3-vector operations agree with GLM", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto a = random.vector3(100.0F);
        const auto b = random.vector3(100.0F);
        check_against_glm(a, b, random.number(0.0F, 1.0F));
    }
}
