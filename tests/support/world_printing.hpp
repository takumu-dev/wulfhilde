// How Catch2 prints the world's types when a check fails.
//
// Every test file whose checks or captures hold one of them includes this, for the reason
// math_printing.hpp gives.
#pragma once

#include "math_printing.hpp"
#include <wulfhilde/core/id.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/location.hpp>
#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/surface.hpp>
#include <wulfhilde/world/surface_ref.hpp>

#include <catch2/catch_tostring.hpp>

#include <format>
#include <string>
#include <utility>
#include <variant>

template <typename Tag> struct Catch::StringMaker<wulfhilde::Id<Tag>> {
    static std::string convert(wulfhilde::Id<Tag> id) { return std::format("#{}", id.index()); }
};

template <> struct Catch::StringMaker<wulfhilde::MapProblem> {
    static std::string convert(const wulfhilde::MapProblem& problem) {
        return std::format("{} [code {}]", problem, std::to_underlying(problem.code));
    }
};

template <> struct Catch::StringMaker<wulfhilde::Surface> {
    static std::string convert(const wulfhilde::Surface& surface) {
        using Catch::StringMaker;
        return std::format("material #{}, offset {}, scale {}, rotation {}",
                           surface.material.index(),
                           StringMaker<wulfhilde::Vector2>::convert(surface.offset),
                           StringMaker<wulfhilde::Vector2>::convert(surface.scale),
                           StringMaker<wulfhilde::Angle>::convert(surface.rotation));
    }
};

template <> struct Catch::StringMaker<wulfhilde::AmbientLight> {
    static std::string convert(const wulfhilde::AmbientLight& ambient) {
        return std::format("{} at {} lux",
                           Catch::StringMaker<wulfhilde::Color>::convert(ambient.color),
                           ambient.lux);
    }
};

template <> struct Catch::StringMaker<wulfhilde::Location> {
    static std::string convert(const wulfhilde::Location& location) {
        return std::format("{} in sector #{}",
                           Catch::StringMaker<wulfhilde::Vector3>::convert(location.point),
                           location.sector.index());
    }
};

template <> struct Catch::StringMaker<wulfhilde::SurfaceRef> {
    static std::string convert(const wulfhilde::SurfaceRef& surface) {
        if (const auto* floor = std::get_if<wulfhilde::SectorFloor>(&surface)) {
            return std::format("the floor of sector #{}", floor->sector.index());
        }
        if (const auto* ceiling = std::get_if<wulfhilde::SectorCeiling>(&surface)) {
            return std::format("the ceiling of sector #{}", ceiling->sector.index());
        }
        return std::format("wall #{}", std::get<wulfhilde::WallId>(surface).index());
    }
};
