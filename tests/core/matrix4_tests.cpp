#include <wulfhilde/core/matrix4.hpp>

#include "glm_reference.hpp"
#include "math_printing.hpp" // IWYU pragma: keep
#include "random_inputs.hpp"
#include <wulfhilde/core/vector4.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <cstddef>
#include <type_traits>

using wulfhilde::Matrix4;
using wulfhilde::Vector4;

namespace {

// A matrix whose element in column c and row r is 10c + r, so each element names its place.
constexpr Matrix4 numbered{
    .columns =
        {
            Vector4{.x = 0.0F, .y = 1.0F, .z = 2.0F, .w = 3.0F},
            Vector4{.x = 10.0F, .y = 11.0F, .z = 12.0F, .w = 13.0F},
            Vector4{.x = 20.0F, .y = 21.0F, .z = 22.0F, .w = 23.0F},
            Vector4{.x = 30.0F, .y = 31.0F, .z = 32.0F, .w = 33.0F},
        },
};

} // namespace

TEST_CASE("a 4x4 matrix is sixteen packed floats, as a shader reads it", "[core]") {
    STATIC_CHECK(sizeof(Matrix4) == 64);
    STATIC_CHECK(std::is_trivially_copyable_v<Matrix4>);
}

TEST_CASE("a matrix is stored column by column, its translation in elements 12 to 14", "[core]") {
    constexpr auto elements = std::bit_cast<std::array<float, 16>>(numbered);
    for (std::size_t i = 0; i < elements.size(); ++i) {
        const auto column = i / 4;
        const auto row = i % 4;
        CAPTURE(i);
        CHECK(elements.at(i) == static_cast<float>((10 * column) + row));
    }
}

TEST_CASE("the identity matrix leaves a vector unchanged", "[core]") {
    constexpr Vector4 v{.x = 1.5F, .y = -2.0F, .z = 3.25F, .w = 1.0F};
    STATIC_CHECK(Matrix4::identity() * v == v);
    STATIC_CHECK(Matrix4::identity() * numbered == numbered);
    STATIC_CHECK(numbered * Matrix4::identity() == numbered);
}

TEST_CASE("a matrix multiplies a column vector on its left", "[core]") {
    STATIC_CHECK(numbered * Vector4{.x = 1.0F} == std::get<0>(numbered.columns));
    STATIC_CHECK(numbered * Vector4{.w = 1.0F} == std::get<3>(numbered.columns));
    STATIC_CHECK(numbered * Vector4{.x = 1.0F, .y = 2.0F, .z = 3.0F, .w = 4.0F} ==
                 Vector4{.x = 200.0F, .y = 210.0F, .z = 220.0F, .w = 230.0F});
}

TEST_CASE("a product of matrices applies the right one first", "[core]") {
    constexpr Matrix4 scale{
        .columns =
            {
                Vector4{.x = 2.0F},
                Vector4{.y = 3.0F},
                Vector4{.z = 4.0F},
                Vector4{.w = 1.0F},
            },
    };
    constexpr Matrix4 translation{
        .columns =
            {
                Vector4{.x = 1.0F},
                Vector4{.y = 1.0F},
                Vector4{.z = 1.0F},
                Vector4{.x = 5.0F, .y = 6.0F, .z = 7.0F, .w = 1.0F},
            },
    };
    constexpr Vector4 point{.x = 1.0F, .y = 1.0F, .z = 1.0F, .w = 1.0F};
    STATIC_CHECK(scale * translation * point ==
                 Vector4{.x = 12.0F, .y = 21.0F, .z = 32.0F, .w = 1.0F});
    STATIC_CHECK(translation * scale * point ==
                 Vector4{.x = 7.0F, .y = 9.0F, .z = 11.0F, .w = 1.0F});
    STATIC_CHECK((scale * translation) * point == scale * (translation * point));
}

TEST_CASE("matrix products agree with GLM", "[core]") {
    RandomInputs random;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto a = random.matrix4(10.0F);
        const auto b = random.matrix4(10.0F);
        const auto v = random.vector4(10.0F);
        CAPTURE(a, b, v);

        CHECK(approx_equal(a * v, from_glm(to_glm(a) * to_glm(v)), 1e-4F));
        CHECK(approx_equal(a * b, from_glm(to_glm(a) * to_glm(b)), 1e-4F));
    }
}
