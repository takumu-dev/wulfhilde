// A game's smallest use of the engine, built by each consumer project: a checked narrowing, a
// message to the default log sink, and the math types, which live wholly in their headers.
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/id.hpp>
#include <wulfhilde/core/log_sink.hpp>
#include <wulfhilde/core/narrow.hpp>
#include <wulfhilde/core/plane.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

int main() {
    const wulfhilde::Id<struct SectorTag> sector{wulfhilde::narrow<std::uint32_t>(std::size_t{12})};
    wulfhilde::log(wulfhilde::LogSeverity::info, "a game linked the engine");

    // Through a portal that turns a quarter left, a facing along +X comes out along +Y, beyond the
    // far side's plane.
    const wulfhilde::RigidTransform portal{
        .rotation =
            wulfhilde::Rotation::from_yaw_pitch_roll(wulfhilde::Angle::degrees(90.0F), {}, {}),
    };
    const auto facing = wulfhilde::transform_direction(portal, {.x = 1.0F});
    const wulfhilde::Plane far_side{.normal = {.y = 1.0F}};
    const auto warm = wulfhilde::Color::from_temperature(2700.0F);
    const bool turned = wulfhilde::approx_equal(facing, {.y = 1.0F}, 1e-6F) &&
                        wulfhilde::signed_distance(far_side, facing) > 0.0F &&
                        wulfhilde::perp_dot({.x = 1.0F}, {.x = facing.x, .y = facing.y}) > 0.0F;

    return sector.index() == 12 && turned && warm.r > 1.0F ? EXIT_SUCCESS : EXIT_FAILURE;
}
