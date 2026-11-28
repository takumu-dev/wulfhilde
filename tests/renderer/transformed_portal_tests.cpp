// Views through transformed portals: each seen from the eye carried through the portal, nested as
// deep as the renderer's settings allow, and past their limits filled flat.
#include "gpu_test.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/frame_measurement.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;
using wulfhilde::Vector3;

// The white ambient light of the test maps, under which the tests' pinned exposure shows a
// mid-grey surface as a mid-grey.
constexpr wulfhilde::AmbientLight white_light{
    .color = {.r = 1.0F, .g = 1.0F, .b = 1.0F},
    .lux = 250.0F,
};

// One wall of a sector's loop: where it starts, its material, and its id if it is a side of a
// portal.
struct WallSpec {
    Vector2 start;
    std::string_view material = "wall";
    std::string_view id{};
};

// A sector bounded by a loop of walls, 3 m high, with a grey floor and ceiling.
wulfhilde::SectorDescription sector(std::string id, const std::vector<WallSpec>& walls) {
    wulfhilde::SectorDescription description{
        .id = std::move(id),
        .floor = {.height = 0.0F, .material = "floor"},
        .ceiling = {.height = 3.0F, .material = "floor"},
        .loops = {{}},
    };
    for (const auto& wall : walls) {
        auto& added = description.loops.front().emplace_back(wulfhilde::WallDescription{
            .start = wall.start,
            .surface = {.material = std::string(wall.material)},
        });
        if (!wall.id.empty()) {
            added.id = std::string(wall.id);
        }
    }
    return description;
}

// A portal transform that turns by a yaw and then moves.
wulfhilde::PortalTransformDescription moved(Vector3 translation, float yaw_degrees = 0.0F) {
    return {.translation = translation, .rotation = {.yaw = Angle::degrees(yaw_degrees)}};
}

