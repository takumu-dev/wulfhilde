#include "cameras.hpp"

#include "command_line.hpp"
#include "controller.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>

#include <expected>
#include <format>
#include <string>

namespace viewer {
namespace {

/// Writes a number rounded to a number of decimal places, without the zeros that end it or a sign
/// on zero.
std::string rounded(float value, int places) {
    auto text = std::format("{:.{}f}", value, places);
    if (text.contains('.')) {
        while (text.ends_with('0')) {
            text.pop_back();
        }
        if (text.ends_with('.')) {
            text.pop_back();
        }
    }
    return text == "-0" ? "0" : text;
}

} // namespace

wulfhilde::Camera camera_at(const Eye& eye) noexcept {
    return {
        .location = eye.location,
        .orientation = eye.facing,
        .vertical_field_of_view = vertical_field_of_view,
    };
}

std::string camera_option(const wulfhilde::Map& map, const Eye& eye) {
    const auto& point = eye.location.point;
    const auto facing = wulfhilde::to_yaw_pitch_roll(eye.facing);
    auto option =
        std::format("{},{},{},{},{},{}", map.sector(eye.location.sector).id, rounded(point.x, 3),
                    rounded(point.y, 3), rounded(point.z, 3), rounded(facing.yaw.in_degrees(), 2),
                    rounded(facing.pitch.in_degrees(), 2));
    if (const auto roll = rounded(facing.roll.in_degrees(), 2); roll != "0") {
        option += std::format(",{}", roll);
    }
    return option;
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
