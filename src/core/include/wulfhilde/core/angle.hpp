/// \file
/// An angle, with its unit stated wherever one is made or read.

#pragma once

#include <cmath>
#include <compare>
#include <numbers>

namespace wulfhilde {

/// An angle, such as a camera's field of view, a spot light's cone or a body's steepest slope.
///
/// It is made only from a number with its unit, `Angle::degrees(90.0F)` or `Angle::radians(x)`,
/// and read only by naming the unit again, so no angle in the interface is a bare number of
/// unknown unit. It is stored in radians.
class Angle {
  public:
    /// Makes a zero angle, which is the same in every unit.
    constexpr Angle() noexcept = default;

    /// Makes an angle from a number of radians.
    ///
    /// \param radians The angle in radians.
    /// \return The angle.
    [[nodiscard]] static constexpr Angle radians(float radians) noexcept { return Angle(radians); }

    /// Makes an angle from a number of degrees.
    ///
    /// \param degrees The angle in degrees.
    /// \return The angle.
    [[nodiscard]] static constexpr Angle degrees(float degrees) noexcept {
        return Angle(static_cast<float>(static_cast<double>(degrees) * radians_per_degree));
    }

    /// Returns the angle in radians.
    [[nodiscard]] constexpr float in_radians() const noexcept { return radians_; }

    /// Returns the angle in degrees.
    [[nodiscard]] constexpr float in_degrees() const noexcept {
        return static_cast<float>(static_cast<double>(radians_) / radians_per_degree);
    }

    /// Adds an angle to this one.
    ///
    /// \param other The angle to add.
    /// \return This angle.
    constexpr Angle& operator+=(Angle other) noexcept {
        radians_ += other.radians_;
        return *this;
    }

    /// Subtracts an angle from this one.
    ///
    /// \param other The angle to subtract.
    /// \return This angle.
    constexpr Angle& operator-=(Angle other) noexcept {
        radians_ -= other.radians_;
        return *this;
    }

    /// Multiplies this angle by a number.
    ///
    /// \param factor The number to multiply by.
    /// \return This angle.
    constexpr Angle& operator*=(float factor) noexcept {
        radians_ *= factor;
        return *this;
    }

    /// Divides this angle by a number.
    ///
    /// \param divisor The number to divide by.
    /// \return This angle.
    constexpr Angle& operator/=(float divisor) noexcept {
        radians_ /= divisor;
        return *this;
    }

    /// Returns the sum of two angles.
    friend constexpr Angle operator+(Angle a, Angle b) noexcept { return a += b; }

    /// Returns the difference of two angles.
    friend constexpr Angle operator-(Angle a, Angle b) noexcept { return a -= b; }

    /// Returns the angle of the opposite sign.
    friend constexpr Angle operator-(Angle angle) noexcept { return Angle(-angle.radians_); }

    /// Returns an angle multiplied by a number.
    friend constexpr Angle operator*(Angle angle, float factor) noexcept { return angle *= factor; }

    /// Returns an angle multiplied by a number.
    friend constexpr Angle operator*(float factor, Angle angle) noexcept { return angle *= factor; }

    /// Returns an angle divided by a number.
    friend constexpr Angle operator/(Angle angle, float divisor) noexcept {
        return angle /= divisor;
    }

    /// Returns how many times one angle goes into another, a number without a unit.
    friend constexpr float operator/(Angle angle, Angle divisor) noexcept {
        return angle.radians_ / divisor.radians_;
    }

    /// Compares two angles exactly.
    constexpr bool operator==(const Angle&) const noexcept = default;

    /// Orders two angles by size, as their values in radians are ordered.
    constexpr std::partial_ordering operator<=>(const Angle&) const noexcept = default;

  private:
    /// The size of a degree in radians. Converting through double rounds once, so that a whole
    /// number of degrees such as 90 comes back exactly.
    static constexpr double radians_per_degree = std::numbers::pi / 180.0;

    /// Makes an angle from a number of radians.
    ///
    /// \param radians The angle in radians.
    constexpr explicit Angle(float radians) noexcept : radians_(radians) {}

    /// The angle in radians.
    float radians_ = 0.0F;
};

/// Returns the sine of an angle.
///
/// \param angle The angle.
/// \return Its sine.
[[nodiscard]] inline float sin(Angle angle) noexcept { return std::sin(angle.in_radians()); }

/// Returns the cosine of an angle.
///
/// \param angle The angle.
/// \return Its cosine.
[[nodiscard]] inline float cos(Angle angle) noexcept { return std::cos(angle.in_radians()); }

/// Returns the tangent of an angle.
///
/// \param angle The angle.
/// \return Its tangent.
[[nodiscard]] inline float tan(Angle angle) noexcept { return std::tan(angle.in_radians()); }

} // namespace wulfhilde
