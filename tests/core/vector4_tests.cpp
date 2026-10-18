#include <wulfhilde/core/vector4.hpp>

#include "glm_reference.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include "random_inputs.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <cstddef>
#include <type_traits>

using wulfhilde::Vector4;

TEST_CASE("a 4-vector is four packed floats, as a shader reads it", "[core]") {
    STATIC_CHECK(sizeof(Vector4) == 16);
    STATIC_CHECK(std::is_trivially_copyable_v<Vector4>);
    STATIC_CHECK(Vector4{} == Vector4{.x = 0.0F, .y = 0.0F, .z = 0.0F, .w = 0.0F});
}

TEST_CASE("4-vectors add, subtract and scale component by component", "[core]") {
    constexpr Vector4 a{.x = 1.0F, .y = 2.0F, .z = 3.0F, .w = 4.0F};
    constexpr Vector4 b{.x = 4.0F, .y = -5.0F, .z = 0.5F, .w = 1.0F};
    STATIC_CHECK(a + b == Vector4{.x = 5.0F, .y = -3.0F, .z = 3.5F, .w = 5.0F});
    STATIC_CHECK(a - b == Vector4{.x = -3.0F, .y = 7.0F, .z = 2.5F, .w = 3.0F});
    STATIC_CHECK(-a == Vector4{.x = -1.0F, .y = -2.0F, .z = -3.0F, .w = -4.0F});
    STATIC_CHECK(a * 2.0F == Vector4{.x = 2.0F, .y = 4.0F, .z = 6.0F, .w = 8.0F});
    STATIC_CHECK(2.0F * a == Vector4{.x = 2.0F, .y = 4.0F, .z = 6.0F, .w = 8.0F});
    STATIC_CHECK(a / 2.0F == Vector4{.x = 0.5F, .y = 1.0F, .z = 1.5F, .w = 2.0F});

    auto v = a;
    v += b;
    CHECK(v == Vector4{.x = 5.0F, .y = -3.0F, .z = 3.5F, .w = 5.0F});
    v -= a;
    CHECK(v == b);
    v *= 4.0F;
    CHECK(v == Vector4{.x = 16.0F, .y = -20.0F, .z = 2.0F, .w = 4.0F});
    v /= 8.0F;
    CHECK(v == Vector4{.x = 2.0F, .y = -2.5F, .z = 0.25F, .w = 0.5F});
}

TEST_CASE("the dot product, length and distance of 4-vectors", "[core]") {
    constexpr Vector4 a{.x = 1.0F, .y = 2.0F, .z = 4.0F, .w = 10.0F};
    STATIC_CHECK(dot(a, Vector4{.x = 1.0F, .y = -1.0F, .z = 0.5F, .w = 0.25F}) == 3.5F);
    STATIC_CHECK(length_squared(a) == 121.0F);
    CHECK(length(a) == 11.0F);
    CHECK(distance(Vector4{.w = 1.0F}, a + Vector4{.w = 1.0F}) == 11.0F);
}

TEST_CASE("normalising a 4-vector keeps its direction at unit length", "[core]") {
    CHECK(approx_equal(normalize(Vector4{.x = 1.0F, .y = 1.0F, .z = 1.0F, .w = 1.0F}),
                       Vector4{.x = 0.5F, .y = 0.5F, .z = 0.5F, .w = 0.5F}, 1e-7F));
    CHECK(normalize(Vector4{.w = -0.25F}) == Vector4{.w = -1.0F});
}

TEST_CASE("interpolating between 4-vectors reaches each end exactly", "[core]") {
    constexpr Vector4 a{.x = 0.1F, .y = -7.3F, .z = 1e6F, .w = 1.0F};
    constexpr Vector4 b{.x = 3.7F, .y = 0.3F, .z = -2e-3F, .w = 0.0F};
    STATIC_CHECK(lerp(a, b, 0.0F) == a);
    STATIC_CHECK(lerp(a, b, 1.0F) == b);
    STATIC_CHECK(lerp(Vector4{}, Vector4{.x = 2.0F, .y = 4.0F, .z = -8.0F, .w = 1.0F}, 0.25F) ==
                 Vector4{.x = 0.5F, .y = 1.0F, .z = -2.0F, .w = 0.25F});
}

TEST_CASE("4-vectors are approximately equal when they lie within the tolerance of each other",
          "[core]") {
    constexpr Vector4 a{.x = 1.0F, .y = 2.0F, .z = 3.0F, .w = 4.0F};
    STATIC_CHECK(approx_equal(a, a + Vector4{.z = 0.03F, .w = 0.04F}, 0.051F));
    STATIC_CHECK_FALSE(approx_equal(a, a + Vector4{.z = 0.03F, .w = 0.04F}, 0.049F));
}

namespace {

// Checks the generic operations on one set of inputs against GLM.
void check_against_glm(Vector4 a, Vector4 b, float t) {
    CAPTURE(a, b, t);
    const auto scale = length(a) * length(b);

    CHECK(dot(a, b) == Catch::Approx(glm::dot(to_glm(a), to_glm(b))).margin(1e-6F * scale));
    CHECK(length(a) == Catch::Approx(glm::length(to_glm(a))).epsilon(1e-6F));
    CHECK(distance(a, b) == Catch::Approx(glm::distance(to_glm(a), to_glm(b))).epsilon(1e-6F));
    CHECK(approx_equal(normalize(a), from_glm(glm::normalize(to_glm(a))), 1e-6F));
    CHECK(approx_equal(lerp(a, b, t), from_glm(glm::mix(to_glm(a), to_glm(b), double{t})),
                       1e-6F * (length(a) + length(b))));
}

} // namespace

TEST_CASE("4-vector operations agree with GLM", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto a = random.vector4(100.0F);
        const auto b = random.vector4(100.0F);
        check_against_glm(a, b, random.number(0.0F, 1.0F));
    }
}
