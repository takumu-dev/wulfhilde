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
#include <wulfhilde/world/opening.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
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

/// The nearest points of a capsule's axis and something it may touch.
struct Nearest {
    Vector3 on_axis{};  ///< The point on the axis.
    Vector3 on_other{}; ///< The point on the other.
};

/// Returns the direction a wall faces, into its sector, which lies to its left, of unit length.
[[nodiscard]] Vector2 inward_of(const Vector2& start, const Vector2& end) {
    return normalize(Vector2{.x = start.y - end.y, .y = end.x - start.x});
}

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
///
/// \param nearest Set to the nearest points of the axis and the wall, when they touch, at the
///                height of the axis there.
[[nodiscard]] std::optional<Contact> wall_contact(const Axis& axis, float radius, WallId id,
                                                  const Vector2& start, const Vector2& end,
                                                  Nearest& nearest) {
    const auto inward = inward_of(start, end);
    const auto a = plan(axis.bottom);
    const auto b = plan(axis.top);
    const auto a_side = dot(a - start, inward);
    const auto b_side = dot(b - start, inward);
    if (a_side < 0.0F && b_side < 0.0F) {
        return std::nullopt;
    }
    // Returns the point of the axis whose plan is a point of the axis's plan.
    const auto lifted = [&axis, &a, &b](const Vector2& point) {
        const auto run = b - a;
        const auto squared = length_squared(run);
        const auto along = squared == 0.0F ? 0.5F : dot(point - a, run) / squared;
        return axis.bottom + ((axis.top - axis.bottom) * along);
    };
    if (segments_meet(a, b, start, end)) {
        const auto crossing = lifted(a + ((b - a) * (a_side / (a_side - b_side))));
        nearest = {.on_axis = crossing, .on_other = crossing};
        return Contact{
            .surface = id,
            .normal = {.x = inward.x, .y = inward.y},
            .depth = radius - std::min(a_side, b_side),
        };
    }
    const auto points = nearest_points(a, b, start, end);
    const auto apart = points.on_first - points.on_second;
    const auto distance = length(apart);
    if (distance >= radius + skin) {
        return std::nullopt;
    }
    const auto on_axis = lifted(points.on_first);
    nearest = {
        .on_axis = on_axis,
        .on_other = {.x = points.on_second.x, .y = points.on_second.y, .z = on_axis.z},
    };
    const auto normal = distance > 0.0F ? apart / distance : inward;
    return Contact{
        .surface = id,
        .normal = {.x = normal.x, .y = normal.y},
        .depth = radius - distance,
    };
}

/// Returns the part of an axis whose height lies within a range, or nothing if none of it does.
[[nodiscard]] std::optional<Axis> clip_axis(const Axis& axis, float low, float high) {
    const auto rise = axis.top.z - axis.bottom.z;
    if (rise == 0.0F) {
        return low <= axis.bottom.z && axis.bottom.z <= high ? std::optional(axis) : std::nullopt;
    }
    // How far along the axis, from its bottom at 0 to its top at 1, each height is.
    const auto at_low = (low - axis.bottom.z) / rise;
    const auto at_high = (high - axis.bottom.z) / rise;
    const auto from = std::max(std::min(at_low, at_high), 0.0F);
    const auto to = std::min(std::max(at_low, at_high), 1.0F);
    if (from > to) {
        return std::nullopt;
    }
    const auto along = axis.top - axis.bottom;
    return Axis{.bottom = axis.bottom + (along * from), .top = axis.bottom + (along * to)};
}

/// Returns the nearest points of two segments, from a to b and from c to d.
[[nodiscard]] Nearest nearest_points(const Vector3& a, const Vector3& b, const Vector3& c,
                                     const Vector3& d) noexcept {
    const auto first = b - a;
    const auto second = d - c;
    const auto apart = a - c;
    const auto first_squared = length_squared(first);
    const auto second_squared = length_squared(second);
    const auto along_second = dot(second, apart);
    // Each segment's parameter, from 0 at its start to 1 at its end, as in Ericson's Real-Time
    // Collision Detection, 5.1.9.
    auto s = 0.0F;
    auto t = 0.0F;
    if (first_squared == 0.0F && second_squared == 0.0F) {
        return {.on_axis = a, .on_other = c};
    }
    if (first_squared == 0.0F) {
        t = std::clamp(along_second / second_squared, 0.0F, 1.0F);
    } else {
        const auto along_first = dot(first, apart);
        if (second_squared == 0.0F) {
            s = std::clamp(-along_first / first_squared, 0.0F, 1.0F);
        } else {
            const auto both = dot(first, second);
            const auto denominator = (first_squared * second_squared) - (both * both);
            if (denominator != 0.0F) {
                s = std::clamp(((both * along_second) - (along_first * second_squared)) /
                                   denominator,
                               0.0F, 1.0F);
            }
            t = ((both * s) + along_second) / second_squared;
            if (t < 0.0F) {
                t = 0.0F;
                s = std::clamp(-along_first / first_squared, 0.0F, 1.0F);
            } else if (t > 1.0F) {
                t = 1.0F;
                s = std::clamp((both - along_first) / first_squared, 0.0F, 1.0F);
            }
        }
    }
    return {.on_axis = a + (first * s), .on_other = c + (second * t)};
}

