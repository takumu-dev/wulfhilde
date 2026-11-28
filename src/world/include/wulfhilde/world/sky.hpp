/// \file
/// What a sky of a map shows.

#pragma once

#include <wulfhilde/world/ids.hpp>

#include <optional>

namespace wulfhilde {

/// A floor or ceiling that shows an infinitely distant backdrop instead of a material, as a map
/// holds it.
///
/// It mirrors `SkyDescription`, with the backdrop's asset path interned as an ID, which
/// `Map::backdrops` turns back into the path. The backdrop is fixed to the sky's sector's
/// coordinates.
struct Sky {
    /// The backdrop the sky shows.
    BackdropId backdrop;

    /// The luminance of the backdrop's white, in nits, or nothing to take the backdrop's own.
    std::optional<float> nits{};

    /// Compares two skies exactly.
    constexpr bool operator==(const Sky&) const noexcept = default;
};

} // namespace wulfhilde
