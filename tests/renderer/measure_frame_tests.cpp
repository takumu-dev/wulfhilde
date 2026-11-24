#include "gpu_test.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/frame_measurement.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;

// The settings of a renderer that measures its frames, under validation.
constexpr wulfhilde::RendererSettings measuring{
    .validation = true,
    .measures_frames = true,
};

// A square room 4 m on a side from a corner at (x, 0), 3 m high, dressed in one material.
wulfhilde::SectorDescription room(std::string id, float x) {
    std::vector<wulfhilde::WallDescription> walls;
    for (const auto point : {
             Vector2{.x = x, .y = 0.0F},
             Vector2{.x = x + 4.0F, .y = 0.0F},
             Vector2{.x = x + 4.0F, .y = 4.0F},
             Vector2{.x = x, .y = 4.0F},
         }) {
        walls.push_back({.start = point, .surface = {.material = "grey"}});
    }
    return {
        .id = std::move(id),
        .floor = {.height = 0.0F, .material = "grey"},
        .ceiling = {.height = 3.0F, .material = "grey"},
        .loops = {std::move(walls)},
    };
}

// A map of rooms side by side along X, unlinked, starting in the first.
wulfhilde::Map rooms(std::size_t count) {
    wulfhilde::MapDescription description{
        .ambient =
            wulfhilde::AmbientLight{.color = {.r = 1.0F, .g = 1.0F, .b = 1.0F}, .lux = 250.0F},
        .start = {.sector = "room0", .point = {.x = 2.0F, .y = 2.0F}},
    };
    for (std::size_t index = 0; index < count; ++index) {
        description.sectors.push_back(
            room(std::format("room{}", index), 5.0F * static_cast<float>(index)));
    }
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// A renderer that measures its frames, with a map set, which must both succeed.
wulfhilde::Renderer measuring_renderer_of(const wulfhilde::Map& map) {
    auto renderer = wulfhilde::Renderer::create_headless(measuring);
    require_value(renderer);
    wulfhilde::MapAssets assets;
    assets.materials.push_back({.color = {.factor = {.r = 0.5F, .g = 0.5F, .b = 0.5F}}});
    require_value(renderer->set_map(map, assets));
    return *std::move(renderer);
}

// A camera at the middle of the first room, facing +X.
wulfhilde::Camera in_first_room(const wulfhilde::Map& map) {
    return {
        .location =
            {
                .sector = map.start().location.sector,
                .point = {.x = 2.0F, .y = 2.0F, .z = 1.5F},
            },
        .orientation = wulfhilde::Rotation::identity(),
        .vertical_field_of_view = Angle::degrees(60.0F),
    };
}

// Measures a frame, which must succeed.
wulfhilde::FrameMeasurement measure(wulfhilde::Renderer& renderer,
                                    const wulfhilde::Camera& camera) {
    auto measurement = renderer.measure_frame(camera, {.width = 64, .height = 48});
    require_value(measurement);
    return *std::move(measurement);
}

} // namespace

TEST_CASE_METHOD(GpuTest,
                 "a measured frame times its main pass, exposure and resolve on the GPU, which "
                 "add up to its GPU time",
                 "[renderer]") {
    const auto map = rooms(1);
    auto renderer = measuring_renderer_of(map);

    const auto measurement = measure(renderer, in_first_room(map));

    std::vector<std::string_view> names;
    std::chrono::nanoseconds sum{};
    for (const auto& pass : measurement.passes) {
        names.push_back(pass.name);
        CHECK(pass.time >= std::chrono::nanoseconds{});
        sum += pass.time;
    }
    CHECK(names == std::vector<std::string_view>{"main", "exposure", "resolve"});
    CHECK(measurement.gpu > std::chrono::nanoseconds{});
    CHECK(sum == measurement.gpu);
}

TEST_CASE_METHOD(GpuTest,
                 "a measured frame draws one view, with a draw for each sector, and renders no "
                 "shadow maps",
                 "[renderer]") {
    const auto map = rooms(3);
    auto renderer = measuring_renderer_of(map);

    const auto measurement = measure(renderer, in_first_room(map));

    CHECK(measurement.views == 1);
    CHECK(measurement.draws == 3);
    CHECK(measurement.shadow_maps == 0);
}

TEST_CASE_METHOD(GpuTest,
                 "a measured frame counts the device memory that holds at least its targets",
                 "[renderer]") {
    const auto map = rooms(1);
    auto renderer = measuring_renderer_of(map);

    const auto measurement = measure(renderer, in_first_room(map));

    // The scene's colour alone takes 8 bytes a sample, at 4 samples a pixel.
    CHECK(measurement.device_memory >= std::size_t{64} * 48 * 8 * 4);
}

TEST_CASE_METHOD(GpuTest, "a measured frame takes CPU time to submit", "[renderer]") {
    const auto map = rooms(1);
    auto renderer = measuring_renderer_of(map);

    const auto measurement = measure(renderer, in_first_room(map));

    CHECK(measurement.submission > std::chrono::nanoseconds{});
}

TEST_CASE_METHOD(GpuTest, "a renderer names its device and the device's driver", "[renderer]") {
    const auto renderer = wulfhilde::Renderer::create_headless(gpu_test_settings);
    require_value(renderer);

    CHECK(!renderer->device_name().empty());
    CHECK(!renderer->driver().empty());
}
