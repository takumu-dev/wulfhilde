/// \file
/// A 4x4 matrix, the form in which a transform reaches a shader.

#pragma once

#include <wulfhilde/core/vector4.hpp>

#include <array>

namespace wulfhilde {

/// A 4x4 matrix of column vectors, stored column by column.
///
/// A matrix multiplies a column vector on its left, `m * v`, and a product `a * b` applies `b`
/// first. Its translation is in its last column, elements 12 to 14 in memory, as in glTF and in
/// Vulkan practice; every shader is compiled with column-major matrices to match. It is 64 packed
/// bytes, so it can sit in a struct that a shader reads under scalar layout.
///
/// There is no general inverse or transpose. A transform is inverted as a `RigidTransform` before
/// it becomes a matrix, and a projection is built directly.
struct Matrix4 {
    /// The four columns, from left to right. The default matrix is all zeros.
    std::array<Vector4, 4> columns{};

    /// Returns the identity matrix, which leaves every vector unchanged.
    [[nodiscard]] static constexpr Matrix4 identity() noexcept {
        return {
            .columns =
                {
                    Vector4{.x = 1.0F},
                    Vector4{.y = 1.0F},
                    Vector4{.z = 1.0F},
                    Vector4{.w = 1.0F},
                },
        };
    }

    /// Compares two matrices exactly, element by element.
    constexpr bool operator==(const Matrix4&) const noexcept = default;
};

/// Multiplies a column vector by a matrix on its left.
///
/// \param m The matrix.
/// \param v The vector.
/// \return The sum of the matrix's columns, each weighted by the matching component of `v`.
[[nodiscard]] constexpr Vector4 operator*(const Matrix4& m, const Vector4& v) noexcept {
    const auto& [x, y, z, w] = m.columns;
    return (x * v.x) + (y * v.y) + (z * v.z) + (w * v.w);
}

/// Returns the product of two matrices, which applies `b` first and then `a`.
///
/// \param a The matrix applied second.
/// \param b The matrix applied first.
/// \return The matrix product `a b`.
[[nodiscard]] constexpr Matrix4 operator*(const Matrix4& a, const Matrix4& b) noexcept {
    const auto& [x, y, z, w] = b.columns;
    return {.columns = {a * x, a * y, a * z, a * w}};
}

} // namespace wulfhilde
