#include <wulfhilde/collision/collision.hpp>

#include <wulfhilde/collision/body_shape.hpp>
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/movement.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace wulfhilde {
namespace {

/// How near a body may come to a surface without touching it, in metres. A body pushed out of a
/// surface ends exactly against it, so rounding must not decide whether it still touches.
constexpr float skin = 1e-3F;

/// How much steeper than a body's steepest slope a surface may seem and still be its ground. A
/// surface exactly that steep is ground, and rounding in its normal or the body's gravity up must
/// not decide otherwise.
constexpr Angle slope_tolerance = Angle::radians(1e-4F);

/// How far a settled body may still overlap a surface, in metres, which only rounding leaves.
constexpr float fit_tolerance = 1e-4F;

/// The smallest radius of a body, in metres. Points in a map 4 km from its origin are only about
/// half a millimetre apart in float, so a smaller body couldn't be told from a point.
constexpr float least_radius = 1e-3F;

/// The longest move, in metres: twice the width of the largest map, whose coordinates lie within
/// 4,096 m of its origin. It also bounds a move's number of substeps.
constexpr float longest_move = 16384.0F;

/// How many times a push-out finds the body's contacts afresh and moves it out of them.
constexpr std::size_t push_out_passes = 8;

/// How many times one push-out sweeps over its contacts' planes.
constexpr std::size_t plane_sweeps = 16;

/// A surface the body overlaps or touches, as a plane on its centre.
struct Contact {
    /// The surface.
    SurfaceRef surface;

    /// The direction that pushes the body off the surface, of unit length.
    Vector3 normal{};

    /// How far the body overlaps the surface along the normal: negative when it is clear of it, by
    /// up to the skin.
    float depth = 0.0F;
};

/// Checks the preconditions that every body's shape and pose share.
void check_body(const BodyShape& shape, const Pose& pose) {
    WULFHILDE_ASSERT(shape.radius >= least_radius, "a body's radius is at least a millimetre");
    WULFHILDE_ASSERT(shape.height >= 2.0F * shape.radius,
                     "a body's height is at least twice its radius");
    WULFHILDE_ASSERT(shape.step_height >= 0.0F, "a body's step height is zero or more");
    WULFHILDE_ASSERT(Angle{} <= shape.steepest_slope &&
                         shape.steepest_slope <= Angle::degrees(90.0F),
                     "a body's steepest slope is from 0 to 90 degrees");
    WULFHILDE_ASSERT(std::abs(length_squared(pose.gravity_up) - 1.0F) <= 1e-4F,
                     "a body's gravity up is of unit length");
}

/// The capsule's axis: the centres of its two caps.
struct Axis {
    Vector3 bottom{}; ///< The centre of the cap toward the tilt's down.
    Vector3 top{};    ///< The centre of the cap toward the tilt's up.
};

/// Returns the axis of a body's capsule where its pose puts it.
[[nodiscard]] Axis axis_of(const BodyShape& shape, const Pose& pose) {
    const auto half = (pose.tilt.up() * ((shape.height * 0.5F) - shape.radius));
    return {.bottom = pose.centre.point - half, .top = pose.centre.point + half};
}

/// Returns a point seen from above.
[[nodiscard]] Vector2 plan(const Vector3& point) noexcept { return {.x = point.x, .y = point.y}; }

/// Returns the point of a segment nearest a point.
[[nodiscard]] Vector2 nearest_on_segment(const Vector2& start, const Vector2& end,
                                         const Vector2& point) noexcept {
    const auto run = end - start;
    const auto squared = length_squared(run);
    if (squared == 0.0F) {
        return start;
    }
    return start + (run * std::clamp(dot(point - start, run) / squared, 0.0F, 1.0F));
}

/// The nearest points of two segments that don't cross: one on each.
struct NearestPoints {
    Vector2 on_first{};  ///< The point on the first segment.
    Vector2 on_second{}; ///< The point on the second segment.
};

/// Returns the nearest points of two segments, from a to b and from c to d, that don't cross.
[[nodiscard]] NearestPoints nearest_points(const Vector2& a, const Vector2& b, const Vector2& c,
                                           const Vector2& d) noexcept {
    const std::array candidates{
        NearestPoints{.on_first = a, .on_second = nearest_on_segment(c, d, a)},
        NearestPoints{.on_first = b, .on_second = nearest_on_segment(c, d, b)},
        NearestPoints{.on_first = nearest_on_segment(a, b, c), .on_second = c},
        NearestPoints{.on_first = nearest_on_segment(a, b, d), .on_second = d},
    };
    return std::ranges::min(candidates, {}, [](const NearestPoints& points) {
        return length_squared(points.on_second - points.on_first);
    });
}

/// Tells whether two segments cross or touch.
[[nodiscard]] bool segments_meet(const Vector2& a, const Vector2& b, const Vector2& c,
                                 const Vector2& d) noexcept {
    const auto c_side = perp_dot(b - a, c - a);
    const auto d_side = perp_dot(b - a, d - a);
    const auto a_side = perp_dot(d - c, a - c);
    const auto b_side = perp_dot(d - c, b - c);
    return ((c_side <= 0.0F && d_side >= 0.0F) || (c_side >= 0.0F && d_side <= 0.0F)) &&
           ((a_side <= 0.0F && b_side >= 0.0F) || (a_side >= 0.0F && b_side <= 0.0F)) &&
           (c_side != 0.0F || d_side != 0.0F);
}

/// Returns the contact of the capsule with a wall, which stands upright from its sector's floor to
/// its ceiling, so the capsule meets it where their plans come within the radius.
///
/// A wall is met from its sector's side only: a capsule whose axis lies wholly behind it is
/// another wall's business.
[[nodiscard]] std::optional<Contact> wall_contact(const Axis& axis, float radius, WallId id,
                                                  const Vector2& start, const Vector2& end) {
    const auto inward = normalize(Vector2{.x = start.y - end.y, .y = end.x - start.x});
    const auto a = plan(axis.bottom);
    const auto b = plan(axis.top);
    const auto a_side = dot(a - start, inward);
    const auto b_side = dot(b - start, inward);
    if (a_side < 0.0F && b_side < 0.0F) {
        return std::nullopt;
    }
    if (segments_meet(a, b, start, end)) {
        return Contact{
            .surface = id,
            .normal = {.x = inward.x, .y = inward.y},
            .depth = radius - std::min(a_side, b_side),
        };
    }
    const auto nearest = nearest_points(a, b, start, end);
    const auto apart = nearest.on_first - nearest.on_second;
    const auto distance = length(apart);
    if (distance >= radius + skin) {
        return std::nullopt;
    }
    const auto normal = distance > 0.0F ? apart / distance : inward;
    return Contact{
        .surface = id,
        .normal = {.x = normal.x, .y = normal.y},
        .depth = radius - distance,
    };
}

/// Returns every surface of its sector that a body overlaps or touches.
[[nodiscard]] std::vector<Contact> contacts_of(const Map& map, const BodyShape& shape,
                                               const Pose& pose) {
    const auto axis = axis_of(shape, pose);
    std::vector<Contact> contacts;
    for (const auto& loop : map.sector(pose.centre.sector).loops) {
        for (const auto id : loop) {
            const auto& wall = map.wall(id);
            if (const auto contact = wall_contact(axis, shape.radius, id, wall.start, wall.end)) {
                contacts.push_back(*contact);
            }
        }
    }

    const auto floor = map.floor_height(pose.centre) + shape.radius;
    if (const auto depth = floor - std::min(axis.bottom.z, axis.top.z); depth > -skin) {
        contacts.push_back({
            .surface = SectorFloor{pose.centre.sector},
            .normal = {.z = 1.0F},
            .depth = depth,
        });
    }
    const auto ceiling = map.ceiling_height(pose.centre) - shape.radius;
    if (const auto depth = std::max(axis.bottom.z, axis.top.z) - ceiling; depth > -skin) {
        contacts.push_back({
            .surface = SectorCeiling{pose.centre.sector},
            .normal = {.z = -1.0F},
            .depth = depth,
        });
    }
    return contacts;
}

/// Pushes a body out of the surfaces it overlaps, as far as the overlaps allow.
void push_out(const Map& map, const BodyShape& shape, Pose& pose) {
    for (std::size_t pass = 0; pass < push_out_passes; ++pass) {
        const auto contacts = contacts_of(map, shape, pose);
        // Each overlap is a plane the centre must lie in front of. Sweeping over them, each
        // pushing the centre out of its own plane, settles on a point in front of them all.
        Vector3 push{};
        for (std::size_t sweep = 0; sweep < plane_sweeps; ++sweep) {
            for (const auto& contact : contacts) {
                const auto short_by = contact.depth - dot(contact.normal, push);
                if (short_by > 0.0F) {
                    push += contact.normal * short_by;
                }
            }
        }
        if (push == Vector3{}) {
            return;
        }
        pose.centre.point += push;
    }
}

/// Returns the ground among the contacts: the one least steep from the body's gravity up, if any is
/// no steeper than its steepest slope.
[[nodiscard]] std::optional<Ground> ground_among(const std::vector<Contact>& contacts,
                                                 const BodyShape& shape, const Pose& pose) {
    std::optional<Ground> ground;
    auto least_steep = shape.steepest_slope + slope_tolerance;
    for (const auto& contact : contacts) {
        const auto steepness = angle_between(contact.normal, pose.gravity_up);
        if (steepness <= least_steep) {
            least_steep = steepness;
            ground = Ground{.surface = contact.surface, .normal = contact.normal};
        }
    }
    return ground;
}

/// Returns a velocity with whatever would carry it into the planes of the contacts removed, so a
/// body slides along one surface, runs along the crease of two and stops against three.
[[nodiscard]] Vector3 clip(const Vector3& velocity, const std::vector<Contact>& contacts) {
    const auto clear_of_all = [&contacts](const Vector3& v) {
        return std::ranges::all_of(
            contacts, [&v](const Contact& contact) { return dot(v, contact.normal) >= -1e-6F; });
    };
    if (clear_of_all(velocity)) {
        return velocity;
    }
    for (const auto& contact : contacts) {
        const auto into = dot(velocity, contact.normal);
        const auto along = velocity - (contact.normal * into);
        if (into < 0.0F && clear_of_all(along)) {
            return along;
        }
    }
    for (std::size_t i = 0; i < contacts.size(); ++i) {
        for (auto j = i + 1; j < contacts.size(); ++j) {
            const auto crease = cross(contacts.at(i).normal, contacts.at(j).normal);
            if (length_squared(crease) < 1e-8F) {
                continue;
            }
            const auto direction = normalize(crease);
            const auto along = direction * dot(velocity, direction);
            if (clear_of_all(along)) {
                return along;
            }
        }
    }
    return {};
}

} // namespace

Movement move(const Map& map, const BodyShape& shape, const Pose& pose, const Vector3& velocity,
              float seconds) {
    check_body(shape, pose);
    WULFHILDE_ASSERT(seconds >= 0.0F, "a body moves for zero seconds or more");
    WULFHILDE_ASSERT(length(velocity) * seconds <= longest_move,
                     "a move is no longer than twice the width of the largest map");

    // Substeps of half the radius can't carry the body's axis past a wall in one go.
    const auto substep_length = shape.radius * 0.5F;
    const auto substeps =
        std::max(std::size_t{1},
                 static_cast<std::size_t>(std::ceil(length(velocity) * seconds / substep_length)));
    const auto substep_seconds = seconds / static_cast<float>(substeps);

    Movement movement{.pose = pose, .velocity = velocity};
    for (std::size_t substep = 0; substep < substeps; ++substep) {
        movement.pose.centre.point += movement.velocity * substep_seconds;
        push_out(map, shape, movement.pose);
        movement.velocity = clip(movement.velocity, contacts_of(map, shape, movement.pose));
    }
    movement.ground = ground_among(contacts_of(map, shape, movement.pose), shape, movement.pose);
    return movement;
}

std::optional<Pose> settle(const Map& map, const BodyShape& shape, const Pose& pose) {
    check_body(shape, pose);
    auto settled = pose;
    push_out(map, shape, settled);
    const auto overlaps =
        std::ranges::any_of(contacts_of(map, shape, settled),
                            [](const Contact& contact) { return contact.depth > fit_tolerance; });
    const auto pushed = distance(pose.centre.point, settled.centre.point);
    if (overlaps || pushed > (shape.radius / 3.0F) + fit_tolerance ||
        !map.contains(settled.centre)) {
        return std::nullopt;
    }
    return settled;
}

} // namespace wulfhilde
