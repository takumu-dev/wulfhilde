#pragma once

#include "command_line.hpp"
#include "controller.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/world/map.hpp>

#include <expected>
#include <string>

namespace viewer {

/// The angle the viewer's frames span from bottom to top.
inline constexpr wulfhilde::Angle vertical_field_of_view = wulfhilde::Angle::degrees(60.0F);

/// Returns the camera at an eye, facing as it faces.
///
/// \param eye The eye.
[[nodiscard]] wulfhilde::Camera camera_at(const Eye& eye) noexcept;

/// Writes an eye as the camera that `--camera` takes, `<sector>,<x>,<y>,<z>,<yaw>,<pitch>`, and
/// `,<roll>` if it has one. Its point is rounded to the millimetre and its facing to the hundredth
/// of a degree.
///
/// \param map The map that the eye is in.
/// \param eye The eye.
/// \return The camera, without `--camera` before it.
[[nodiscard]] std::string camera_option(const wulfhilde::Map& map, const Eye& eye);

/// Places the camera that `--camera` names in the map.
///
/// \param map The map.
/// \param option The camera, as the command line names it.
/// \return The camera, or the usage error of a sector the map doesn't have, or of a point outside
///         its sector.
[[nodiscard]] std::expected<wulfhilde::Camera, UsageError>
camera_from_option(const wulfhilde::Map& map, const CameraOption& option);

} // namespace viewer
