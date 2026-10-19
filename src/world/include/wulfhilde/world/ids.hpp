/// \file
/// The typed IDs of a map's objects.

#pragma once

#include <wulfhilde/core/id.hpp>

namespace wulfhilde {

/// The ID of one of a map's sectors: its index among the map's sectors, in the order the map's
/// description lists them.
using SectorId = Id<struct SectorTag>;

/// The ID of one of a map's walls, unique across the map: its index among all the map's walls, in
/// the order the map's description lists them, sector by sector and loop by loop.
using WallId = Id<struct WallTag>;

/// The ID of one of the materials a map's surfaces carry. Each distinct asset path gets one, in the
/// order the map's description first names it.
using MaterialId = Id<struct MaterialTag>;

} // namespace wulfhilde
