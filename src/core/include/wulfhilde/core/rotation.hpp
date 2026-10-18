/// \file
/// A rotation in three dimensions, and the engine's conventions for facings.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>

#include <cmath>
#include <limits>
#include <numbers>

namespace wulfhilde {

/// A facing given as a yaw, a pitch and a roll, which apply in that order.
struct YawPitchRoll {
    Angle yaw;   ///< How far to turn left, anticlockwise seen from above, about Z.
    Angle pitch; ///< How far then to tilt the facing up, about the new Y.
    Angle roll;  ///< How far then to tilt the right side down, about the new X.
};

/// A rotation in three dimensions, such as a facing, the tilt of a body or the turn of a portal
/// transform.
///
/// At the identity rotation, the facing is +X with +Z up, so +Y is to the left. A rotation turns
/// those axes to its `forward()`, `left()` and `up()`. Rotations made from yaw, pitch and roll
/// follow one convention everywhere, in portal transforms, the start position and lights alike:
/// yaw turns left (anticlockwise seen from above) about Z, then pitch tilts the facing up about the
/// new Y, then roll tilts the right side down about the new X.
///
/// It holds a unit quaternion, which is renormalised after each composition, so a long chain of
/// portal crossings doesn't drift away from a rotation.
class Rotation {
  public:
    /// Makes the identity rotation, which leaves every vector as it is.
    constexpr Rotation() noexcept = default;

    /// Returns the identity rotation, which leaves every vector as it is.
    [[nodiscard]] static constexpr Rotation identity() noexcept { return {}; }

    /// Makes the rotation about an axis by an angle.
    ///
    /// \param unit_axis The axis to turn about, of unit length: the square of its length must be
    ///                  within 0.0001 of 1.
    /// \param angle How far to turn: a positive angle turns anticlockwise as seen from where the
    ///              axis points, by the right-hand rule.
    /// \return The rotation.
    [[nodiscard]] static Rotation about_axis(const Vector3& unit_axis, Angle angle) {
        WULFHILDE_ASSERT(std::abs(length_squared(unit_axis) - 1.0F) <= 1e-4F,
                         "the axis of a rotation is of unit length");
        return about_unit_axis(unit_axis, angle).renormalized();
    }

    /// Makes the rotation that turns the facing by a yaw, a pitch and a roll, in that order.
    ///
    /// \param yaw How far to turn left, anticlockwise seen from above, about Z.
    /// \param pitch How far then to tilt the facing up, about the new Y.
    /// \param roll How far then to tilt the right side down, about the new X.
    /// \return The rotation.
    [[nodiscard]] static Rotation from_yaw_pitch_roll(Angle yaw, Angle pitch, Angle roll) noexcept {
        // Pitching up turns +X toward +Z, which is a turn about -Y by the right-hand rule.
        return about_unit_axis({.z = 1.0F}, yaw) * about_unit_axis({.y = -1.0F}, pitch) *
               about_unit_axis({.x = 1.0F}, roll);
    }

    /// Returns the facing: where the rotation takes +X.
    [[nodiscard]] constexpr Vector3 forward() const noexcept;

    /// Returns the left: where the rotation takes +Y.
    [[nodiscard]] constexpr Vector3 left() const noexcept;

    /// Returns the up: where the rotation takes +Z.
    [[nodiscard]] constexpr Vector3 up() const noexcept;

    /// Composes two rotations into one that applies `b` first and then `a`, as in mathematics.
    ///
    /// The result is renormalised unless it is already of unit length to within float rounding,
    /// so composing with the identity is exact.
    ///
    /// \param a The rotation applied second.
    /// \param b The rotation applied first.
    /// \return The rotation `a b`.
    [[nodiscard]] friend Rotation operator*(const Rotation& a, const Rotation& b) noexcept {
        return Rotation((b.vector_ * a.scalar_) + (a.vector_ * b.scalar_) +
                            cross(a.vector_, b.vector_),
                        (a.scalar_ * b.scalar_) - dot(a.vector_, b.vector_))
            .renormalized();
    }

  private:
    friend constexpr Rotation inverse(const Rotation& r) noexcept;
    friend constexpr Vector3 rotate(const Rotation& r, const Vector3& v) noexcept;
    friend bool approx_equal(const Rotation& a, const Rotation& b, Angle tolerance) noexcept;

    /// Makes the rotation of a quaternion.
    ///
    /// \param vector The quaternion's vector part.
    /// \param scalar The quaternion's scalar part.
    constexpr Rotation(const Vector3& vector, float scalar) noexcept
        : vector_(vector), scalar_(scalar) {}

    /// Returns the rotation about an axis by an angle, with no check of the axis.
    ///
    /// \param unit_axis The axis to turn about, of unit length.
    /// \param angle How far to turn.
    /// \return The rotation, whose quaternion is as far from unit length as the axis is.
    [[nodiscard]] static Rotation about_unit_axis(const Vector3& unit_axis, Angle angle) noexcept {
        const auto half = angle / 2.0F;
        return {unit_axis * sin(half), cos(half)};
    }

    /// Returns the same rotation with its quaternion scaled back to unit length.
    ///
    /// A quaternion whose squared length is already within two float epsilons of 1 comes back as
    /// it is. Rounding a normalised quaternion to float leaves it within half that, so composing
    /// with the identity stays exact, while a rotated vector still keeps its length to within a
    /// few parts in ten million.
    [[nodiscard]] Rotation renormalized() const noexcept {
        const auto x = static_cast<double>(vector_.x);
        const auto y = static_cast<double>(vector_.y);
        const auto z = static_cast<double>(vector_.z);
        const auto w = static_cast<double>(scalar_);
        const auto norm_squared = (x * x) + (y * y) + (z * z) + (w * w);
        if (std::abs(norm_squared - 1.0) <= 2.0 * double{std::numeric_limits<float>::epsilon()}) {
            return *this;
        }
        const auto scale = 1.0 / std::sqrt(norm_squared);
        const Vector3 vector{
            .x = static_cast<float>(x * scale),
            .y = static_cast<float>(y * scale),
            .z = static_cast<float>(z * scale),
        };
        return {vector, static_cast<float>(w * scale)};
    }

