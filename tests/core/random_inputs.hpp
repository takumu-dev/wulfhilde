// Random inputs for property tests, drawn from a fixed seed so that every run, on every standard
// library, tests the same values. std::mt19937's sequence is specified exactly, but the standard's
// distributions are not, so the conversion to float is done here.
#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>

#include <cstdint>
#include <random>

class RandomInputs {
  public:
    // A number from [low, high).
    float number(float low, float high) {
        // The top 24 bits of a draw fill a float's significand, so each is exact.
        const auto unit = static_cast<float>(engine_() >> 8U) * 0x1p-24F;
        return low + ((high - low) * unit);
    }

    // A vector with each component from [-extent, extent).
    wulfhilde::Vector2 vector2(float extent) {
        return {.x = number(-extent, extent), .y = number(-extent, extent)};
    }

    // A vector with each component from [-extent, extent).
    wulfhilde::Vector3 vector3(float extent) {
        return {
            .x = number(-extent, extent),
            .y = number(-extent, extent),
            .z = number(-extent, extent),
        };
    }

    // A vector with each component from [-extent, extent).
    wulfhilde::Vector4 vector4(float extent) {
        return {
            .x = number(-extent, extent),
            .y = number(-extent, extent),
            .z = number(-extent, extent),
            .w = number(-extent, extent),
        };
    }

    // A matrix with each element from [-extent, extent).
    wulfhilde::Matrix4 matrix4(float extent) {
        return {
            .columns = {vector4(extent), vector4(extent), vector4(extent), vector4(extent)},
        };
    }

    // A unit vector, every direction equally likely.
    wulfhilde::Vector3 unit_vector3() {
        // Drawn from within the unit ball, rather than the cube around it, whose corners would be
        // likelier directions.
        auto v = vector3(1.0F);
        while (length_squared(v) <= 0.01F || length_squared(v) > 1.0F) {
            v = vector3(1.0F);
        }
        return normalize(v);
    }

    // An angle from [-180, 180) degrees.
    wulfhilde::Angle angle() { return wulfhilde::Angle::degrees(number(-180.0F, 180.0F)); }

    // A rotation about any axis by any angle.
    wulfhilde::Rotation rotation() {
        const auto axis = unit_vector3();
        return wulfhilde::Rotation::about_axis(axis, angle());
    }

    // A rotation about any axis and a translation with each component from [-extent, extent).
    wulfhilde::RigidTransform rigid_transform(float extent) {
        return {.rotation = rotation(), .translation = vector3(extent)};
    }

  private:
    // NOLINTNEXTLINE(bugprone-random-generator-seed): a fixed seed, so every run tests alike
    std::mt19937 engine_{20'261'018U};
};
