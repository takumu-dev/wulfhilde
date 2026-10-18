#include <wulfhilde/core/angle.hpp>

#include "math_printing.hpp" // IWYU pragma: keep

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

using wulfhilde::Angle;

TEST_CASE("an angle in degrees is the same angle in radians", "[core]") {
    STATIC_CHECK(Angle::degrees(180.0F) == Angle::radians(std::numbers::pi_v<float>));
    STATIC_CHECK(Angle::radians(std::numbers::pi_v<float> / 2.0F).in_degrees() == 90.0F);
    STATIC_CHECK(Angle::degrees(-45.0F).in_radians() == -std::numbers::pi_v<float> / 4.0F);
    STATIC_CHECK(Angle::degrees(30.0F).in_degrees() == 30.0F);
    STATIC_CHECK(Angle{} == Angle::degrees(0.0F));
}

TEST_CASE("angles add, subtract and scale", "[core]") {
    STATIC_CHECK(Angle::degrees(30.0F) + Angle::degrees(60.0F) == Angle::degrees(90.0F));
    STATIC_CHECK(Angle::degrees(90.0F) - Angle::degrees(60.0F) == Angle::degrees(30.0F));
    STATIC_CHECK(-Angle::degrees(45.0F) == Angle::degrees(-45.0F));
    STATIC_CHECK(Angle::degrees(45.0F) * 2.0F == Angle::degrees(90.0F));
    STATIC_CHECK(2.0F * Angle::degrees(45.0F) == Angle::degrees(90.0F));
    STATIC_CHECK(Angle::degrees(90.0F) / 2.0F == Angle::degrees(45.0F));
    STATIC_CHECK(Angle::degrees(90.0F) / Angle::degrees(45.0F) == 2.0F);

    auto angle = Angle::radians(1.0F);
    angle += Angle::radians(0.5F);
    CHECK(angle == Angle::radians(1.5F));
    angle -= Angle::radians(1.0F);
    CHECK(angle == Angle::radians(0.5F));
    angle *= 3.0F;
    CHECK(angle == Angle::radians(1.5F));
    angle /= 4.0F;
    CHECK(angle == Angle::radians(0.375F));
}

TEST_CASE("angles are ordered by size", "[core]") {
    STATIC_CHECK(Angle::degrees(30.0F) < Angle::degrees(31.0F));
    STATIC_CHECK(Angle::degrees(-90.0F) < Angle::degrees(0.0F));
    STATIC_CHECK(Angle::degrees(90.0F) >= Angle::radians(1.0F));
    STATIC_CHECK(Angle::degrees(1.0F) != Angle::radians(1.0F));
}

TEST_CASE("the sine, cosine and tangent of an angle take its unit into account", "[core]") {
    CHECK(sin(Angle::degrees(30.0F)) == Catch::Approx(0.5F));
    CHECK(cos(Angle::degrees(60.0F)) == Catch::Approx(0.5F));
    CHECK(tan(Angle::degrees(45.0F)) == Catch::Approx(1.0F));
    CHECK(sin(Angle::radians(std::numbers::pi_v<float> / 2.0F)) == 1.0F);
}
