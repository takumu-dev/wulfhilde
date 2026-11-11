/// \file
/// Loading the assets a map uses.

#pragma once

#include <wulfhilde/assets/asset_error.hpp>
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/world/map.hpp>

#include <expected>
#include <string_view>

namespace wulfhilde {

/// The extension of a material's file, which follows the material's asset path.
inline constexpr std::string_view material_extension = ".material.json";

/// The extension of an image's file, which follows the image's asset path.
inline constexpr std::string_view image_extension = ".png";

/// Loads every material a map's surfaces carry, and every image the materials read as textures.
///
/// A material is a strict JSON file ending `.material.json`, read under a map's rules. Each of its
/// files finds problems in phases, and reports every problem of the first phase that finds any:
/// its syntax; whether it is a material of this version of the engine; its structure and the range
/// of each value; and then its textures. An image is a PNG of palette, grey or RGB, with or without
/// alpha, at most 4,096 pixels on a side. A 16-bit image is narrowed to 8 bits per channel, and one
/// of fewer bits widened, and its colour space is chosen by the property that reads it, never by
/// the PNG's metadata.
///
/// Every problem in every asset is reported at once, and each asset that is missing or broken only
/// once. A missing asset is reported at the first place that refers to it: a missing image at its
/// texture in the material, and a missing material at the JSON pointer of the first surface of the
/// map that carries it, with no file or position, since a map doesn't know its file. A broken asset
/// is reported in its own file, naming that first place.
///
/// \param root The content root that holds the assets.
/// \param map The map.
/// \return The assets, or every problem found in them.
[[nodiscard]] std::expected<MapAssets, AssetError> load_map_assets(const ContentRoot& root,
                                                                   const Map& map);

} // namespace wulfhilde