    /// The quaternion's vector part: the axis of the turn, scaled by the sine of half its angle.
    Vector3 vector_;

    /// The quaternion's scalar part: the cosine of half the angle of the turn.
    float scalar_ = 1.0F;
};

/// Returns the rotation that undoes a rotation.
///
/// \param r The rotation.
/// \return The rotation that turns everything back, so `inverse(r) * r` is the identity.
[[nodiscard]] constexpr Rotation inverse(const Rotation& r) noexcept {
    return {-r.vector_, r.scalar_};
}

/// Rotates a vector.
///
/// \param r The rotation.
/// \param v The vector, a direction or a point about the origin.
/// \return The vector turned by the rotation, with its length kept.
[[nodiscard]] constexpr Vector3 rotate(const Rotation& r, const Vector3& v) noexcept {
    // v + 2w cross(u, v) + 2 cross(u, cross(u, v)), for the quaternion w + u.
    const auto t = cross(r.vector_, v) * 2.0F;
    return v + (t * r.scalar_) + cross(r.vector_, t);
}

/// Tells whether two rotations are within an angle of each other.
///
/// A quaternion and its negation are the same rotation, and this treats them so.
///
/// \param a The first rotation.
/// \param b The second rotation.
/// \param tolerance The greatest angle of the rotation that takes one to the other.
/// \return Whether turning from `a` to `b` takes at most `tolerance`.
[[nodiscard]] inline bool approx_equal(const Rotation& a, const Rotation& b,
                                       Angle tolerance) noexcept {
    // The quaternion of the rotation from a to b, inverse(a) * b, is w + u with w = cos(t / 2) and
    // |u| = sin(t / 2) for the angle t between them. Taking |w| reads -q as q.
    const auto w = (a.scalar_ * b.scalar_) + dot(a.vector_, b.vector_);
    const auto u = (b.vector_ * a.scalar_) - (a.vector_ * b.scalar_) - cross(a.vector_, b.vector_);
    return Angle::radians(2.0F * std::atan2(length(u), std::abs(w))) <= tolerance;
}

constexpr Vector3 Rotation::forward() const noexcept { return rotate(*this, {.x = 1.0F}); }

constexpr Vector3 Rotation::left() const noexcept { return rotate(*this, {.y = 1.0F}); }

constexpr Vector3 Rotation::up() const noexcept { return rotate(*this, {.z = 1.0F}); }

/// Returns the yaw, pitch and roll that make a rotation, the inverse of
/// `Rotation::from_yaw_pitch_roll`.
///
/// Pitch is from -90 to 90 degrees, and yaw and roll are above -180 degrees and at most 180. Facing
/// straight up or down, yaw and roll turn about the same axis, so the roll is 0 there and the yaw
/// holds the whole turn.
///
/// \param r The rotation.
/// \return Angles that make the same rotation.
[[nodiscard]] inline YawPitchRoll to_yaw_pitch_roll(const Rotation& r) noexcept {
    // The angle whose tangent is y / x, from above -180 degrees up to 180: an arctangent that
    // gives 180 degrees rather than -180.
    const auto arctangent = [](float y, float x) {
        constexpr auto half_turn = std::numbers::pi_v<float>;
        const auto radians = std::atan2(y, x);
        return Angle::radians(radians == -half_turn ? half_turn : radians);
    };

    const auto facing = r.forward();
    const auto left = r.left();
    // The facing's horizontal length is the cosine of the pitch. Below this, the facing is within
    // 0.0006 degrees of straight up or down, closer than float rounding can place it from a yaw.
    const auto horizontal = std::hypot(facing.x, facing.y);
    if (horizontal <= 1e-5F) {
        // With no roll, a yaw alone turns +Y to (-sin yaw, cos yaw, 0).
        return {
            .yaw = arctangent(-left.x, left.y),
            .pitch = Angle::degrees(facing.z > 0.0F ? 90.0F : -90.0F),
            .roll = Angle{},
        };
    }

    // Undoing the yaw and then the pitch leaves the left turned only by the roll, about +X, to
    // (0, cos roll, sin roll). Taking the roll from the left in this way, rather than from the up,
    // keeps it consistent with the yaw even close to straight up or down.
    const auto cos_yaw = facing.x / horizontal;
    const auto sin_yaw = facing.y / horizontal;
    const auto left_ahead = (left.x * cos_yaw) + (left.y * sin_yaw);
    const auto left_aside = (left.y * cos_yaw) - (left.x * sin_yaw);
    const auto left_above = (left.z * horizontal) - (left_ahead * facing.z);
    return {
        .yaw = arctangent(facing.y, facing.x),
        .pitch = Angle::radians(std::atan2(facing.z, horizontal)),
        .roll = arctangent(left_above, left_aside),
    };
}

/// Returns a rotation as a matrix, which turns column vectors as the rotation does.
///
/// \param r The rotation.
/// \return The matrix whose first three columns are `r.forward()`, `r.left()` and `r.up()`, with
///         no translation.
[[nodiscard]] constexpr Matrix4 to_matrix(const Rotation& r) noexcept {
    const auto column = [](const Vector3& v) { return Vector4{.x = v.x, .y = v.y, .z = v.z}; };
    return {.columns = {column(r.forward()), column(r.left()), column(r.up()), Vector4{.w = 1.0F}}};
}

} // namespace wulfhilde
