/// \file
/// A sector of a map, as the map holds it.

#pragma once

#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/sky.hpp>
#include <wulfhilde/world/surface.hpp>

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace wulfhilde {

/// What covers a floor or a ceiling: a surface's material, with u and v along X and Y on a flat one
/// and along the hinge and up the slope on a sloped one, or a sky.
using Cover = std::variant<Surface, Sky>;

/// How a floor or a ceiling slopes, as a map holds it: from a hinge along one of its sector's
/// walls, where it lies at its height, it rises square to the wall, into the sector, at a gradient.
struct Slope {
    /// The wall it is hinged on, a wall of the same sector.
    WallId hinge;

    /// How far it rises for each metre square to the hinge, into the sector, in metres. A negative
    /// gradient makes it fall.
    float gradient = 0.0F;

    /// Compares two slopes exactly.
    constexpr bool operator==(const Slope&) const noexcept = default;
};

/// A sector's floor or ceiling, as a map holds it: the height it lies at, how it slopes, and what
/// covers it.
///
/// It is a plane, which `Map::floor_height` and `Map::ceiling_height` give the height of anywhere.
struct FloorOrCeiling {
    /// The height it lies at, in metres: everywhere, if it is flat, or along its hinge, if it
    /// slopes.
    float height = 0.0F;

    /// How it slopes, or nothing for a flat floor or ceiling.
    std::optional<Slope> slope{};

    /// What covers it.
    Cover cover;
};

/// A sector of a map: a region bounded by an outer loop of walls and any inner loops around holes
/// in it, with a floor below and a ceiling above. Its down points toward its floor.
///
/// A map holds it, and every reference in it is to that map's objects.
struct Sector {
    /// The sector's name, unique in its map, as the map's description gives it.
    std::string id{};

    /// The sector's floor.
    FloorOrCeiling floor;

    /// The sector's ceiling.
    FloorOrCeiling ceiling;

    /// The sector's loops, each the IDs of its walls in order: first its outer loop, which runs
    /// anticlockwise seen from above, then any inner loops, which run clockwise.
    std::vector<std::vector<WallId>> loops{};

    /// The sector's ambient light: its own, or else its map's, or else black.
    AmbientLight ambient{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};
};

} // namespace wulfhilde
