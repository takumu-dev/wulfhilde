/// \file
/// What a body's move through a map came to.

#pragma once

#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector3.hpp>

#include <optional>

namespace wulfhilde {

/// What a body's move through a map came to: where it ended, how it was moving then, and what it
/// stands on.
struct Movement {
    /// Where the body ended, and how it is tilted and which way its gravity pulls there.
    Pose pose;

    /// The body's velocity at the end, clipped by what it met, in the coordinates of the sector its
    /// centre ended in, in metres per second.
    Vector3 velocity{};

    /// The combined transform of the portals the body's centre crossed, from its start's sector's
    /// coordinates into its end's. It is the identity when it crossed none.
    RigidTransform transform = RigidTransform::identity();

    /// How far instant steps lifted the body along its gravity up, in metres: negative when they
    /// lowered it. A game can ease its eye over them.
    float stepped = 0.0F;

    /// What the body stands on at the end, or nothing when it stands on nothing.
    std::optional<Ground> ground{};
};

} // namespace wulfhilde
