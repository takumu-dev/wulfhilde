#include <wulfhilde/core/color.hpp>

#include "math_printing.hpp" // IWYU pragma: keep

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using wulfhilde::Color;

namespace {

// Rec.709's luminance, derived from its primaries and D65 white.
float luminance(const Color& c) {
    return (0.2126390F * c.r) + (0.7151687F * c.g) + (0.0721923F * c.b);
}

struct Chromaticity {
    float x;
    float y;
};

// The CIE 1931 chromaticity of a colour, through the matrix from linear Rec.709 to CIE XYZ.
Chromaticity chromaticity(const Color& c) {
    const auto x = (0.4123908F * c.r) + (0.3575843F * c.g) + (0.1804808F * c.b);
    const auto y = luminance(c);
    const auto z = (0.0193308F * c.r) + (0.1191948F * c.g) + (0.9505322F * c.b);
    return {.x = x / (x + y + z), .y = y / (x + y + z)};
}

} // namespace

TEST_CASE("a colour is three packed floats, as a shader reads it", "[core]") {
    STATIC_CHECK(sizeof(Color) == 12);
    STATIC_CHECK(std::is_trivially_copyable_v<Color>);
}

TEST_CASE("a colour scales, and multiplies another component by component, without clamping",
          "[core]") {
    constexpr Color c{.r = 0.5F, .g = 1.0F, .b = 0.25F};
    STATIC_CHECK(c * 4.0F == Color{.r = 2.0F, .g = 4.0F, .b = 1.0F});
    STATIC_CHECK(4.0F * c == Color{.r = 2.0F, .g = 4.0F, .b = 1.0F});
    STATIC_CHECK(c * Color{.r = 2.0F, .g = 0.5F, .b = 8.0F} ==
                 Color{.r = 1.0F, .g = 0.5F, .b = 2.0F});
}

TEST_CASE("a colour temperature has the luminance of white", "[core]") {
    for (const auto kelvin : {1000.0F, 1500.0F, 2700.0F, 4000.0F, 6500.0F, 10000.0F, 40000.0F}) {
        CAPTURE(kelvin);
        CHECK(luminance(Color::from_temperature(kelvin)) == Catch::Approx(1.0F).epsilon(1e-5F));
    }
}

TEST_CASE("a colour temperature lies on the Planckian locus of the CIE 1931 observer", "[core]") {
    // The locus as integrated from the CIE's tabulated observer at 1 nm. The fit of the observer
    // the engine integrates against strays from it by less than 0.003 at these temperatures.
    struct Point {
        float kelvin;
        Chromaticity expected;
    };

    for (const auto& [kelvin, expected] : {
             Point{.kelvin = 2000.0F, .expected = {.x = 0.52668F, .y = 0.41330F}},
             Point{.kelvin = 2700.0F, .expected = {.x = 0.45986F, .y = 0.41060F}},
             Point{.kelvin = 4000.0F, .expected = {.x = 0.38044F, .y = 0.37675F}},
             Point{.kelvin = 6500.0F, .expected = {.x = 0.31353F, .y = 0.32363F}},
             Point{.kelvin = 10000.0F, .expected = {.x = 0.28063F, .y = 0.28829F}},
             Point{.kelvin = 40000.0F, .expected = {.x = 0.24720F, .y = 0.24472F}},
         }) {
        CAPTURE(kelvin);
        const auto actual = chromaticity(Color::from_temperature(kelvin));
        CHECK(actual.x == Catch::Approx(expected.x).margin(0.003F));
        CHECK(actual.y == Catch::Approx(expected.y).margin(0.003F));
    }
}

TEST_CASE("a warm white is redder than white, and a cool one bluer", "[core]") {
    // From an independent integration in double of Planck's law against the same fit.
    const auto warm = Color::from_temperature(2700.0F);
    CHECK(warm.r == Catch::Approx(1.911856F).epsilon(1e-4F));
    CHECK(warm.g == Catch::Approx(0.810597F).epsilon(1e-4F));
    CHECK(warm.b == Catch::Approx(0.190479F).epsilon(1e-4F));

    const auto cool = Color::from_temperature(10000.0F);
    CHECK(cool.r == Catch::Approx(0.871450F).epsilon(1e-4F));
    CHECK(cool.g == Catch::Approx(0.994860F).epsilon(1e-4F));
    CHECK(cool.b == Catch::Approx(1.429558F).epsilon(1e-4F));
}

TEST_CASE("a black body redder than Rec.709 can show gives out no negative light", "[core]") {
    // Below about 1,900 K, a black body lies outside Rec.709's gamut, with a negative blue.
    const auto ember = Color::from_temperature(1000.0F);
    CHECK(ember.r == Catch::Approx(4.283090F).epsilon(1e-4F));
    CHECK(ember.g == Catch::Approx(0.124793F).epsilon(1e-4F));
    CHECK(ember.b == 0.0F);
}