// Makes a map from a description that must be valid.
wulfhilde::Map make_map(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The hall, from (0, 0) to (4, 4), with cyan walls and a door in its north wall from x = 1 to 3,
// through a portal that turns 90 degrees left and moves 24 m along X into the gallery: the square
// from (16, 0) to (20, 4), whose door is its east wall's middle, from y = 1 to 3. Looking north out
// of the hall looks west into the gallery, at its yellow west wall. The gallery has an ambient
// light of its own.
wulfhilde::MapDescription turned_map(const wulfhilde::AmbientLight& gallery_light = white_light) {
    auto gallery = sector("gallery", {
                                         {.start = {.x = 16.0F, .y = 0.0F}},
                                         {.start = {.x = 20.0F, .y = 0.0F}},
                                         {.start = {.x = 20.0F, .y = 1.0F}, .id = "gallery-door"},
                                         {.start = {.x = 20.0F, .y = 3.0F}},
                                         {.start = {.x = 20.0F, .y = 4.0F}},
                                         {.start = {.x = 16.0F, .y = 4.0F}, .material = "far"},
                                     });
    gallery.ambient = gallery_light;
    return {
        .ambient = white_light,
        .sectors =
            {
                sector("hall",
                       {
                           {.start = {.x = 0.0F, .y = 0.0F}, .material = "near"},
                           {.start = {.x = 4.0F, .y = 0.0F}, .material = "near"},
                           {.start = {.x = 4.0F, .y = 4.0F}, .material = "near"},
                           {.start = {.x = 3.0F, .y = 4.0F}, .material = "near", .id = "hall-door"},
                           {.start = {.x = 1.0F, .y = 4.0F}, .material = "near"},
                           {.start = {.x = 0.0F, .y = 4.0F}, .material = "near"},
                       }),
                std::move(gallery),
            },
        .portals =
            {
                {
                    .a = {.id = "hall-door"},
                    .b = {.id = "gallery-door"},
                    .transform = moved({.x = 24.0F}, 90.0F),
                },
            },
        .start = {.sector = "hall", .point = {.x = 2.0F, .y = 2.0F}},
    };
}

// A corridor from (0, 0) to (4, 2) whose east and west walls are the sides of a portal that moves
// 4 m west, so that it is seen again through each, without end.
wulfhilde::MapDescription mirror_map() {
    return {
        .ambient = white_light,
        .sectors =
            {
                sector("corridor",
                       {
                           {.start = {.x = 0.0F, .y = 0.0F}},
                           {.start = {.x = 4.0F, .y = 0.0F}, .id = "east"},
                           {.start = {.x = 4.0F, .y = 2.0F}},
                           {.start = {.x = 0.0F, .y = 2.0F}, .id = "west"},
                       }),
            },
        .portals = {{.a = {.id = "east"}, .b = {.id = "west"}, .transform = moved({.x = -4.0F})}},
        .start = {.sector = "corridor", .point = {.x = 2.0F, .y = 1.0F}},
    };
}

// The hub, a square from (0, 0) to (4, 4) whose walls are each wholly a side of a portal, its
// north linked to its south and its east to its west, so that through any of them it is seen
// again, beside itself, without end.
wulfhilde::MapDescription hub_map() {
    return {
        .ambient = white_light,
        .sectors =
            {
                sector("hub",
                       {
                           {.start = {.x = 0.0F, .y = 0.0F}, .id = "south"},
                           {.start = {.x = 4.0F, .y = 0.0F}, .id = "east"},
                           {.start = {.x = 4.0F, .y = 4.0F}, .id = "north"},
                           {.start = {.x = 0.0F, .y = 4.0F}, .id = "west"},
                       }),
            },
        .portals =
            {
                {.a = {.id = "north"}, .b = {.id = "south"}, .transform = moved({.y = -4.0F})},
                {.a = {.id = "east"}, .b = {.id = "west"}, .transform = moved({.x = -4.0F})},
            },
        .start = {.sector = "hub", .point = {.x = 2.0F, .y = 2.0F}},
    };
}

// A dark room from (0, 0) to (4, 4) without ambient light, whose east wall glows.
wulfhilde::MapDescription glowing_wall_map() {
    return {
        .sectors =
            {
                sector("room",
                       {
                           {.start = {.x = 0.0F, .y = 0.0F}},
                           {.start = {.x = 4.0F, .y = 0.0F}, .material = "glow"},
                           {.start = {.x = 4.0F, .y = 4.0F}},
                           {.start = {.x = 0.0F, .y = 4.0F}},
                       }),
            },
        .start = {.sector = "room", .point = {.x = 2.0F, .y = 2.0F}},
    };
}

// The assets of a test map: each material a base colour, apart from the glowing wall, which is
// black and glows as a grey surface reflecting 18% of an ambient light would.
wulfhilde::MapAssets assets_of(const wulfhilde::Map& map,
                               const wulfhilde::AmbientLight& glow = white_light) {
    static const std::map<std::string, wulfhilde::Color, std::less<>> colors{
        {"floor", {.r = 0.2F, .g = 0.2F, .b = 0.2F}},
        {"wall", {.r = 0.2F, .g = 0.2F, .b = 0.2F}},
        {"near", {.r = 0.05F, .g = 0.8F, .b = 0.8F}},
        {"far", {.r = 0.8F, .g = 0.8F, .b = 0.05F}},
        {"glow", {}},
    };
    wulfhilde::MapAssets assets;
    for (const auto& path : map.materials()) {
        auto& material = assets.materials.emplace_back(
            wulfhilde::Material{.color = {.factor = colors.at(path)}});
        if (path == "glow") {
            material.emission = wulfhilde::Emission{
                .nits = 0.18F * glow.lux / std::numbers::pi_v<float>,
                .color = glow.color,
            };
        }
    }
    return assets;
}

// A renderer with a map set, which must both succeed.
wulfhilde::Renderer renderer_of(const wulfhilde::Map& map, wulfhilde::RendererSettings settings,
                                const wulfhilde::AmbientLight& glow = white_light) {
    auto renderer = wulfhilde::Renderer::create_headless(settings);
    require_value(renderer);
    require_value(renderer->set_map(map, assets_of(map, glow)));
    return *std::move(renderer);
}

// A camera in the first sector of a map, facing by a yaw, seeing 90 degrees from the image's top to
// its bottom.
wulfhilde::Camera camera_at(Vector3 point, Angle yaw) {
    return {
        .location = {.sector = wulfhilde::SectorId{0}, .point = point},
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, Angle{}, Angle{}),
        .vertical_field_of_view = Angle::degrees(90.0F),
    };
}

// Counts the views of a frame, which must be measured.
std::size_t views_of(const wulfhilde::Map& map, const wulfhilde::Camera& camera,
                     wulfhilde::RendererSettings settings = {.validation = true}) {
    settings.measures_frames = true;
    auto renderer = renderer_of(map, settings);
    auto measurement = renderer.measure_frame(camera, {.width = 64, .height = 64});
    require_value(measurement);
    return measurement ? measurement->views : 0;
}

// Renders an image 64 px square, which must succeed.
wulfhilde::Image render(wulfhilde::Renderer& renderer, const wulfhilde::Camera& camera) {
    auto image = renderer.render_image(camera, {.width = 64, .height = 64});
    require_value(image);
    return *std::move(image);
}

using Pixel = std::array<std::uint8_t, 4>;

Pixel pixel_at(const wulfhilde::Image& image, std::size_t x, std::size_t y) {
    const auto start = ((y * image.extent().width) + x) * wulfhilde::Image::bytes_per_pixel;
    Pixel pixel{};
    std::ranges::copy(image.pixels().subspan(start, pixel.size()), pixel.begin());
    return pixel;
}

