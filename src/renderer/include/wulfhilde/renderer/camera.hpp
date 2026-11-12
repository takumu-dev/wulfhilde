/// \file
/// A camera: where an image of a map is seen from.

#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/world/location.hpp>

namespace wulfhilde {

/// Where an image of a map is seen from, which way it looks, and how wide it sees.
///
/// The image's centre lies along the orientation's `forward()`, with its `up()` toward the top of
/// the image and its `left()` toward the left edge.
struct Camera {
    /// Where the camera is: a sector of the map, and the eye's point in that sector's coordinates.
    Location location;

    /// Which way the camera looks, in its sector's coordinates.
    Rotation orientation = Rotation::identity();

    /// The angle the image spans from its bottom edge to its top edge, more than 0° and less than
    /// 180°. Its width follows from its aspect ratio.
    Angle vertical_field_of_view = Angle::degrees(60.0F);
};

} // namespace wulfhilde
