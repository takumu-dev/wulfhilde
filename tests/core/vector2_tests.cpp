#include <wulfhilde/core/vector2.hpp>

#include "glm_reference.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include "random_inputs.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/common.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/geometric.hpp>

#include <cstddef>
#include <type_traits>

using wulfhilde::Vector2;

TEST_CASE("a 2-vector is two packed floats, as a shader reads it", "[core]") {
    STATIC_CHECK(sizeof(Vector2) == 8);
    STATIC_CHECK(std::is_trivially_copyable_v<Vector2>);
    STATIC_CHECK(Vector2{} == Vector2{.x = 0.0F, .y = 0.0F});
}

TEST_CASE("2-vectors add, subtract and scale component by component", "[core]") {
    constexpr Vector2 a{.x = 1.0F, .y = 2.0F};
    constexpr Vector2 b{.x = 4.0F, .y = -5.0F};
    STATIC_CHECK(a + b == Vector2{.x = 5.0F, .y = -3.0F});
    STATIC_CHECK(a - b == Vector2{.x = -3.0F, .y = 7.0F});
    STATIC_CHECK(-a == Vector2{.x = -1.0F, .y = -2.0F});
    STATIC_CHECK(a * 2.0F == Vector2{.x = 2.0F, .y = 4.0F});
    STATIC_CHECK(2.0F * a == Vector2{.x = 2.0F, .y = 4.0F});
    STATIC_CHECK(a / 2.0F == Vector2{.x = 0.5F, .y = 1.0F});

    auto v = a;
    v += b;
    CHECK(v == Vector2{.x = 5.0F, .y = -3.0F});
    v -= a;
    CHECK(v == b);
    v *= 4.0F;
    CHECK(v == Vector2{.x = 16.0F, .y = -20.0F});
    v /= 8.0F;
    CHECK(v == Vector2{.x = 2.0F, .y = -2.5F});
}

TEST_CASE("the perp-dot product of 2-vectors is positive when the second turns left", "[core]") {
    constexpr Vector2 east{.x = 1.0F};
    constexpr Vector2 north{.y = 1.0F};
    STATIC_CHECK(perp_dot(east, north) == 1.0F);
    STATIC_CHECK(perp_dot(north, east) == -1.0F);
    STATIC_CHECK(perp_dot(east, east * 3.0F) == 0.0F);
    STATIC_CHECK(perp_dot(Vector2{.x = 2.0F, .y = 1.0F}, Vector2{.x = 1.0F, .y = 3.0F}) == 5.0F);
}

TEST_CASE("the dot product, length and distance of 2-vectors", "[core]") {
    constexpr Vector2 a{.x = 3.0F, .y = 4.0F};
    STATIC_CHECK(dot(a, Vector2{.x = 1.0F, .y = -0.5F}) == 1.0F);
    STATIC_CHECK(length_squared(a) == 25.0F);
    CHECK(length(a) == 5.0F);
    CHECK(distance(Vector2{.x = -1.0F, .y = 2.0F}, Vector2{.x = 2.0F, .y = 6.0F}) == 5.0F);
}

TEST_CASE("normalising a 2-vector keeps its direction at unit length", "[core]") {
    CHECK(approx_equal(normalize(Vector2{.x = 3.0F, .y = -4.0F}), Vector2{.x = 0.6F, .y = -0.8F},
                       1e-7F));
    CHECK(normalize(Vector2{.y = 0.25F}) == Vector2{.y = 1.0F});
}

TEST_CASE("interpolating between 2-vectors reaches each end exactly", "[core]") {
    constexpr Vector2 a{.x = 0.1F, .y = -7.3F};
    constexpr Vector2 b{.x = 1e6F, .y = 0.3F};
    STATIC_CHECK(lerp(a, b, 0.0F) == a);
    STATIC_CHECK(lerp(a, b, 1.0F) == b);
    STATIC_CHECK(lerp(Vector2{}, Vector2{.x = 2.0F, .y = -8.0F}, 0.75F) ==
                 Vector2{.x = 1.5F, .y = -6.0F});
}

TEST_CASE("2-vectors are approximately equal when they lie within the tolerance of each other",
          "[core]") {
    constexpr Vector2 a{.x = 1.0F, .y = 2.0F};
    STATIC_CHECK(approx_equal(a, a + Vector2{.x = 0.03F, .y = 0.04F}, 0.051F));
    STATIC_CHECK_FALSE(approx_equal(a, a + Vector2{.x = 0.03F, .y = 0.04F}, 0.049F));
}

namespace {

// Checks the generic operations on one set of inputs against GLM.
void check_against_glm(Vector2 a, Vector2 b, float t) {
    CAPTURE(a, b, t);
    const auto scale = length(a) * length(b);

    CHECK(dot(a, b) == Catch::Approx(glm::dot(to_glm(a), to_glm(b))).margin(1e-6F * scale));
    CHECK(perp_dot(a, b) ==
          Catch::Approx(glm::cross(glm::dvec3(to_glm(a), 0.0), glm::dvec3(to_glm(b), 0.0)).z)
              .margin(1e-6F * scale));
    CHECK(length(a) == Catch::Approx(glm::length(to_glm(a))).epsilon(1e-6F));
    CHECK(distance(a, b) == Catch::Approx(glm::distance(to_glm(a), to_glm(b))).epsilon(1e-6F));
    CHECK(approx_equal(normalize(a), from_glm(glm::normalize(to_glm(a))), 1e-6F));
    CHECK(approx_equal(lerp(a, b, t), from_glm(glm::mix(to_glm(a), to_glm(b), double{t})),
                       1e-6F * (length(a) + length(b))));
}

} // namespace

TEST_CASE("2-vector operations agree with GLM", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto a = random.vector2(100.0F);
        const auto b = random.vector2(100.0F);
        check_against_glm(a, b, random.number(0.0F, 1.0F));
    }
}
