// Geometry seen from above, on the points of loops. It computes in double, so that rounding is far
// finer than the millimetres a map is written in.
#pragma once

#include <wulfhilde/core/vector2.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>
#include <vector>

namespace wulfhilde::detail {

/// A point seen from above, in double.
struct PlanPoint {
    double x = 0.0; ///< The coordinate along X.
    double y = 0.0; ///< The coordinate along Y.
};

/// Returns a point in double.
[[nodiscard]] constexpr PlanPoint to_plan(const Vector2& point) noexcept {
    return {.x = static_cast<double>(point.x), .y = static_cast<double>(point.y)};
}

/// Returns which way the turn from `a` to `b` to `c` goes: positive anticlockwise, negative
/// clockwise and zero when the three are in a line. Its size is twice the area of their triangle.
[[nodiscard]] constexpr double turn(const PlanPoint& a, const PlanPoint& b,
                                    const PlanPoint& c) noexcept {
    return ((b.x - a.x) * (c.y - a.y)) - ((b.y - a.y) * (c.x - a.x));
}

/// Tells whether a point in line with a segment lies on it, its ends included.
[[nodiscard]] constexpr bool within(const PlanPoint& a, const PlanPoint& b,
                                    const PlanPoint& p) noexcept {
    return std::min(a.x, b.x) <= p.x && p.x <= std::max(a.x, b.x) && std::min(a.y, b.y) <= p.y &&
           p.y <= std::max(a.y, b.y);
}

/// Tells whether two segments cross or touch anywhere, their ends included.
[[nodiscard]] constexpr bool segments_meet(const PlanPoint& a, const PlanPoint& b,
                                           const PlanPoint& c, const PlanPoint& d) noexcept {
    const auto c_side = turn(a, b, c);
    const auto d_side = turn(a, b, d);
    const auto a_side = turn(c, d, a);
    const auto b_side = turn(c, d, b);
    if (((c_side > 0.0 && d_side < 0.0) || (c_side < 0.0 && d_side > 0.0)) &&
        ((a_side > 0.0 && b_side < 0.0) || (a_side < 0.0 && b_side > 0.0))) {
        return true;
    }
    return (c_side == 0.0 && within(a, b, c)) || (d_side == 0.0 && within(a, b, d)) ||
           (a_side == 0.0 && within(c, d, a)) || (b_side == 0.0 && within(c, d, b));
}

/// Tells whether a segment from `a` to `b`, followed by one from `b` to `c`, folds back over
/// itself, so that the two meet beyond the point they share.
[[nodiscard]] constexpr bool folds_back(const PlanPoint& a, const PlanPoint& b,
                                        const PlanPoint& c) noexcept {
    return turn(a, b, c) == 0.0 && ((b.x - a.x) * (c.x - b.x)) + ((b.y - a.y) * (c.y - b.y)) < 0.0;
}

/// A wall seen from above, as a segment from its start to its end.
struct PlanSegment {
    PlanPoint start; ///< Where the wall starts.
    PlanPoint end;   ///< Where the wall ends.
};

/// Returns where each wall of a loop ends: where the next wall starts, and for the last wall, where
/// the first does.
///
/// \param starts Where each wall of the loop starts, in order.
[[nodiscard]] inline std::vector<Vector2> loop_ends(std::span<const Vector2> starts) {
    std::vector<Vector2> ends(starts.begin(), starts.end());
    if (!ends.empty()) {
        std::ranges::rotate(ends, std::next(ends.begin()));
    }
    return ends;
}

/// Returns a loop's walls as segments, each ending where the next wall starts.
///
/// \param starts Where each wall of the loop starts, in order.
[[nodiscard]] inline std::vector<PlanSegment> loop_segments(std::span<const Vector2> starts) {
    std::vector<PlanSegment> walls;
    for (const auto& [start, end] : std::views::zip(starts, loop_ends(starts))) {
        walls.push_back({.start = to_plan(start), .end = to_plan(end)});
    }
    return walls;
}

/// Returns twice the area a loop encloses, positive when it runs anticlockwise seen from above.
///
/// \param walls The loop's walls, in order.
[[nodiscard]] constexpr double twice_signed_area(std::span<const PlanSegment> walls) noexcept {
    auto sum = 0.0;
    for (const auto& [start, end] : walls) {
        sum += (start.x * end.y) - (end.x * start.y);
    }
    return sum;
}

} // namespace wulfhilde::detail
