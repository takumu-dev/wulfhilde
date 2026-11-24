#pragma once

#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>

#include <optional>
#include <string>

namespace viewer {

/// What the player asks the body to do, held for one tick.
struct Actions {
    /// How hard to walk forward, from -1, straight back, to 1.
    float forward = 0.0F;

    /// How hard to walk to the left, from -1, straight to the right, to 1.
    float strafe = 0.0F;

    /// Whether to run rather than walk.
    bool run = false;

    /// Whether to jump, which the body does from ground only, and again each time it lands while
    /// this is held.
    bool jump = false;
};

/// Where the player sees from, and which way.
struct Eye {
    /// Where the eye is.
    wulfhilde::Location location;

    /// Which way the eye faces, in the coordinates of its sector.
    wulfhilde::Rotation facing = wulfhilde::Rotation::identity();
};

/// The player's choices of how the controller feels, which it holds from when it is made.
struct ControllerSettings {};

/// Turns the player's input into the movement of their body and the facing of its eye.
///
/// It is a value that holds no map: the map comes with each call that needs one, so it may be
/// copied, and a copy goes its own way. The body's shape and how the body moves are the
/// controller's own.
///
/// Without a body, it is a viewpoint: the eye looks about, movement does nothing, and returning to
/// the start tries to place a body there again.
class Controller {
  public:
    /// How long one tick lasts, in seconds: the body moves in fixed ticks of 240 a second, so it
    /// moves the same however fast frames are drawn.
    static constexpr float tick_seconds = 1.0F / 240.0F;

    /// Makes the controller of a body standing at the map's start position, facing as it faces, or
    /// a viewpoint at its eye if the body doesn't fit there.
    ///
    /// \param map The map.
    /// \param settings The player's choices.
    /// \return The controller.
    [[nodiscard]] static Controller at_start(const wulfhilde::Map& map,
                                             const ControllerSettings& settings = {});

    /// Makes the controller of a viewpoint, without a body, at a location and a facing.
    ///
    /// \param location Where the eye is.
    /// \param yaw How far the facing turns left from +X.
    /// \param pitch How far it then tilts up.
    /// \param roll How far it then tilts its right side down.
    /// \param settings The player's choices.
    /// \return The controller.
    [[nodiscard]] static Controller at_camera(const wulfhilde::Location& location,
                                              wulfhilde::Angle yaw, wulfhilde::Angle pitch,
                                              wulfhilde::Angle roll,
                                              const ControllerSettings& settings = {});

    /// Turns the eye, relative to the body: left about the body's up and up about its left. A tilt
    /// stops within 89 degrees of the body's horizontal, while a camera looking further keeps its
    /// pitch until the eye tilts.
    ///
    /// \param yaw How far to turn left.
    /// \param pitch How far to tilt up.
    void look(wulfhilde::Angle yaw, wulfhilde::Angle pitch) noexcept;

    /// Moves the body through one tick, as the player asks. A viewpoint has no body to move.
    ///
    /// \param map The map, the one the body is in.
    /// \param actions What the player asks, for this tick.
    void tick(const wulfhilde::Map& map, const Actions& actions);

    /// Places the body back at the map's start position, still and facing as the start faces, or
    /// makes a viewpoint at its eye if the body doesn't fit there.
    ///
    /// \param map The map.
    void return_to_start(const wulfhilde::Map& map);

    /// Returns where the player sees from, and which way.
    ///
    /// A body's eye is above its centre along its up, traced there so that it stops short of any
    /// surface in its way.
    ///
    /// \param map The map, the one the body is in.
    /// \return The eye.
    [[nodiscard]] Eye eye(const wulfhilde::Map& map) const;

    /// Returns the body's pose, or nothing for a viewpoint.
    [[nodiscard]] const std::optional<wulfhilde::Pose>& body() const noexcept { return body_; }

    /// Returns whether the body stands on ground, which a viewpoint never does.
    [[nodiscard]] bool grounded() const noexcept { return ground_.has_value(); }

  private:
    /// Makes the controller of a viewpoint.
    Controller(const ControllerSettings& settings, const wulfhilde::Location& location,
               wulfhilde::Angle yaw, wulfhilde::Angle pitch, wulfhilde::Angle roll) noexcept;

    /// The player's choices.
    ControllerSettings settings_;

    /// The body, or nothing for a viewpoint.
    std::optional<wulfhilde::Pose> body_;

    /// Where a viewpoint's eye is. A body's eye follows the body instead.
    wulfhilde::Location viewpoint_;

    /// How far the eye is turned left, relative to the body.
    wulfhilde::Angle yaw_;

    /// How far the eye is tilted up, relative to the body.
    wulfhilde::Angle pitch_;

    /// How far a viewpoint's eye is tilted with its right side down. A body's eye is never rolled
    /// relative to the body.
    wulfhilde::Angle roll_;

    /// How fast the body moves and which way, in the coordinates of its sector, in metres per
    /// second.
    wulfhilde::Vector3 velocity_;

    /// What the body stands on, if anything.
    std::optional<wulfhilde::Ground> ground_;
};

/// Returns the warning the viewer gives when its body doesn't fit at the map's start position,
/// naming the start position and the body's size.
///
/// \param map The map.
/// \return The warning, as one line with no line break.
[[nodiscard]] std::string unfit_start_warning(const wulfhilde::Map& map);

} // namespace viewer
