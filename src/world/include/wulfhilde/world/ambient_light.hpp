/// \file
/// A sector's ambient light.

#pragma once

#include <wulfhilde/core/color.hpp>

namespace wulfhilde {

/// A sector's ambient light: its base light, arriving equally from every direction and the same
/// everywhere in the sector, on top of any lights.
///
/// The default is black, which a sector has when neither it nor its map sets one.
struct AmbientLight {
    /// The light's colour in linear Rec.709, which filters white.
    Color color{};

    /// The illuminance the light gives a surface facing any way, in lux.
    float lux = 0.0F;

    /// Compares two ambient lights exactly.
    constexpr bool operator==(const AmbientLight&) const noexcept = default;
};

} // namespace wulfhilde
