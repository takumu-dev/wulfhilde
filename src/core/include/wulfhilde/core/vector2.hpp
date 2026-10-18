/// \file
/// A vector in two dimensions, such as a point of a sector's loop seen from above.

#pragma once

#include <wulfhilde/core/assertion.hpp>

#include <cmath>

namespace wulfhilde {

/// A vector in two dimensions, which stands for a point, a direction or an offset alike.
///
/// It is two packed floats, 8 bytes with no padding, so it can sit in a struct that a shader reads
/// under scalar layout.
struct Vector2 {
    float x = 0.0F; ///< The component along the X axis.
    float y = 0.0F; ///< The component along the Y axis.

    /// Adds a vector to this one.
    ///
    /// \param other The vector to add.
    /// \return This vector.
    constexpr Vector2& operator+=(const Vector2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    /// Subtracts a vector from this one.
    ///
    /// \param other The vector to subtract.
    /// \return This vector.
    constexpr Vector2& operator-=(const Vector2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    /// Multiplies this vector by a number.
    ///
    /// \param factor The number to multiply each component by.
    /// \return This vector.
    constexpr Vector2& operator*=(float factor) noexcept {
        x *= factor;
        y *= factor;
        return *this;
    }

    /// Divides this vector by a number.
    ///
    /// \param divisor The number to divide each component by.
    /// \return This vector.
    constexpr Vector2& operator/=(float divisor) noexcept {
        x /= divisor;
        y /= divisor;
        return *this;
    }

    /// Compares two vectors exactly, component by component.
    constexpr bool operator==(const Vector2&) const noexcept = default;
};

/// Returns the sum of two vectors.
[[nodiscard]] constexpr Vector2 operator+(Vector2 a, const Vector2& b) noexcept { return a += b; }

/// Returns the difference of two vectors.
[[nodiscard]] constexpr Vector2 operator-(Vector2 a, const Vector2& b) noexcept { return a -= b; }

/// Returns the vector pointing the opposite way.
[[nodiscard]] constexpr Vector2 operator-(const Vector2& v) noexcept {
    return {.x = -v.x, .y = -v.y};
}

/// Returns a vector multiplied by a number.
[[nodiscard]] constexpr Vector2 operator*(Vector2 v, float factor) noexcept { return v *= factor; }

/// Returns a vector multiplied by a number.
[[nodiscard]] constexpr Vector2 operator*(float factor, Vector2 v) noexcept { return v *= factor; }

/// Returns a vector divided by a number.
[[nodiscard]] constexpr Vector2 operator/(Vector2 v, float divisor) noexcept {
    return v /= divisor;
}

/// Returns the dot product of two vectors.
///
/// \param a The first vector.
/// \param b The second vector.
/// \return The sum of the products of their components.
[[nodiscard]] constexpr float dot(const Vector2& a, const Vector2& b) noexcept {
    return (a.x * b.x) + (a.y * b.y);
}

/// Returns the perp-dot product of two vectors, the Z component of their cross product as if they
/// lay in the XY plane.
///
/// Seen from above, it is positive when `b` turns left from `a`, anticlockwise, negative when it
/// turns right, and zero when they are parallel. Its size is the area of the parallelogram they
/// span.
///
/// \param a The first vector.
/// \param b The second vector.
/// \return `a.x * b.y - a.y * b.x`.
[[nodiscard]] constexpr float perp_dot(const Vector2& a, const Vector2& b) noexcept {
    return (a.x * b.y) - (a.y * b.x);
}

/// Returns the square of a vector's length, which needs no square root.
///
/// \param v The vector.
/// \return The square of its length.
[[nodiscard]] constexpr float length_squared(const Vector2& v) noexcept { return dot(v, v); }

/// Returns a vector's length.
///
/// \param v The vector.
/// \return Its length.
[[nodiscard]] inline float length(const Vector2& v) noexcept {
    return std::sqrt(length_squared(v));
}

/// Returns the distance between two points.
///
/// \param a The first point.
/// \param b The second point.
/// \return The length of the offset between them.
[[nodiscard]] inline float distance(const Vector2& a, const Vector2& b) noexcept {
    return length(b - a);
}

/// Returns a vector scaled to unit length, keeping its direction.
///
/// \param v The vector, which must not be zero, nor so short that the square of its length rounds
///          to zero.
/// \return The vector of length 1 in the same direction.
[[nodiscard]] inline Vector2 normalize(const Vector2& v) {
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
[[nodiscard]] constexpr Vector2 lerp(const Vector2& a, const Vector2& b, float t) noexcept {
    return (a * (1.0F - t)) + (b * t);
}

/// Tells whether two vectors lie within a distance of each other.
///
/// \param a The first vector.
/// \param b The second vector.
/// \param tolerance The greatest distance between them.
/// \return Whether the distance between `a` and `b` is at most `tolerance`.
[[nodiscard]] constexpr bool approx_equal(const Vector2& a, const Vector2& b,
                                          float tolerance) noexcept {
    return length_squared(b - a) <= tolerance * tolerance;
}

} // namespace wulfhilde
