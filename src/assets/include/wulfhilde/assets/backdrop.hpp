/// \file
/// A backdrop: an image of everything infinitely far away, as its asset describes it.

#pragma once

#include <cstddef>

namespace wulfhilde {

/// An image of everything infinitely far away in every direction, with its own brightness, which
/// skies show, as a backdrop's asset describes it.
///
/// Its image is equirectangular: its centre column faces +X, moving left in it turns anticlockwise
/// toward +Y, and its top row is straight up. A texel of the image shines at `nits` times its
/// colour, in linear Rec.709, unless a sky gives nits of its own.
struct Backdrop {
    /// The luminance of white, in nits (candelas per square metre).
    float nits = 0.0F;

    /// The index of the backdrop's image in `MapAssets::images`, read as a colour.
    std::size_t image = 0;

    /// Compares two backdrops exactly.
    bool operator==(const Backdrop&) const = default;
};

} // namespace wulfhilde
