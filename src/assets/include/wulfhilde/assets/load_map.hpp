/// \file
/// Loading a map from its file.

#pragma once

#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_load_error.hpp>

#include <expected>
#include <string_view>

namespace wulfhilde {

/// The extension of a map's file, which follows the map's asset path, as in `maps/e1m1.map.json`.
inline constexpr std::string_view map_extension = ".map.json";

/// Loads a map from its file, a strict JSON file ending `.map.json`, and makes the map.
///
/// A load finds problems in phases, so that no problem is a knock-on of another, and reports every
/// problem of the first phase that finds any:
///
/// 1. the file's syntax: the first place where it stops being JSON, since nothing after it can be
///    read with any certainty;
/// 2. its structure: first whether it is a map of this version of the engine, and if it is, every
///    value of the wrong kind, missing member, unknown member and duplicate key;
/// 3. the map's own rules, which `Map::create` checks.
///
/// Each problem has the JSON pointer of the value it lies in, and that value's line and column in
/// the file. A problem `Map::create` finds at a value the file never wrote, such as one left out
/// for its default, takes the position of the nearest value on the way to it that the file did
/// write. The problems come in the order of their positions.
///
/// \param root The content root that holds the map.
/// \param path The map's asset path.
/// \return The map, or the file's name and every problem of the first phase that found any.
[[nodiscard]] std::expected<Map, MapLoadError> load_map(const ContentRoot& root,
                                                        std::string_view path);

} // namespace wulfhilde
