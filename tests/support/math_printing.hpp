// How Catch2 prints the engine's math types when a check fails. Each float is printed in its
// shortest form that reads back exactly, so two values that differ in their last place look
// different.
//
// Every test file whose checks or captures hold a math type includes this. A file that didn't
// would print it with Catch2's default, and its copy of Catch2's inline printing could replace this
// one in every other file of the program.
#pragma once

#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/matrix4.hpp>
#include <wulfhilde/core/plane.hpp>
#include <wulfhilde/core/rigid_transform.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/core/vector4.hpp>

#include <catch2/catch_tostring.hpp>

#include <format>
#include <string>

template <> struct Catch::StringMaker<wulfhilde::Angle> {
    static std::string convert(wulfhilde::Angle angle) {
        return std::format("{} degrees", angle.in_degrees());
    }
};

template <> struct Catch::StringMaker<wulfhilde::Vector2> {
    static std::string convert(wulfhilde::Vector2 v) { return std::format("({}, {})", v.x, v.y); }
};

template <> struct Catch::StringMaker<wulfhilde::Vector3> {
    static std::string convert(wulfhilde::Vector3 v) {
        return std::format("({}, {}, {})", v.x, v.y, v.z);
    }
};

template <> struct Catch::StringMaker<wulfhilde::Vector4> {
    static std::string convert(wulfhilde::Vector4 v) {
        return std::format("({}, {}, {}, {})", v.x, v.y, v.z, v.w);
    }
};

template <> struct Catch::StringMaker<wulfhilde::Matrix4> {
    static std::string convert(const wulfhilde::Matrix4& m) {
        std::string columns;
        for (const auto& column : m.columns) {
            columns += Catch::StringMaker<wulfhilde::Vector4>::convert(column);
        }
        return std::format("columns {{{}}}", columns);
    }
};

template <> struct Catch::StringMaker<wulfhilde::YawPitchRoll> {
    static std::string convert(const wulfhilde::YawPitchRoll& angles) {
        return std::format("yaw {}, pitch {}, roll {}", angles.yaw.in_degrees(),
                           angles.pitch.in_degrees(), angles.roll.in_degrees());
    }
};

template <> struct Catch::StringMaker<wulfhilde::Rotation> {
    static std::string convert(const wulfhilde::Rotation& r) {
        using Catch::StringMaker;
        return std::format("facing {}, left {}, up {}",
                           StringMaker<wulfhilde::Vector3>::convert(r.forward()),
                           StringMaker<wulfhilde::Vector3>::convert(r.left()),
                           StringMaker<wulfhilde::Vector3>::convert(r.up()));
    }
};

template <> struct Catch::StringMaker<wulfhilde::RigidTransform> {
    static std::string convert(const wulfhilde::RigidTransform& t) {
        using Catch::StringMaker;
        return std::format("rotation {{{}}}, translation {}",
                           StringMaker<wulfhilde::Rotation>::convert(t.rotation),
                           StringMaker<wulfhilde::Vector3>::convert(t.translation));
    }
};

template <> struct Catch::StringMaker<wulfhilde::Plane> {
    static std::string convert(const wulfhilde::Plane& plane) {
        return std::format("normal {}, distance {}",
                           Catch::StringMaker<wulfhilde::Vector3>::convert(plane.normal),
                           plane.distance);
    }
};

template <> struct Catch::StringMaker<wulfhilde::Color> {
    static std::string convert(const wulfhilde::Color& c) {
        return std::format("rgb({}, {}, {})", c.r, c.g, c.b);
    }
};
