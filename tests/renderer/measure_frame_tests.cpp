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
#include <cmath>
#include <cstddef>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;

// The white ambient light of every test map.
constexpr wulfhilde::AmbientLight light{.color = {.r = 1.0F, .g = 1.0F, .b = 1.0F}, .lux = 250.0F};

// The settings of a renderer that measures its frames, under validation.
constexpr wulfhilde::RendererSettings measuring{
    .validation = true,
    .measures_frames = true,
};

// A sector 3 m high, dressed in one material, bounded by a loop of walls from each point in turn.
// The walls that are doorways take the ids given, by the index of the point each starts from.
wulfhilde::SectorDescription sector(std::string id, const std::vector<Vector2>& points,
                                    const std::map<std::size_t, std::string>& doorways = {}) {
    std::vector<wulfhilde::WallDescription> walls;
    for (std::size_t index = 0; const auto point : points) {
        auto& wall = walls.emplace_back(wulfhilde::WallDescription{
            .start = point,
            .surface = {.material = "grey"},
        });
        if (const auto doorway = doorways.find(index); doorway != doorways.end()) {
            wall.id = doorway->second;
        }
        ++index;
    }
    return {
        .id = std::move(id),
        .floor = {.height = 0.0F, .material = "grey"},
        .ceiling = {.height = 3.0F, .material = "grey"},
        .loops = {std::move(walls)},
    };
}

// A square room 4 m on a side from a corner at (x, 0).
wulfhilde::SectorDescription room(std::string id, float x) {
    return sector(std::move(id), {
                                     {.x = x, .y = 0.0F},
                                     {.x = x + 4.0F, .y = 0.0F},
                                     {.x = x + 4.0F, .y = 4.0F},
                                     {.x = x, .y = 4.0F},
                                 });
}

