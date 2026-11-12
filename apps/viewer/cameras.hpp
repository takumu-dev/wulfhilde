#pragma once

#include "command_line.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/world/map.hpp>

#include <expected>

namespace viewer {

/// How high the eye is above the feet, in metres.
inline constexpr float eye_height = 1.65F;

/// The angle the viewer's frames span from bottom to top.
inline constexpr wulfhilde::Angle vertical_field_of_view = wulfhilde::Angle::degrees(60.0F);

/// Returns the camera at the eye of someone standing at the map's start position, facing as it
/// faces.
///
/// \param map The map.
[[nodiscard]] wulfhilde::Camera camera_at_start(const wulfhilde::Map& map);

/// Places the camera that `--camera` names in the map.
///
/// \param map The map.
/// \param option The camera, as the command line names it.
/// \return The camera, or the usage error of a sector the map doesn't have, or of a point outside
///         its sector.
[[nodiscard]] std::expected<wulfhilde::Camera, UsageError>
camera_from_option(const wulfhilde::Map& map, const CameraOption& option);

} // namespace viewer
