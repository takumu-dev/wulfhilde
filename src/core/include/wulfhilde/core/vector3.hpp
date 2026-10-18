/// \file
/// A vector in three dimensions, the engine's points, directions and offsets.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>

#include <cmath>

namespace wulfhilde {

/// A vector in three dimensions, which stands for a point, a direction or an offset alike.
///
/// It is three packed floats, 12 bytes with no padding, so it can sit in a struct that a shader
/// reads under scalar layout. Whether a vector is a point or a direction is said by what is done
/// with it: `transform_point` moves a point, and `transform_direction` only turns a direction.
///
/// At the identity rotation, +X is the facing, +Y is to the left and +Z is up.
struct Vector3 {
    float x = 0.0F; ///< The component along the X axis.
    float y = 0.0F; ///< The component along the Y axis.
    float z = 0.0F; ///< The component along the Z axis.

    /// Adds a vector to this one.
    ///
    /// \param other The vector to add.
    /// \return This vector.
    constexpr Vector3& operator+=(const Vector3& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    /// Subtracts a vector from this one.
    ///
    /// \param other The vector to subtract.
    /// \return This vector.
    constexpr Vector3& operator-=(const Vector3& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    /// Multiplies this vector by a number.
    ///
    /// \param factor The number to multiply each component by.
    /// \return This vector.
    constexpr Vector3& operator*=(float factor) noexcept {
        x *= factor;
        y *= factor;
        z *= factor;
        return *this;
    }

    /// Divides this vector by a number.
    ///
    /// \param divisor The number to divide each component by.
    /// \return This vector.
    constexpr Vector3& operator/=(float divisor) noexcept {
        x /= divisor;
        y /= divisor;
        z /= divisor;
        return *this;
    }

    /// Compares two vectors exactly, component by component.
    constexpr bool operator==(const Vector3&) const noexcept = default;
};

/// Returns the sum of two vectors.
[[nodiscard]] constexpr Vector3 operator+(Vector3 a, const Vector3& b) noexcept { return a += b; }

/// Returns the difference of two vectors.
[[nodiscard]] constexpr Vector3 operator-(Vector3 a, const Vector3& b) noexcept { return a -= b; }

/// Returns the vector pointing the opposite way.
[[nodiscard]] constexpr Vector3 operator-(const Vector3& v) noexcept {
    return {.x = -v.x, .y = -v.y, .z = -v.z};
}

/// Returns a vector multiplied by a number.
[[nodiscard]] constexpr Vector3 operator*(Vector3 v, float factor) noexcept { return v *= factor; }

/// Returns a vector multiplied by a number.
[[nodiscard]] constexpr Vector3 operator*(float factor, Vector3 v) noexcept { return v *= factor; }

/// Returns a vector divided by a number.
[[nodiscard]] constexpr Vector3 operator/(Vector3 v, float divisor) noexcept {
    return v /= divisor;
}

/// Returns the dot product of two vectors.
///
/// \param a The first vector.
/// \param b The second vector.
/// \return The sum of the products of their components.
[[nodiscard]] constexpr float dot(const Vector3& a, const Vector3& b) noexcept {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

/// Returns the cross product of two vectors, which follows the right-hand rule: the cross product
/// of +X and +Y is +Z.
///
/// \param a The first vector.
/// \param b The second vector.
/// \return A vector at right angles to both, whose length is the area of the parallelogram they
///         span.
[[nodiscard]] constexpr Vector3 cross(const Vector3& a, const Vector3& b) noexcept {
    return {
        .x = (a.y * b.z) - (a.z * b.y),
        .y = (a.z * b.x) - (a.x * b.z),
        .z = (a.x * b.y) - (a.y * b.x),
    };
}

/// Returns the square of a vector's length, which needs no square root.
///
/// \param v The vector.
/// \return The square of its length.
[[nodiscard]] constexpr float length_squared(const Vector3& v) noexcept { return dot(v, v); }

/// Returns a vector's length.
///
/// \param v The vector.
/// \return Its length.
[[nodiscard]] inline float length(const Vector3& v) noexcept {
    return std::sqrt(length_squared(v));
}

/// Returns the distance between two points.
///
/// \param a The first point.
/// \param b The second point.
/// \return The length of the offset between them.
[[nodiscard]] inline float distance(const Vector3& a, const Vector3& b) noexcept {
    return length(b - a);
}

/// Returns a vector scaled to unit length, keeping its direction.
///
/// \param v The vector, which must not be zero, nor so short that the square of its length rounds
///          to zero.
/// \return The vector of length 1 in the same direction.
[[nodiscard]] inline Vector3 normalize(const Vector3& v) {
    const auto squared = length_squared(v);
    WULFHILDE_ASSERT(squared > 0.0F, "a vector to normalise isn't zero");
    return v / std::sqrt(squared);
}

/// Interpolates linearly between two vectors.
///
/// \param a The vector at `t` = 0, which is returned exactly.
/// \param b The vector at `t` = 1, which is returned exactly.
/// \param t How far to go from `a` to `b`. Outside [0, 1], it extrapolates.
/// \return The vector that far between them.
[[nodiscard]] constexpr Vector3 lerp(const Vector3& a, const Vector3& b, float t) noexcept {
    return (a * (1.0F - t)) + (b * t);
}

/// Tells whether two vectors lie within a distance of each other.
///
/// \param a The first vector.
/// \param b The second vector.
/// \param tolerance The greatest distance between them.
/// \return Whether the distance between `a` and `b` is at most `tolerance`.
[[nodiscard]] constexpr bool approx_equal(const Vector3& a, const Vector3& b,
                                          float tolerance) noexcept {
    return length_squared(b - a) <= tolerance * tolerance;
}

/// Returns the angle between two vectors.
///
/// It is computed from both the cross and the dot products, so it stays accurate near 0 and 180
/// degrees, where an arccosine of the dot product loses its precision.
///
/// \param a The first vector, of any length.
/// \param b The second vector, of any length.
/// \return The angle between their directions, from 0 to 180 degrees. A zero vector has no
///         direction, and gives 0 or 180.
[[nodiscard]] inline Angle angle_between(const Vector3& a, const Vector3& b) noexcept {
    return Angle::radians(std::atan2(length(cross(a, b)), dot(a, b)));
}

} // namespace wulfhilde
