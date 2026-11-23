// How Catch2 prints collision's types when a check fails.
//
// Every test file whose checks or captures hold one of them includes this, for the reason
// math_printing.hpp gives.
#pragma once

#include "world_printing.hpp"
#include <wulfhilde/collision/ground.hpp>
#include <wulfhilde/collision/pose.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_tostring.hpp>

#include <format>
#include <string>

template <> struct Catch::StringMaker<wulfhilde::Ground> {
    static std::string convert(const wulfhilde::Ground& ground) {
        using Catch::StringMaker;
        return std::format("{}, facing {}",
                           StringMaker<wulfhilde::SurfaceRef>::convert(ground.surface),
                           StringMaker<wulfhilde::Vector3>::convert(ground.normal));
    }
};

template <> struct Catch::StringMaker<wulfhilde::Pose> {
    static std::string convert(const wulfhilde::Pose& pose) {
        using Catch::StringMaker;
        return std::format("centre {}, tilt {}, gravity up {}",
                           StringMaker<wulfhilde::Location>::convert(pose.centre),
                           StringMaker<wulfhilde::Rotation>::convert(pose.tilt),
                           StringMaker<wulfhilde::Vector3>::convert(pose.gravity_up));
    }
};
