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
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/opening.hpp>
#include <wulfhilde/world/surface_ref.hpp>
#include <wulfhilde/world/wall_face.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
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

/// How far rounding may move a height that a step's rules compare, in metres: a step exactly as
/// high as a body's step height is one it walks up, and an opening exactly as tall as the body is
/// one it fits in.
constexpr float height_tolerance = 1e-4F;

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

/// How far a body reaches along its gravity up, as heights along it.
struct Span {
    float feet = 0.0F; ///< The height of its lowest point.
    float top = 0.0F;  ///< The height of its highest point.
};

/// Returns how far a body reaches along its gravity up where its pose puts it.
[[nodiscard]] Span span_of(const BodyShape& shape, const Pose& pose) {
    const auto axis = axis_of(shape, pose);
    const auto bottom = dot(axis.bottom, pose.gravity_up);
    const auto top = dot(axis.top, pose.gravity_up);
    return {
        .feet = std::min(bottom, top) - shape.radius,
        .top = std::max(bottom, top) + shape.radius,
    };
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

/// Returns the contact of the capsule with a part of a wall, a convex polygon in the wall's upright
/// plane whose corners run anticlockwise as the wall is seen from its sector, met from that side
/// only.
///
/// \param nearest Set to the nearest points of the axis and the part, when they touch.
[[nodiscard]] std::optional<Contact>
part_contact(const Axis& axis, float radius, const SurfaceRef& surface, const Vector2& start,
             const Vector2& end, const std::vector<Vector3>& corners, Nearest& nearest) {
    const auto inward = inward_of(start, end);
    const auto a_side = dot(plan(axis.bottom) - start, inward);
    const auto b_side = dot(plan(axis.top) - start, inward);
    if (a_side < 0.0F && b_side < 0.0F) {
        return std::nullopt;
    }
    const Vector3 normal{.x = inward.x, .y = inward.y};
    const auto along_unit = normalize(end - start);
    // Where a point of the wall's plane lies on it: how far along it, and how high.
    const auto on_wall = [&](const Vector3& point) {
        return Vector2{.x = dot(plan(point) - start, along_unit), .y = point.z};
    };
    // Whether a point of the wall's plane lies within the part, whose corners run clockwise as
    // along and height run, with the part to the right of each edge.
    const auto inside = [&](const Vector3& point) {
        const auto p = on_wall(point);
        for (std::size_t i = 0; i < corners.size(); ++i) {
            const auto from = on_wall(corners.at(i));
            const auto to = on_wall(corners.at((i + 1) % corners.size()));
            if (perp_dot(to - from, p - from) > 0.0F) {
                return false;
            }
        }
        return true;
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

    // The nearest points of the axis and the part's edges, or of an end of the axis and the point
    // of the part it lies square to.
    auto best = nearest_points(axis.bottom, axis.top, corners.front(), corners.at(1));
    const auto consider = [&best](const Nearest& candidate) {
        if (length_squared(candidate.on_axis - candidate.on_other) <
            length_squared(best.on_axis - best.on_other)) {
            best = candidate;
        }
    };
    for (std::size_t i = 0; i < corners.size(); ++i) {
        consider(nearest_points(axis.bottom, axis.top, corners.at(i),
                                corners.at((i + 1) % corners.size())));
    }
    for (const auto& [tip, side] : {std::pair{axis.bottom, a_side}, std::pair{axis.top, b_side}}) {
        const auto onto = tip - (normal * side);
        if (inside(onto)) {
            consider({.on_axis = tip, .on_other = onto});
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
        const auto opening = map.opening(id, plan(crossing)).value_or(Opening{});
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

/// A strip of a side of a portal at a point of its wall, from the sector's floor or ceiling to the
/// opening's edge.
struct Strip {
    float bottom = 0.0F; ///< The height of its bottom.
    float top = 0.0F;    ///< The height of its top.
    float edge = 0.0F;   ///< The height of its edge along the opening: its top or its bottom.
};

/// Returns a strip of a side of a portal, and the opening beside it, at a point of its wall.
[[nodiscard]] std::pair<Strip, Opening> strip_at(const Map& map, WallId id, bool lower,
                                                 const Vector3& point) {
    const auto opening = map.opening(id, plan(point)).value_or(Opening{});
    const Location on_wall{.sector = map.wall(id).sector, .point = point};
    if (lower) {
        return {
            Strip{
                .bottom = map.floor_height(on_wall),
                .top = opening.bottom,
                .edge = opening.bottom,
            },
            opening,
        };
    }
    return {
        Strip{.bottom = opening.top, .top = map.ceiling_height(on_wall), .edge = opening.top},
        opening,
    };
}

/// Tells whether a body passes a strip beside an opening that it touches, by Build's rules on a
/// capsule, with heights measured along its gravity up at the point of the strip it touches.
///
/// A strip below the opening is a step, which the body walks up if its edge is at most the body's
/// step height above its feet and the body fits in the opening. The sector under its centre
/// decides its height, so it is lifted once its centre crosses. A strip above the opening whose
/// edge is no lower than the body's top can only graze it, as the body passes below.
[[nodiscard]] bool passes(const BodyShape& shape, const Pose& pose, const Opening& opening,
                          const Strip& strip, const Nearest& nearest) {
    const auto height_at = [&nearest, &pose](float z) {
        return dot(Vector3{.x = nearest.on_other.x, .y = nearest.on_other.y, .z = z},
                   pose.gravity_up);
    };
    const auto edge = height_at(strip.edge);
    const auto span = span_of(shape, pose);
    if (height_at((strip.bottom + strip.top) * 0.5F) <
        height_at((opening.bottom + opening.top) * 0.5F)) {
        return edge - span.feet <= shape.step_height + height_tolerance &&
               opening.top - opening.bottom >= shape.height - height_tolerance;
    }
    return edge >= span.top - height_tolerance;
}

/// Adds the contacts of the strips of a side of a portal in a sector the body reaches, unless the
/// body passes them, and reaches the sector beyond if the body comes through its opening, unless
/// it is reached already.
///
/// The edge of a strip along the opening is the edge on the portal's plane of the floor or the
/// ceiling beyond, so a cap reaching past the plane meets that surface there, and is pushed along
/// the line from the edge to the cap's centre.
void reach_through(const Map& map, const BodyShape& shape, const Pose& pose, const Reach& reach,
                   WallId id, std::vector<Reach>& reached, std::vector<Contact>& contacts) {
    const auto& wall = map.wall(id);
    const auto& link = wall.link;
    if (!link || std::ranges::contains(reached, map.wall(link->wall).sector, &Reach::sector)) {
        return;
    }
    const auto face = map.face(id);
    for (const auto& [corners, lower] :
         {std::pair{&face.lower, true}, std::pair{&face.upper, false}}) {
        if (corners->empty()) {
            continue;
        }
        Nearest nearest;
        const auto contact =
            part_contact(reach.share, shape.radius, id, wall.start, wall.end, *corners, nearest);
        if (!contact) {
            continue;
        }
        const auto [strip, opening] = strip_at(map, id, lower, nearest.on_other);
        if (!passes(shape, pose, opening, strip, nearest)) {
            add_contact(map, reach, contact, nearest, contacts);
        }
    }

    if (face.opening.empty()) {
        return;
    }
    const auto [lowest, highest] =
        std::ranges::minmax(face.opening | std::views::transform(&Vector3::z));
    const auto share = clip_axis(reach.share, lowest, highest);
    if (!share) {
        return;
    }
    Nearest nearest;
    if (part_contact(*share, shape.radius, id, wall.start, wall.end, face.opening, nearest) &&
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

/// Returns the contact of the capsule with a sector's floor or ceiling, a plane that carries on
/// beyond its walls, if it touches it, along with the nearest points of the axis and the plane.
[[nodiscard]] std::optional<Contact> plane_contact(const Map& map, const Axis& axis, float radius,
                                                   SectorId sector, bool floor, Nearest& nearest) {
    const auto normal = floor ? map.floor_normal(sector) : map.ceiling_normal(sector);
    // How far a point lies in front of the plane, square to it: its height above the floor or
    // below the ceiling, shortened by the plane's tilt.
    const auto in_front = [&](const Vector3& point) {
        const Location at{.sector = sector, .point = point};
        const auto gap = floor ? point.z - map.floor_height(at) : map.ceiling_height(at) - point.z;
        return gap * std::abs(normal.z);
    };
    const auto bottom = in_front(axis.bottom);
    const auto top = in_front(axis.top);
    const auto& end = bottom <= top ? axis.bottom : axis.top;
    const auto distance = std::min(bottom, top);
    if (radius - distance <= -skin) {
        return std::nullopt;
    }
    nearest = {.on_axis = end, .on_other = end - (normal * distance)};
    return Contact{
        .surface = floor ? SurfaceRef{SectorFloor{sector}} : SurfaceRef{SectorCeiling{sector}},
        .normal = normal,
        .depth = radius - distance,
    };
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
        // A floor or ceiling beyond an opening touches the body only where the body comes through
        // to it, which a flat one's point nearest the body, straight above or below it, never is.
        // The point a slope rising away from the opening comes nearest the body may be.
        if (next > 0) {
            for (const auto floor : {true, false}) {
                Nearest nearest;
                const auto contact =
                    plane_contact(map, reach.share, shape.radius, reach.sector, floor, nearest);
                add_contact(map, reach, contact, nearest, contacts);
            }
        }
        for (const auto& loop : map.sector(reach.sector).loops) {
            for (const auto id : loop) {
                const auto& wall = map.wall(id);
                if (wall.link) {
                    reach_through(map, shape, pose, reach, id, reached, contacts);
                    continue;
                }
                Nearest nearest;
                const auto contact =
                    wall_contact(reach.share, shape.radius, id, wall.start, wall.end, nearest);
                add_contact(map, reach, contact, nearest, contacts);
            }
        }
    }

    for (const auto floor : {true, false}) {
        Nearest nearest;
        if (const auto contact =
                plane_contact(map, axis, shape.radius, pose.centre.sector, floor, nearest)) {
            contacts.push_back(*contact);
        }
    }
    return contacts;
}

/// Moves a body's centre, carrying it into the sector beyond any opening it crosses.
void move_centre(const Map& map, Pose& pose, const Vector3& motion) {
    pose.centre = map.trace(pose.centre, motion).end;
}

/// Turns the contacts of a grounded body with surfaces too steep to be its ground, which face up
/// along its gravity, into walls upright to its gravity, so that walking into such a slope stops
/// the body, as a wall does, rather than lifting it off its ground or carrying it up the slope.
///
/// Each such contact's depth grows to what pushing the body out along its new normal takes: no more
/// than the move across gravity that carried the body into it.
void as_walls(std::vector<Contact>& contacts, const BodyShape& shape, const Pose& pose) {
    for (auto& contact : contacts) {
        const auto up = dot(contact.normal, pose.gravity_up);
        const auto level = contact.normal - (pose.gravity_up * up);
        const auto across = length(level);
        if (up > 0.0F && across > 0.0F &&
            angle_between(contact.normal, pose.gravity_up) >
                shape.steepest_slope + slope_tolerance) {
            contact.normal = level / across;
            contact.depth /= across;
        }
    }
}

/// Pushes a body out of the surfaces it overlaps, as far as the overlaps allow, returning the
/// contacts it pushed the body out of. A grounded body is pushed out of slopes too steep for it as
/// out of walls.
///
/// The body may end a little clear of a contact it was pushed out of. A contact's plane stands in
/// for a surface at the body as it was, and pushing the body off the edge of one turns the edge's
/// normal as it goes, so the push runs past the point where it touches.
[[nodiscard]] std::vector<Contact> push_out(const Map& map, const BodyShape& shape, Pose& pose,
                                            bool grounded) {
    std::vector<Contact> pushed;
    for (std::size_t pass = 0; pass < push_out_passes; ++pass) {
        auto contacts = contacts_of(map, shape, pose);
        if (grounded) {
            as_walls(contacts, shape, pose);
        }
        std::ranges::copy_if(contacts, std::back_inserter(pushed),
                             [](const Contact& contact) { return contact.depth > 0.0F; });
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
            break;
        }
        move_centre(map, pose, push);
    }
    return pushed;
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

/// Lowers a body that has walked off its ground onto ground up to its step height below, if there
/// is any there, returning how far it lowered it along its gravity up, or nothing if there isn't.
///
/// A body whose centre a portal would carry into another sector on the way isn't lowered.
[[nodiscard]] std::optional<float> snap_down(const Map& map, const BodyShape& shape, Pose& pose) {
    auto lowered = pose;
    move_centre(map, lowered, pose.gravity_up * -shape.step_height);
    std::ignore = push_out(map, shape, lowered, true);
    if (lowered.centre.sector != pose.centre.sector ||
        !ground_among(contacts_of(map, shape, lowered), shape, lowered)) {
        return std::nullopt;
    }
    const auto drop = dot(pose.centre.point - lowered.centre.point, pose.gravity_up);
    pose = lowered;
    return drop;
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

    Movement movement{
        .pose = pose,
        .velocity = velocity,
        .ground = ground_among(contacts_of(map, shape, pose), shape, pose),
    };
    for (std::size_t substep = 0; substep < substeps; ++substep) {
        const auto grounded = movement.ground.has_value();
        const auto from = movement.pose.centre.sector;
        move_centre(map, movement.pose, movement.velocity * substep_seconds);
        const auto crossed_to = movement.pose.centre;
        const auto pushed = push_out(map, shape, movement.pose, grounded);
        // The floor under the centre decides the body's height, so a grounded body whose centre
        // crosses onto a higher floor is lifted onto it by the push-out, which is the step.
        if (grounded && crossed_to.sector != from &&
            movement.pose.centre.sector == crossed_to.sector) {
            if (const auto lift =
                    dot(movement.pose.centre.point - crossed_to.point, movement.pose.gravity_up);
                lift > skin) {
                movement.stepped += lift;
            }
        }

        auto contacts = contacts_of(map, shape, movement.pose);
        movement.ground = ground_among(contacts, shape, movement.pose);
        // A grounded body that walks off its ground, rather than leaving it upward, snaps down.
        if (grounded && !movement.ground &&
            dot(movement.velocity, movement.pose.gravity_up) <= 0.0F) {
            if (const auto drop = snap_down(map, shape, movement.pose)) {
                movement.stepped -= *drop;
                contacts = contacts_of(map, shape, movement.pose);
                movement.ground = ground_among(contacts, shape, movement.pose);
            }
        }
        // What the body was pushed out of clips its velocity too, though it may now be clear of it.
        contacts.insert(contacts.end(), pushed.begin(), pushed.end());
        if (grounded || movement.ground) {
            as_walls(contacts, shape, movement.pose);
        }
        movement.velocity = clip(movement.velocity, contacts);
    }
    return movement;
}

std::optional<Pose> settle(const Map& map, const BodyShape& shape, const Pose& pose) {
    check_body(shape, pose);
    auto settled = pose;
    std::ignore = push_out(map, shape, settled, false);
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
