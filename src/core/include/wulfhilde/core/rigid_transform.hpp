/// \file
/// A rigid transform, a rotation followed by a translation, such as a portal transform.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>

#include <array>

namespace wulfhilde {

/// A rigid transform: a rotation followed by a translation, which moves a shape without changing
/// its size or handedness.
///
/// It maps a point p to `rotate(rotation, p) + translation`. A portal transform is one, from its
/// own sector's coordinates into the far sector's. Transforms compose in mathematical order:
/// `a * b` applies `b` first, so crossing portal P1 and then portal P2 is `P2 * P1`.
///
/// A vector is moved with `transform_point` or only turned with `transform_direction`; there is no
/// `t * v`, which wouldn't say which.
struct RigidTransform {
    Rotation rotation = Rotation::identity(); ///< The turn, applied first.
    Vector3 translation{};                    ///< The movement, applied after the turn.

    /// Returns the identity transform, which leaves every point where it is.
    [[nodiscard]] static constexpr RigidTransform identity() noexcept { return {}; }
};

/// Composes two transforms into one that applies `b` first and then `a`, as in mathematics.
///
/// Composing with the identity is exact.
///
/// \param a The transform applied second.
/// \param b The transform applied first.
/// \return The transform `a b`.
[[nodiscard]] inline RigidTransform operator*(const RigidTransform& a,
                                              const RigidTransform& b) noexcept {
    return {
        .rotation = a.rotation * b.rotation,
        .translation = rotate(a.rotation, b.translation) + a.translation,
    };
}

/// Moves a point by a transform: turns it, then translates it.
///
/// \param t The transform.
/// \param point The point.
/// \return Where the transform takes the point.
[[nodiscard]] constexpr Vector3 transform_point(const RigidTransform& t,
                                                const Vector3& point) noexcept {
    return rotate(t.rotation, point) + t.translation;
}

/// Turns a direction by a transform, which a translation doesn't change.
///
/// \param t The transform.
/// \param direction The direction, or any vector that isn't a point, such as a velocity.
/// \return The direction turned by the transform's rotation.
[[nodiscard]] constexpr Vector3 transform_direction(const RigidTransform& t,
                                                    const Vector3& direction) noexcept {
    return rotate(t.rotation, direction);
}

/// Returns the transform that undoes a transform.
///
/// \param t The transform.
/// \return The transform that takes every point back, so `inverse(t) * t` is the identity.
[[nodiscard]] constexpr RigidTransform inverse(const RigidTransform& t) noexcept {
    const auto turn_back = inverse(t.rotation);
    return {.rotation = turn_back, .translation = -rotate(turn_back, t.translation)};
}

/// Tells whether two transforms are within an angle and a distance of each other.
///
/// \param a The first transform.
/// \param b The second transform.
/// \param rotation_tolerance The greatest angle of the rotation that takes one's rotation to the
///                           other's.
/// \param translation_tolerance The greatest distance between their translations.
/// \return Whether both are within their tolerances.
[[nodiscard]] inline bool approx_equal(const RigidTransform& a, const RigidTransform& b,
                                       Angle rotation_tolerance,
                                       float translation_tolerance) noexcept {
    return approx_equal(a.rotation, b.rotation, rotation_tolerance) &&
           approx_equal(a.translation, b.translation, translation_tolerance);
}

/// Returns a transform as a matrix, which applies it to a point (`w` = 1) or a direction (`w` = 0)
/// in homogeneous coordinates.
///
/// \param t The transform.
/// \return The matrix of the rotation, with the translation in its last column.
[[nodiscard]] constexpr Matrix4 to_matrix(const RigidTransform& t) noexcept {
    auto m = to_matrix(t.rotation);
    std::get<3>(m.columns) =
        Vector4{.x = t.translation.x, .y = t.translation.y, .z = t.translation.z, .w = 1.0F};
    return m;
}

} // namespace wulfhilde