// Makes a map from a description that must be valid.
wulfhilde::Map make_map(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// A map of rooms side by side along X, unlinked, starting in the first.
wulfhilde::Map rooms(std::size_t count) {
    wulfhilde::MapDescription description{
        .ambient = light,
        .start = {.sector = "room0", .point = {.x = 2.0F, .y = 2.0F}},
    };
    for (std::size_t index = 0; index < count; ++index) {
        description.sectors.push_back(
            room(std::format("room{}", index), 5.0F * static_cast<float>(index)));
    }
    return make_map(std::move(description));
}

// Rooms linked through doorways 1 m wide, starting in the first. The first room is from (0, 0) to
// (4, 4), and the second east of it to (8, 4), through the middle of the wall between them. The
// third, if there is one, is north of the second, to (8, 8), through the wall between them from
// x = 6.5 to 7.5; without it, the second room's north wall is solid there.
wulfhilde::Map linked_rooms(bool with_third) {
    wulfhilde::MapDescription description{
        .ambient = light,
        .sectors =
            {
                sector("first",
                       {
                           {.x = 0.0F, .y = 0.0F},
                           {.x = 4.0F, .y = 0.0F},
                           {.x = 4.0F, .y = 1.5F},
                           {.x = 4.0F, .y = 2.5F},
                           {.x = 4.0F, .y = 4.0F},
                           {.x = 0.0F, .y = 4.0F},
                       },
                       {{2, "first-east"}}),
                sector("second",
                       {
                           {.x = 4.0F, .y = 0.0F},
                           {.x = 8.0F, .y = 0.0F},
                           {.x = 8.0F, .y = 4.0F},
                           {.x = 7.5F, .y = 4.0F},
                           {.x = 6.5F, .y = 4.0F},
                           {.x = 4.0F, .y = 4.0F},
                           {.x = 4.0F, .y = 2.5F},
                           {.x = 4.0F, .y = 1.5F},
                       },
                       {{3, "second-north"}, {6, "second-west"}}),
            },
        .portals = {{.a = {.id = "first-east"}, .b = {.id = "second-west"}}},
        .start = {.sector = "first", .point = {.x = 2.0F, .y = 2.0F}},
    };
    if (with_third) {
        description.sectors.push_back(sector("third",
                                             {
                                                 {.x = 4.0F, .y = 4.0F},
                                                 {.x = 6.5F, .y = 4.0F},
                                                 {.x = 7.5F, .y = 4.0F},
                                                 {.x = 8.0F, .y = 4.0F},
                                                 {.x = 8.0F, .y = 8.0F},
                                                 {.x = 4.0F, .y = 8.0F},
                                             },
                                             {{1, "third-south"}}));
        description.portals.push_back({.a = {.id = "second-north"}, .b = {.id = "third-south"}});
    }
    return make_map(std::move(description));
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

// A camera in the room where a map starts, 1.5 m up, facing by a yaw.
wulfhilde::Camera in_first_room(const wulfhilde::Map& map, Vector2 point, Angle yaw) {
    return {
        .location =
            {
                .sector = map.start().location.sector,
                .point = {.x = point.x, .y = point.y, .z = 1.5F},
            },
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, Angle{}, Angle{}),
        .vertical_field_of_view = Angle::degrees(60.0F),
    };
}

// A camera at the middle of the first room, facing +X.
wulfhilde::Camera in_first_room(const wulfhilde::Map& map) {
    return in_first_room(map, {.x = 2.0F, .y = 2.0F}, Angle{});
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
                 "a measured frame times its culling, main pass, exposure and resolve on the GPU, "
                 "which add up to its GPU time",
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
    CHECK(names == std::vector<std::string_view>{"culling", "main", "exposure", "resolve"});
    CHECK(measurement.gpu > std::chrono::nanoseconds{});
    CHECK(sum == measurement.gpu);
}

TEST_CASE_METHOD(GpuTest, "a measured frame draws one view, and renders no shadow maps",
                 "[renderer]") {
    const auto map = rooms(1);
    auto renderer = measuring_renderer_of(map);

    const auto measurement = measure(renderer, in_first_room(map));

    CHECK(measurement.views == 1);
    CHECK(measurement.shadow_maps == 0);
}

TEST_CASE_METHOD(GpuTest, "a frame draws only the surfaces within the camera's sight",
                 "[renderer]") {
    const auto map = rooms(1);
    auto renderer = measuring_renderer_of(map);

    // Facing the east wall from 2 m away, the wall fills the image. Facing the north-east corner,
    // the image holds the east and north walls, with the floor and the ceiling near the corner.
    const auto facing_wall = measure(renderer, in_first_room(map));
    const auto facing_corner =
        measure(renderer, in_first_room(map, {.x = 2.0F, .y = 2.0F}, Angle::degrees(45.0F)));

    CHECK(facing_wall.draws == 1);
    CHECK(facing_corner.draws == 4);
}

TEST_CASE_METHOD(GpuTest, "a sector that no portal leads to adds no draws", "[renderer]") {
    const auto alone = rooms(1);
    const auto beside = rooms(3);
    auto renderer_alone = measuring_renderer_of(alone);
    auto renderer_beside = measuring_renderer_of(beside);

    // The other rooms lie straight ahead, behind the east wall.
    const auto measured_alone = measure(renderer_alone, in_first_room(alone));
    const auto measured_beside = measure(renderer_beside, in_first_room(beside));

    CHECK(measured_beside.draws == measured_alone.draws);
}

TEST_CASE_METHOD(GpuTest,
                 "a sector beyond two openings that the first opening hides adds no draws, though "
                 "it lies within the camera's field of view",
                 "[renderer]") {
    const auto two = linked_rooms(false);
    const auto three = linked_rooms(true);
    auto renderer_two = measuring_renderer_of(two);
    auto renderer_three = measuring_renderer_of(three);

    // Facing east through the first doorway, the second doorway is 2 m to the left and 6 m ahead,
    // within the image but outside the first doorway.
    const Vector2 point{.x = 1.0F, .y = 2.0F};
    const auto measured_two = measure(renderer_two, in_first_room(two, point, Angle{}));
    const auto measured_three = measure(renderer_three, in_first_room(three, point, Angle{}));

    CHECK(measured_three.draws == measured_two.draws);
}

TEST_CASE_METHOD(GpuTest, "a sector seen through two openings in a row adds draws", "[renderer]") {
    const auto two = linked_rooms(false);
    const auto three = linked_rooms(true);
    auto renderer_two = measuring_renderer_of(two);
    auto renderer_three = measuring_renderer_of(three);

    // Looking through the middle of the first doorway, at (4, 2), to the middle of the second, at
    // (7, 4).
    const Vector2 point{.x = 2.5F, .y = 1.0F};
    const auto yaw = Angle::radians(std::atan2(2.0F, 3.0F));
    const auto measured_two = measure(renderer_two, in_first_room(two, point, yaw));
    const auto measured_three = measure(renderer_three, in_first_room(three, point, yaw));

    CHECK(measured_three.views == 1);
    CHECK(measured_three.draws > measured_two.draws);
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
