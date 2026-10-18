/// \file
/// A plane, such as the plane of a portal.

#pragma once

#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector3.hpp>

namespace wulfhilde {

/// A plane in three dimensions, with a side its normal points to.
///
/// It holds the points p where `dot(normal, p) == distance`. The plane of a portal is one, which
/// traces, collision and the renderer's clipping all use.
struct Plane {
    /// The direction the plane faces, of unit length.
    Vector3 normal{};

    /// How far the plane lies from the origin along its normal, negative when the origin is on
    /// the side the normal points to.
    float distance = 0.0F;

    /// Compares two planes exactly.
    constexpr bool operator==(const Plane&) const noexcept = default;
};

/// Returns how far a point lies from a plane, on the side the plane's normal points to.
///
/// \param plane The plane.
/// \param point The point.
/// \return The distance from the plane to the point, positive on the side the normal points to,
///         negative on the other and zero on the plane.
[[nodiscard]] constexpr float signed_distance(const Plane& plane, const Vector3& point) noexcept {
    return dot(plane.normal, point) - plane.distance;
}

/// Moves a plane by a transform.
///
/// \param t The transform.
/// \param plane The plane.
/// \return The plane that holds the points `t` takes from `plane`, facing where `t` turns its
///         normal.
[[nodiscard]] constexpr Plane transform(const RigidTransform& t, const Plane& plane) noexcept {
    const auto normal = transform_direction(t, plane.normal);
    return {.normal = normal, .distance = plane.distance + dot(normal, t.translation)};
}

} // namespace wulfhilde
