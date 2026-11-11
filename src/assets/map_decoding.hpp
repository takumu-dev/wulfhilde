#pragma once

#include "json_file.hpp"
#include <wulfhilde/world/map_description.hpp>

#include <optional>

namespace wulfhilde::detail {

/// Decodes a map's file into the map's description, reporting every structural problem in it.
///
/// A file that isn't a map of this version of the engine is reported as that alone, since nothing
/// else about it would mean anything.
///
/// \param file The file.
/// \return The description, which is complete only if no problem was reported, or nothing if the
///         file isn't a map of this version.
[[nodiscard]] std::optional<MapDescription> decode_map(JsonFile& file);

} // namespace wulfhilde::detail
