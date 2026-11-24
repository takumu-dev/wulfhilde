#include "controller.hpp"

#include <wulfhilde/collision/body_shape.hpp>
#include <wulfhilde/collision/collision.hpp>
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/trace.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>

namespace viewer {
namespace {

using wulfhilde::Angle;
using wulfhilde::Vector3;

/// The player's body: the size of a person, stepping up a stair and standing on a steep slope.
constexpr wulfhilde::BodyShape body_shape{
    .radius = 0.3F,
    .height = 1.8F,
    .step_height = 0.4F,
    .steepest_slope = Angle::degrees(45.0F),
};

/// How fast the body walks, and how fast it runs, in metres per second.
constexpr float walking_speed = 4.0F;
constexpr float running_speed = 8.0F;

/// How fast the body's speed across its gravity changes toward the speed the player asks for, on
/// ground and in the air, in metres per second squared. On ground, the body reaches walking speed,
/// or stops, in 0.1 s.
constexpr float ground_acceleration = 40.0F;
constexpr float air_acceleration = 8.0F;

/// How fast gravity speeds the body's fall, in metres per second squared: Earth's, so a fall
/// takes as long as it would from a real height.
constexpr float gravity = 9.81F;

/// How high a jump lifts the body, in metres.
constexpr float jump_height = 1.0F;

/// How fast a jump leaves the ground along the body's gravity up, in metres per second: the square
/// root of twice gravity times the jump's height, so gravity stops it at that height.
constexpr float jump_speed = 4.4294469F;
static_assert((jump_speed * jump_speed) - (2.0F * gravity * jump_height) < 1e-5F &&
              (2.0F * gravity * jump_height) - (jump_speed * jump_speed) < 1e-5F);

/// The body's greatest speed, in metres per second, in any direction, since a tilted portal can
/// turn a fall sideways. In one tick, it moves the body less than its radius.
constexpr float top_speed = 50.0F;

/// How high the eye is above the feet, in metres.
constexpr float eye_height = 1.65F;

/// How far short of a surface the eye's trace stops, in metres. The renderer's near plane reaches
/// 5 cm from the eye, and its corners about 1.55 times that, so a surface this far away is never
/// cut.
constexpr float eye_margin = 0.1F;

/// How far the eye looks up or down from the body's horizontal, at most. Straight up, its yaw
/// would turn it about its own facing.
constexpr Angle steepest_look = Angle::degrees(89.0F);

/// Returns the part of a vector across a direction of unit length.
Vector3 across(const Vector3& v, const Vector3& unit_direction) {
    return v - (unit_direction * dot(v, unit_direction));
}

/// Returns a vector moved toward a target by at most a distance.
Vector3 toward(const Vector3& from, const Vector3& to, float most) {
    const auto difference = to - from;
    const auto apart = length(difference);
    return apart <= most ? to : from + (difference * (most / apart));
}

/// Returns a velocity across gravity laid along the ground under it, keeping its direction seen
/// along gravity and its speed, so a slope neither slows the body nor lets it creep.
Vector3 along_ground(const Vector3& velocity, const wulfhilde::Ground& ground, const Vector3& up) {
    const auto speed = length(velocity);
    if (speed == 0.0F) {
        return velocity;
    }
    const auto laid = velocity - (up * (dot(velocity, ground.normal) / dot(up, ground.normal)));
    return laid * (speed / length(laid));
}

/// Returns the trace up from a location that places an eye: as far as the height, or the margin
/// short of a surface in the way.
wulfhilde::Trace traced_eye(const wulfhilde::Map& map, const wulfhilde::Location& from,
                            const Vector3& up, float height) {
    const auto reach = map.trace(from, up * (height + eye_margin));
    if (!reach.surface) {
        return map.trace(from, up * height);
    }
    // The trace runs straight in the start's coordinates, which the inverse of its portals'
    // transform takes its end back into, whichever portals it crossed.
    const auto reached =
        wulfhilde::distance(from.point, transform_point(inverse(reach.transform), reach.end.point));
    return map.trace(from, up * std::max(reached - eye_margin, 0.0F));
}

} // namespace

Controller Controller::at_start(const wulfhilde::Map& map, const ControllerSettings& settings) {
    const auto& start = map.start();
    const wulfhilde::Pose standing{
        .centre =
            {
                .sector = start.location.sector,
                .point = start.location.point + Vector3{.z = body_shape.height * 0.5F},
            },
    };
    const auto pitch = std::clamp(start.pitch, -steepest_look, steepest_look);
    Controller controller{settings, start.location, start.yaw, pitch, {}};
    controller.body_ = wulfhilde::settle(map, body_shape, standing);
    if (!controller.body_) {
        controller.viewpoint_ = traced_eye(map, start.location, {.z = 1.0F}, eye_height).end;
    } else {
        // A move of no time finds the ground the body stands on, without moving it.
        constexpr Vector3 still{};
        controller.ground_ =
            wulfhilde::move(map, body_shape, *controller.body_, still, 0.0F).ground;
    }
    return controller;
}

Controller Controller::at_camera(const wulfhilde::Location& location, Angle yaw, Angle pitch,
                                 Angle roll, const ControllerSettings& settings) {
    return {settings, location, yaw, pitch, roll};
}

Controller::Controller(const ControllerSettings& settings, const wulfhilde::Location& location,
                       Angle yaw, Angle pitch, Angle roll) noexcept
    : settings_(settings), viewpoint_(location), yaw_(yaw), pitch_(pitch), roll_(roll) {}

void Controller::look(Angle yaw, Angle pitch) noexcept {
    // The yaw is kept within a turn, where a float holds it most finely.
    constexpr auto turn = Angle::degrees(360.0F);
    yaw_ += yaw;
    yaw_ -= turn * std::round(yaw_ / turn);
    // A camera from the command line may look straight up or down, which only a tilt gives up.
    if (pitch != Angle{}) {
        pitch_ = std::clamp(pitch_ + pitch, -steepest_look, steepest_look);
    }
}

void Controller::tick(const wulfhilde::Map& map, const Actions& actions) {
    if (!body_) {
        return;
    }
    // Walking runs across the body's gravity, along the heading's axes laid flat without making
    // them whole again, so an axis that stands along gravity walks nowhere.
    const auto up = body_->gravity_up;
    const auto heading = body_->tilt * wulfhilde::Rotation::from_yaw_pitch_roll(yaw_, {}, {});
    auto asked = (across(heading.forward(), up) * actions.forward) +
                 (across(heading.left(), up) * actions.strafe);
    // Walking forward and sideways at once is no faster than either.
    if (const auto strength = std::hypot(actions.forward, actions.strafe); strength > 1.0F) {
        asked = asked / strength;
    }
    const auto wanted = asked * (actions.run ? running_speed : walking_speed);

    if (ground_) {
        velocity_ = toward(velocity_, along_ground(wanted, *ground_, up),
                           ground_acceleration * tick_seconds);
        if (actions.jump) {
            velocity_ = across(velocity_, up) + (up * jump_speed);
        }
    } else {
        const auto sideways = across(velocity_, up);
        velocity_ = toward(sideways, wanted, air_acceleration * tick_seconds) +
                    (velocity_ - sideways) - (up * (gravity * tick_seconds));
    }
    if (const auto speed = length(velocity_); speed > top_speed) {
        velocity_ = velocity_ * (top_speed / speed);
    }

    const auto movement = wulfhilde::move(map, body_shape, *body_, velocity_, tick_seconds);
    body_ = movement.pose;
    velocity_ = movement.velocity;
    ground_ = movement.ground;
}

void Controller::return_to_start(const wulfhilde::Map& map) { *this = at_start(map, settings_); }

Eye Controller::eye(const wulfhilde::Map& map) const {
    const auto look = wulfhilde::Rotation::from_yaw_pitch_roll(yaw_, pitch_, roll_);
    if (!body_) {
        return {.location = viewpoint_, .facing = look};
    }
    const auto up = body_->tilt.up();
    const auto traced = traced_eye(map, body_->centre, up, eye_height - (body_shape.height * 0.5F));
    return {.location = traced.end, .facing = traced.transform.rotation * body_->tilt * look};
}

std::string unfit_start_warning(const wulfhilde::Map& map) {
    const auto& start = map.start().location;
    return std::format("the body, {} m in radius and {} m tall, doesn't fit at the start position, "
                       "({}, {}, {}) in sector {}, so the viewer looks from there without a body",
                       body_shape.radius, body_shape.height, start.point.x, start.point.y,
                       start.point.z, map.sector(start.sector).id);
}

} // namespace viewer
