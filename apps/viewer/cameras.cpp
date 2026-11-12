#include "cameras.hpp"

#include "command_line.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>

#include <expected>
#include <format>

namespace viewer {

wulfhilde::Camera camera_at_start(const wulfhilde::Map& map) {
    const auto& start = map.start();
    // The start position is where the feet are, upright under the sector's gravity, along its Z.
    return {
        .location =
            {
                .sector = start.location.sector,
                .point = start.location.point + wulfhilde::Vector3{.z = eye_height},
            },
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(start.yaw, start.pitch,
                                                                wulfhilde::Angle::degrees(0.0F)),
        .vertical_field_of_view = vertical_field_of_view,
    };
}

std::expected<wulfhilde::Camera, UsageError> camera_from_option(const wulfhilde::Map& map,
                                                                const CameraOption& option) {
    const auto sector = map.find_sector(option.sector);
    if (!sector) {
        return std::unexpected(UsageError{
            .code = UsageErrorCode::camera_outside_map,
            .message = std::format("--camera names no sector of the map: {}", option.sector),
        });
    }
    const wulfhilde::Location location{.sector = *sector, .point = option.point};
    if (!map.contains(location)) {
        return std::unexpected(UsageError{
            .code = UsageErrorCode::camera_outside_map,
            .message = std::format("--camera's point, {},{},{}, isn't in sector {}", option.point.x,
                                   option.point.y, option.point.z, option.sector),
        });
    }
    return wulfhilde::Camera{
        .location = location,
        .orientation =
            wulfhilde::Rotation::from_yaw_pitch_roll(option.yaw, option.pitch, option.roll),
        .vertical_field_of_view = vertical_field_of_view,
    };
}

} // namespace viewer
