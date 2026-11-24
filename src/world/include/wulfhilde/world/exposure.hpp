/// \file
/// The exposures a map is seen at.

#pragma once

namespace wulfhilde {

/// The exposures a map is seen at, as a camera's EV at ISO 100: the range that automatic exposure
/// stays within, and a compensation on top of it.
///
/// A higher EV100 takes in less light, so suits a brighter place: about 15 for a sunlit yard, and
/// about 5 for a room lit indoors. The default range, 1 to 16, takes in both.
struct Exposure {
    /// The lowest exposure, for the darkest places, in EV100.
    float min = 1.0F;

    /// The highest exposure, for the brightest places, in EV100, no lower than `min`.
    float max = 16.0F;

    /// How many stops brighter than the metered exposure the map is seen, or darker below 0.
    float compensation = 0.0F;

    /// Compares two exposures exactly.
    constexpr bool operator==(const Exposure&) const noexcept = default;
};

} // namespace wulfhilde
