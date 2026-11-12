// The renderer drawing into a window. The tests' windows open offscreen, where Vulkan presents to
// a headless surface that nobody sees, so what a frame shows is checked by hand.
#include <wulfhilde/platform/window.hpp>

#include "gpu_test.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/platform/platform.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <expected>
#include <format>
#include <utility>

namespace {

// White ambient light, under which the renderer's pinned exposure shows a mid-grey as a mid-grey.
constexpr wulfhilde::AmbientLight white_light{
    .color = {.r = 1.0F, .g = 1.0F, .b = 1.0F},
    .lux = 250.0F,
};

wulfhilde::WallDescription wall(float x, float y) {
    return {
        .start = {.x = x, .y = y},
        .surface = {.material = "box"},
    };
}

// A square sector 4 m on a side, 3 m high, every surface of one material.
wulfhilde::Map box_map() {
    wulfhilde::SectorDescription box{
        .id = "box",
        .floor = {.height = 0.0F, .material = "box"},
        .ceiling = {.height = 3.0F, .material = "box"},
        .loops = {{wall(0.0F, 0.0F), wall(4.0F, 0.0F), wall(4.0F, 4.0F), wall(0.0F, 4.0F)}},
    };
    auto map = wulfhilde::Map::create({
        .ambient = white_light,
        .sectors = {std::move(box)},
        .start = {.sector = "box", .point = {.x = 2.0F, .y = 2.0F}},
    });
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

wulfhilde::MapAssets box_assets() {
    wulfhilde::MapAssets assets;
    assets.materials.push_back({.color = {.factor = {.r = 0.5F, .g = 0.5F, .b = 0.5F}}});
    return assets;
}

wulfhilde::Camera camera_in(const wulfhilde::Map& map) {
    return {
        .location =
            {
                .sector = map.start().location.sector,
                .point = {.x = 2.0F, .y = 2.0F, .z = 1.65F},
            },
    };
}

// The platform and a window of the tests, which must both come up.
struct TestWindow {
    wulfhilde::Platform platform;
    wulfhilde::Window window;
};

TestWindow open_window() {
    auto platform = wulfhilde::Platform::create();
    if (!platform) {
        FAIL(std::format("{}", platform.error()));
    }
    auto window = wulfhilde::Window::create(
        *platform, {.title = "a renderer's window", .size = {.width = 320, .height = 180}});
    if (!window) {
        FAIL(std::format("{}", window.error()));
    }
    return {.platform = *std::move(platform), .window = *std::move(window)};
}

} // namespace

TEST_CASE_METHOD(GpuTest, "a renderer comes up on a window under validation", "[renderer]") {
    auto test = open_window();

    const auto renderer = wulfhilde::Renderer::create(test.window, gpu_test_settings);

    require_value(renderer);
}

TEST_CASE_METHOD(GpuTest, "a renderer draws a map into its window, frame after frame",
                 "[renderer]") {
    auto test = open_window();
    auto renderer = wulfhilde::Renderer::create(test.window, gpu_test_settings);
    require_value(renderer);
    const auto map = box_map();
    require_value(renderer->set_map(map, box_assets()));

    for (int frame = 0; frame < 5; ++frame) {
        require_value(renderer->draw(camera_in(map)));
    }
}

TEST_CASE_METHOD(GpuTest, "a renderer on a window renders images between its frames",
                 "[renderer]") {
    auto test = open_window();
    auto renderer = wulfhilde::Renderer::create(test.window, gpu_test_settings);
    require_value(renderer);
    const auto map = box_map();
    require_value(renderer->set_map(map, box_assets()));

    require_value(renderer->draw(camera_in(map)));
    const auto image = renderer->render_image(camera_in(map), {.width = 64, .height = 36});
    require_value(image);
    CHECK(image->extent() == wulfhilde::Extent{.width = 64, .height = 36});
    require_value(renderer->draw(camera_in(map)));
}

TEST_CASE_METHOD(GpuTest,
                 "a renderer on a window refuses a sample count the device lacks, naming the "
                 "counts it has",
                 "[renderer]") {
    auto test = open_window();

    const auto renderer =
        wulfhilde::Renderer::create(test.window, {.validation = true, .sample_count = 3});

    REQUIRE(!renderer.has_value());
    CHECK(renderer.error().code == wulfhilde::DeviceErrorCode::unsupported_sample_count);
    CHECK_THAT(renderer.error().message,
               Catch::Matchers::StartsWith("the device can't take 3 samples a pixel, only 1, "));
}

TEST_CASE_METHOD(GpuTest, "a renderer moved from leaves its window to the one it moved to",
                 "[renderer]") {
    auto test = open_window();
    auto renderer = wulfhilde::Renderer::create(test.window, gpu_test_settings);
    require_value(renderer);
    const auto map = box_map();

    auto moved = std::move(*renderer);

    require_value(moved.set_map(map, box_assets()));
    require_value(moved.draw(camera_in(map)));
}
