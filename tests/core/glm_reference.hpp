// Conversions to and from GLM, the reference that property tests compare the generic operations
// with. The references are computed in double, so that each float result is checked against a
// value that is correct to well within float precision.
#pragma once

#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>

#include <glm/ext/matrix_double4x4.hpp>
#include <glm/ext/vector_double2.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/ext/vector_double4.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <algorithm>

inline glm::dvec2 to_glm(wulfhilde::Vector2 v) {
    return {static_cast<double>(v.x), static_cast<double>(v.y)};
}

inline wulfhilde::Vector2 from_glm(const glm::dvec2& v) {
    return {.x = static_cast<float>(v.x), .y = static_cast<float>(v.y)};
}

inline glm::dvec3 to_glm(wulfhilde::Vector3 v) {
    return {static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z)};
}

inline wulfhilde::Vector3 from_glm(const glm::dvec3& v) {
    return {
        .x = static_cast<float>(v.x),
        .y = static_cast<float>(v.y),
        .z = static_cast<float>(v.z),
    };
}

inline glm::dvec4 to_glm(wulfhilde::Vector4 v) {
    return {
        static_cast<double>(v.x),
        static_cast<double>(v.y),
        static_cast<double>(v.z),
        static_cast<double>(v.w),
    };
}

inline wulfhilde::Vector4 from_glm(const glm::dvec4& v) {
    return {
        .x = static_cast<float>(v.x),
        .y = static_cast<float>(v.y),
        .z = static_cast<float>(v.z),
        .w = static_cast<float>(v.w),
    };
}

inline glm::dmat4 to_glm(const wulfhilde::Matrix4& m) {
    const auto& [x, y, z, w] = m.columns;
    return {to_glm(x), to_glm(y), to_glm(z), to_glm(w)};
}

inline wulfhilde::Matrix4 from_glm(const glm::dmat4& m) {
    return {
        .columns =
            {
                from_glm(glm::column(m, 0)),
                from_glm(glm::column(m, 1)),
                from_glm(glm::column(m, 2)),
                from_glm(glm::column(m, 3)),
            },
    };
}

// Whether each column of one matrix lies within a distance of the same column of another.
inline bool approx_equal(const wulfhilde::Matrix4& a, const wulfhilde::Matrix4& b,
                         float tolerance) {
    return std::ranges::equal(
        a.columns, b.columns,
        [tolerance](const wulfhilde::Vector4& p, const wulfhilde::Vector4& q) {
            return wulfhilde::approx_equal(p, q, tolerance);
        });
}