std::string describe(const Pixel& pixel) {
    return std::format("({}, {}, {}, {})", pixel.at(0), pixel.at(1), pixel.at(2), pixel.at(3));
}

// Whether two pixels are the same colour, but for a step of rounding in each channel.
bool alike(const Pixel& a, const Pixel& b) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
        if (std::abs(int{a.at(channel)} - int{b.at(channel)}) > 1) {
            return false;
        }
    }
    return true;
}

// The pixel at the middle of the glowing wall, seen square on from the middle of the dark room.
Pixel glowing_wall(const wulfhilde::AmbientLight& glow = white_light) {
    const auto map = make_map(glowing_wall_map());
    auto renderer = renderer_of(map, gpu_test_settings, glow);
    return pixel_at(render(renderer, camera_at({.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle{})), 32,
                    32);
}

} // namespace

TEST_CASE_METHOD(GpuTest,
                 "a sector seen through a transformed portal is seen in a view of its own, from "
                 "the eye carried through the portal",
                 "[renderer]") {
    const auto map = make_map(turned_map());
    const auto north = camera_at({.x = 2.0F, .y = 1.0F, .z = 1.5F}, Angle::degrees(90.0F));
    auto renderer = renderer_of(map, gpu_test_settings);

    // Straight ahead, through the door, the gallery's west wall stands 4 m beyond it, and to
    // either side of the door, the hall's own north wall.
    const auto image = render(renderer, north);
    const auto through = pixel_at(image, 32, 32);
    const auto beside = pixel_at(image, 4, 32);

    CAPTURE(describe(through), describe(beside));
    CHECK(views_of(map, north) == 2);
    CHECK(through.at(0) > through.at(2) + 64);
    CHECK(through.at(1) > through.at(2) + 64);
    CHECK(beside.at(1) > beside.at(0) + 64);
    CHECK(beside.at(2) > beside.at(0) + 64);
}

TEST_CASE_METHOD(GpuTest,
                 "facing portals nest views as deep as the nested view limit, 4 by default",
                 "[renderer]") {
    const auto map = make_map(mirror_map());
    const auto east = camera_at({.x = 2.0F, .y = 1.0F, .z = 1.5F}, Angle{});

    CHECK(views_of(map, east) == 5);
    CHECK(views_of(map, east, {.validation = true, .nested_view_limit = 2}) == 3);
    CHECK(views_of(map, east, {.validation = true, .nested_view_limit = 0}) == 1);
}

TEST_CASE_METHOD(GpuTest, "a frame has as many views as the view limit, 16 by default",
                 "[renderer]") {
    const auto map = make_map(hub_map());
    // Toward the north-east corner, through two portals at once, each seeing two more.
    const auto corner = camera_at({.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(45.0F));

    CHECK(views_of(map, corner) == 16);
    CHECK(views_of(map, corner, {.validation = true, .view_limit = 6}) == 6);
    CHECK(views_of(map, corner, {.validation = true, .view_limit = 1}) == 1);
}

TEST_CASE_METHOD(GpuTest,
                 "an opening past the nested view limit is filled flat with the far sector's "
                 "ambient light, as an 18% grey surface reflects it",
                 "[renderer]") {
    const auto map = make_map(mirror_map());
    auto settings = gpu_test_settings;
    settings.nested_view_limit = 0;
    auto renderer = renderer_of(map, settings);

    const auto image = render(renderer, camera_at({.x = 2.0F, .y = 1.0F, .z = 1.5F}, Angle{}));
    const auto glow = glowing_wall();

    for (const auto& [x, y] : {std::pair{32U, 32U}, std::pair{28U, 24U}, std::pair{36U, 40U}}) {
        const auto fill = pixel_at(image, x, y);
        CAPTURE(x, y, describe(fill), describe(glow));
        CHECK(alike(fill, glow));
    }
}

TEST_CASE_METHOD(GpuTest,
                 "an opening past the view limit is filled flat with the ambient light of the "
                 "sector beyond it",
                 "[renderer]") {
    constexpr wulfhilde::AmbientLight red_light{
        .color = {.r = 1.0F, .g = 0.25F, .b = 0.25F},
        .lux = 500.0F,
    };
    const auto map = make_map(turned_map(red_light));
    auto settings = gpu_test_settings;
    settings.view_limit = 1;
    auto renderer = renderer_of(map, settings);

    const auto fill = pixel_at(
        render(renderer, camera_at({.x = 2.0F, .y = 1.0F, .z = 1.5F}, Angle::degrees(90.0F))), 32,
        32);
    const auto glow = glowing_wall(red_light);

    CAPTURE(describe(fill), describe(glow));
    CHECK(alike(fill, glow));
}
