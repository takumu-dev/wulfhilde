/// \file
/// The opening of a wall portal at a point of its wall, through which the sector beyond is seen and
/// reached.

#pragma once

namespace wulfhilde {

/// The part of a wall portal through which the sector beyond is seen and reached, at one point of
/// the wall: where the two sectors' spans from floor to ceiling overlap there.
///
/// Below it is the wall's lower strip, down to its sector's floor, and above it its upper strip, up
/// to its sector's ceiling. Where the spans don't overlap, the opening is empty, with its bottom at
/// its top, and the strips cover the whole wall. Along a wall beside a sloped floor or ceiling, the
/// opening's bottom and top change from point to point, as `WallFace` draws them.
struct Opening {
    /// The height of the opening's bottom, in metres: the higher of the two floors, but no higher
    /// than the wall's own ceiling.
    float bottom = 0.0F;

    /// The height of the opening's top, in metres: the lower of the two ceilings, but no lower than
    /// the opening's bottom.
    float top = 0.0F;

    /// Compares two openings exactly.
    constexpr bool operator==(const Opening&) const noexcept = default;
};

} // namespace wulfhilde
