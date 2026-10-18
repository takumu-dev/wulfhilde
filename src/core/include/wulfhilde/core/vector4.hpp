/// \file
/// A vector in four dimensions, the column of a matrix and a point in homogeneous coordinates.

#pragma once

#include <wulfhilde/core/assertion.hpp>

#include <cmath>

namespace wulfhilde {

/// A vector in four dimensions: a column of a `Matrix4`, or a point (`w` = 1) or direction
/// (`w` = 0) in homogeneous coordinates.
///
/// It is four packed floats, 16 bytes with no padding, so it can sit in a struct that a shader
/// reads under scalar layout.
struct Vector4 {
    float x = 0.0F; ///< The first component.
    float y = 0.0F; ///< The second component.
    float z = 0.0F; ///< The third component.
    float w = 0.0F; ///< The fourth component.

    /// Adds a vector to this one.
    ///
    /// \param other The vector to add.
    /// \return This vector.
    constexpr Vector4& operator+=(const Vector4& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    /// Subtracts a vector from this one.
    ///
    /// \param other The vector to subtract.
    /// \return This vector.
    constexpr Vector4& operator-=(const Vector4& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    /// Multiplies this vector by a number.
    ///
    /// \param factor The number to multiply each component by.
    /// \return This vector.
    constexpr Vector4& operator*=(float factor) noexcept {
        x *= factor;
        y *= factor;
        z *= factor;
        w *= factor;
        return *this;
    }

    /// Divides this vector by a number.
    ///
    /// \param divisor The number to divide each component by.
    /// \return This vector.
    constexpr Vector4& operator/=(float divisor) noexcept {
        x /= divisor;
        y /= divisor;
        z /= divisor;
        w /= divisor;
        return *this;
    }

    /// Compares two vectors exactly, component by component.
    constexpr bool operator==(const Vector4&) const noexcept = default;
};

/// Returns the sum of two vectors.
[[nodiscard]] constexpr Vector4 operator+(Vector4 a, const Vector4& b) noexcept { return a += b; }

/// Returns the difference of two vectors.
[[nodiscard]] constexpr Vector4 operator-(Vector4 a, const Vector4& b) noexcept { return a -= b; }

/// Returns the vector pointing the opposite way.
[[nodiscard]] constexpr Vector4 operator-(const Vector4& v) noexcept {
    return {.x = -v.x, .y = -v.y, .z = -v.z, .w = -v.w};
}

/// Returns a vector multiplied by a number.
[[nodiscard]] constexpr Vector4 operator*(Vector4 v, float factor) noexcept { return v *= factor; }

/// Returns a vector multiplied by a number.
[[nodiscard]] constexpr Vector4 operator*(float factor, Vector4 v) noexcept { return v *= factor; }

/// Returns a vector divided by a number.
[[nodiscard]] constexpr Vector4 operator/(Vector4 v, float divisor) noexcept {
    return v /= divisor;
}

/// Returns the dot product of two vectors.
///
/// \param a The first vector.
/// \param b The second vector.
/// \return The sum of the products of their components.
[[nodiscard]] constexpr float dot(const Vector4& a, const Vector4& b) noexcept {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
}

/// Returns the square of a vector's length, which needs no square root.
///
/// \param v The vector.
/// \return The square of its length.
[[nodiscard]] constexpr float length_squared(const Vector4& v) noexcept { return dot(v, v); }

/// Returns a vector's length.
///
/// \param v The vector.
/// \return Its length.
[[nodiscard]] inline float length(const Vector4& v) noexcept {
    return std::sqrt(length_squared(v));
}

/// Returns the distance between two vectors.
///
/// \param a The first vector.
/// \param b The second vector.
/// \return The length of the offset between them.
[[nodiscard]] inline float distance(const Vector4& a, const Vector4& b) noexcept {
    return length(b - a);
}

/// Returns a vector scaled to unit length, keeping its direction.
///
/// \param v The vector, which must not be zero, nor so short that the square of its length rounds
///          to zero.
/// \return The vector of length 1 in the same direction.
[[nodiscard]] inline Vector4 normalize(const Vector4& v) {
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
[[nodiscard]] constexpr Vector4 lerp(const Vector4& a, const Vector4& b, float t) noexcept {
    return (a * (1.0F - t)) + (b * t);
}

/// Tells whether two vectors lie within a distance of each other.
///
/// \param a The first vector.
/// \param b The second vector.
/// \param tolerance The greatest distance between them.
/// \return Whether the distance between `a` and `b` is at most `tolerance`.
[[nodiscard]] constexpr bool approx_equal(const Vector4& a, const Vector4& b,
                                          float tolerance) noexcept {
    return length_squared(b - a) <= tolerance * tolerance;
}

} // namespace wulfhilde