/// A wall, or part of one, as an upright rectangle that faces into its sector.
struct Panel {
    Vector2 start{};     ///< Where it starts, seen from above.
    Vector2 end{};       ///< Where it ends, seen from above.
    float bottom = 0.0F; ///< The height of its bottom.
    float top = 0.0F;    ///< The height of its top.
};

/// Returns the contact of the capsule with a panel of a wall, met from its sector's side only.
///
/// \param nearest Set to the nearest points of the axis and the panel, when they touch.
[[nodiscard]] std::optional<Contact> panel_contact(const Axis& axis, float radius,
                                                   const SurfaceRef& surface, const Panel& panel,
                                                   Nearest& nearest) {
    const auto inward = inward_of(panel.start, panel.end);
    const auto a_side = dot(plan(axis.bottom) - panel.start, inward);
    const auto b_side = dot(plan(axis.top) - panel.start, inward);
    if (a_side < 0.0F && b_side < 0.0F) {
        return std::nullopt;
    }
    const Vector3 normal{.x = inward.x, .y = inward.y};
    const auto run = panel.end - panel.start;
    const auto run_squared = length_squared(run);
    // Whether a point of the panel's plane lies within the panel.
    const auto inside = [&](const Vector3& point) {
        const auto along = dot(plan(point) - panel.start, run) / run_squared;
        return 0.0F <= along && along <= 1.0F && panel.bottom <= point.z && point.z <= panel.top;
    };
    if ((a_side <= 0.0F) != (b_side <= 0.0F)) {
        const auto crossing =
            axis.bottom + ((axis.top - axis.bottom) * (a_side / (a_side - b_side)));
        if (inside(crossing)) {
            nearest = {.on_axis = crossing, .on_other = crossing};
            return Contact{
                .surface = surface,
                .normal = normal,
                .depth = radius - std::min(a_side, b_side),
            };
        }
    }

    const auto corner = [](const Vector2& point, float height) {
        return Vector3{.x = point.x, .y = point.y, .z = height};
    };
    const std::array edges{
        std::pair{corner(panel.start, panel.bottom), corner(panel.end, panel.bottom)},
        std::pair{corner(panel.start, panel.top), corner(panel.end, panel.top)},
        std::pair{corner(panel.start, panel.bottom), corner(panel.start, panel.top)},
        std::pair{corner(panel.end, panel.bottom), corner(panel.end, panel.top)},
    };
    // The nearest points of the axis and the panel's edges, or of an end of the axis and the
    // point of the panel it lies square to.
    auto best = nearest_points(axis.bottom, axis.top, edges.front().first, edges.front().second);
    const auto consider = [&best](const Nearest& candidate) {
        if (length_squared(candidate.on_axis - candidate.on_other) <
            length_squared(best.on_axis - best.on_other)) {
            best = candidate;
        }
    };
    for (const auto& [from, to] : edges) {
        consider(nearest_points(axis.bottom, axis.top, from, to));
    }
    for (const auto& [end, side] : {std::pair{axis.bottom, a_side}, std::pair{axis.top, b_side}}) {
        const auto onto = end - (normal * side);
        if (inside(onto)) {
            consider({.on_axis = end, .on_other = onto});
        }
    }
    const auto apart = best.on_axis - best.on_other;
    const auto distance = length(apart);
    if (distance >= radius + skin) {
        return std::nullopt;
    }
    nearest = best;
    return Contact{
        .surface = surface,
        .normal = distance > 0.0F ? apart / distance : normal,
        .depth = radius - distance,
    };
}

/// A sector that a body reaches through openings from the sector its centre is in.
struct Reach {
    /// The sector.
    SectorId sector;

    /// The walls whose openings the body comes through, in order from its centre's sector.
    std::vector<WallId> through{};

    /// The part of the body's axis that lies within every one of those openings.
    Axis share{};
};

/// Tells whether a straight path from one point to another comes through the openings of walls in
/// order, crossing each wall's plane from its sector's side within its opening. Its edges are in
/// it, with a skin's room for rounding.
[[nodiscard]] bool comes_through(const Map& map, const std::vector<WallId>& walls,
                                 const Vector3& from, const Vector3& to) {
    return std::ranges::all_of(walls, [&](WallId id) {
        const auto& wall = map.wall(id);
        const auto inward = inward_of(wall.start, wall.end);
        const auto from_side = dot(plan(from) - wall.start, inward);
        const auto to_side = dot(plan(to) - wall.start, inward);
        if (from_side < 0.0F || to_side > 0.0F || (from_side == 0.0F && to_side == 0.0F)) {
            return false;
        }
        const auto crossing = from + ((to - from) * (from_side / (from_side - to_side)));
        const auto run = wall.end - wall.start;
        const auto slack = skin / length(run);
        const auto along = dot(plan(crossing) - wall.start, run) / length_squared(run);
        const auto opening = map.opening(id).value_or(Opening{});
        return -slack <= along && along <= 1.0F + slack && opening.bottom - skin <= crossing.z &&
               crossing.z <= opening.top + skin;
    });
}

/// Adds a contact of a surface of a sector the body reaches, if the body touches it there.
///
/// A far sector's surface touches the body only where the body comes through the openings to it.
void add_contact(const Map& map, const Reach& reach, const std::optional<Contact>& contact,
                 const Nearest& nearest, std::vector<Contact>& contacts) {
    if (contact && comes_through(map, reach.through, nearest.on_axis, nearest.on_other)) {
        contacts.push_back(*contact);
    }
}

/// Adds the contacts of the strips of a side of a portal in a sector the body reaches, and reaches
/// the sector beyond if the body comes through its opening, unless it is reached already.
void reach_through(const Map& map, const BodyShape& shape, const Reach& reach, WallId id,
                   std::vector<Reach>& reached, std::vector<Contact>& contacts) {
    const auto& wall = map.wall(id);
    const auto& link = wall.link;
    const auto opening = map.opening(id);
    if (!link || !opening ||
        std::ranges::contains(reached, map.wall(link->wall).sector, &Reach::sector)) {
        return;
    }
    const auto& sector = map.sector(wall.sector);
    for (const auto& [bottom, top] : {
             std::pair{sector.floor.height, opening->bottom},
             std::pair{opening->top, sector.ceiling.height},
         }) {
        if (bottom < top) {
            Nearest nearest;
            const auto contact = panel_contact(
                reach.share, shape.radius, id,
                {.start = wall.start, .end = wall.end, .bottom = bottom, .top = top}, nearest);
            add_contact(map, reach, contact, nearest, contacts);
        }
    }

    const auto share = clip_axis(reach.share, opening->bottom, opening->top);
    if (!share || opening->bottom >= opening->top) {
        return;
    }
    Nearest nearest;
    const Panel open{
        .start = wall.start,
        .end = wall.end,
        .bottom = opening->bottom,
        .top = opening->top,
    };
    if (panel_contact(*share, shape.radius, id, open, nearest) &&
        comes_through(map, reach.through, nearest.on_axis, nearest.on_other)) {
        auto through = reach.through;
        through.push_back(id);
        reached.push_back({
            .sector = map.wall(link->wall).sector,
            .through = std::move(through),
            .share = *share,
        });
    }
}

/// Returns every surface that a body overlaps or touches: those of its centre's sector, and those
/// of each sector it reaches through openings, as far as it reaches, where it reaches them.
[[nodiscard]] std::vector<Contact> contacts_of(const Map& map, const BodyShape& shape,
                                               const Pose& pose) {
    const auto axis = axis_of(shape, pose);
    std::vector<Contact> contacts;
    std::vector<Reach> reached{{.sector = pose.centre.sector, .share = axis}};
    // Each sector is tested once, through the first openings it is reached through.
    for (std::size_t next = 0; next < reached.size(); ++next) {
        const auto reach = reached.at(next);
        for (const auto& loop : map.sector(reach.sector).loops) {
            for (const auto id : loop) {
                const auto& wall = map.wall(id);
                if (wall.link) {
                    reach_through(map, shape, reach, id, reached, contacts);
                    continue;
                }
                Nearest nearest;
                const auto contact =
                    wall_contact(reach.share, shape.radius, id, wall.start, wall.end, nearest);
                add_contact(map, reach, contact, nearest, contacts);
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

/// Moves a body's centre, carrying it into the sector beyond any opening it crosses.
void move_centre(const Map& map, Pose& pose, const Vector3& motion) {
    pose.centre = map.trace(pose.centre, motion).end;
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
        move_centre(map, pose, push);
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
        move_centre(map, movement.pose, movement.velocity * substep_seconds);
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
