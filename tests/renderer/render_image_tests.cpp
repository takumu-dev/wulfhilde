#include "gpu_test.hpp"
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector2.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/camera.hpp>
#include <wulfhilde/renderer/device_error.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/image.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/ambient_light.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;
using wulfhilde::Vector3;

// The ambient light of every test map: white, at an illuminance under which the renderer's pinned
// exposure shows a mid-grey surface as a mid-grey.
constexpr wulfhilde::AmbientLight white_light{
    .color = {.r = 1.0F, .g = 1.0F, .b = 1.0F},
    .lux = 250.0F,
};

// A sector bounded by a loop of walls from each point in turn, each wall with its own material,
// named in the same order, under a floor and a ceiling with materials of their own.
wulfhilde::SectorDescription sector(std::string id, const std::vector<Vector2>& points,
                                    const std::vector<std::string>& wall_materials,
                                    float ceiling_height = 3.0F) {
    REQUIRE(points.size() == wall_materials.size());
    std::vector<wulfhilde::WallDescription> walls;
    walls.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index) {
        walls.push_back({
            .start = points.at(index),
            .surface = {.material = wall_materials.at(index)},
        });
    }
    return {
        .id = std::move(id),
        .floor = {.height = 0.0F, .material = "floor"},
        .ceiling = {.height = ceiling_height, .material = "ceiling"},
        .loops = {std::move(walls)},
    };
}

// The box: a square sector 4 m on a side from (0, 0) to (4, 4), 3 m high, whose walls each have a
// material named after the way they face from outside.
wulfhilde::MapDescription box_map() {
    return {
        .ambient = white_light,
        .sectors =
            {
                sector("box",
                       {
                           {.x = 0.0F, .y = 0.0F},
                           {.x = 4.0F, .y = 0.0F},
                           {.x = 4.0F, .y = 4.0F},
                           {.x = 0.0F, .y = 4.0F},
                       },
                       {"south", "east", "north", "west"}),
            },
        .start = {.sector = "box", .point = {.x = 2.0F, .y = 2.0F}},
    };
}

// Makes a map from a description that must be valid.
wulfhilde::Map make_map(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The base colour of each test material, by its asset path: the floor red, the ceiling green, the
// east wall blue, the north wall yellow, the south wall magenta, and the west wall cyan.
const std::map<std::string, wulfhilde::Color, std::less<>>& material_colors() {
    static const std::map<std::string, wulfhilde::Color, std::less<>> colors{
        {"floor", {.r = 0.8F, .g = 0.05F, .b = 0.05F}},
        {"ceiling", {.r = 0.05F, .g = 0.8F, .b = 0.05F}},
        {"east", {.r = 0.05F, .g = 0.05F, .b = 0.8F}},
        {"north", {.r = 0.8F, .g = 0.8F, .b = 0.05F}},
        {"south", {.r = 0.8F, .g = 0.05F, .b = 0.8F}},
        {"west", {.r = 0.05F, .g = 0.8F, .b = 0.8F}},
    };
    return colors;
}

// The assets of a test map, each material only a base colour.
wulfhilde::MapAssets assets_of(const wulfhilde::Map& map) {
    wulfhilde::MapAssets assets;
    for (const auto& path : map.materials()) {
        assets.materials.push_back({.color = {.factor = material_colors().at(path)}});
    }
    return assets;
}

// A renderer with a map set, which must both succeed.
wulfhilde::Renderer renderer_of(const wulfhilde::Map& map,
                                wulfhilde::RendererSettings settings = gpu_test_settings) {
    auto renderer = wulfhilde::Renderer::create_headless(settings);
    require_value(renderer);
    require_value(renderer->set_map(map, assets_of(map)));
    return *std::move(renderer);
}

// A camera in the sector where a map starts, facing by a yaw and a pitch.
wulfhilde::Camera camera_at(const wulfhilde::Map& map, Vector3 point, Angle yaw,
                            Angle pitch = Angle::degrees(0.0F),
                            Angle vertical_field_of_view = Angle::degrees(90.0F)) {
    return {
        .location = {.sector = map.start().location.sector, .point = point},
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, pitch, Angle::degrees(0.0F)),
        .vertical_field_of_view = vertical_field_of_view,
    };
}

// Renders an image, which must succeed.
wulfhilde::Image render(wulfhilde::Renderer& renderer, const wulfhilde::Camera& camera,
                        wulfhilde::Extent extent = {.width = 64, .height = 64}) {
    auto image = renderer.render_image(camera, extent);
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

Pixel centre_of(const wulfhilde::Image& image) {
    return pixel_at(image, image.extent().width / 2, image.extent().height / 2);
}

// Whether a pixel's channels that `channels` names are each well above every other, as a surface
// of one of the test colours shows under white light. AgX lifts the weak channels of a strong
// colour toward white, so they are far from zero.
bool shows(const Pixel& pixel, std::array<bool, 3> channels) {
    constexpr int margin = 64;
    for (std::size_t strong = 0; strong < 3; ++strong) {
        for (std::size_t weak = 0; weak < 3; ++weak) {
            if (channels.at(strong) && !channels.at(weak) &&
                pixel.at(strong) < pixel.at(weak) + margin) {
                return false;
            }
        }
    }
    return true;
}

constexpr std::array red{true, false, false};
constexpr std::array green{false, true, false};
constexpr std::array blue{false, false, true};
constexpr std::array yellow{true, true, false};
constexpr std::array magenta{true, false, true};
constexpr Pixel black{0, 0, 0, 255};

std::string describe(const Pixel& pixel) {
    return std::format("({}, {}, {}, {})", pixel.at(0), pixel.at(1), pixel.at(2), pixel.at(3));
}

} // namespace

TEST_CASE_METHOD(GpuTest, "a sample count the device lacks is refused, naming the counts it has",
                 "[renderer]") {
    const auto renderer =
        wulfhilde::Renderer::create_headless({.validation = true, .sample_count = 3});

    REQUIRE(!renderer.has_value());
    CHECK(renderer.error().code == wulfhilde::DeviceErrorCode::unsupported_sample_count);
    CHECK_THAT(renderer.error().message,
               Catch::Matchers::StartsWith("the device can't take 3 samples a pixel, only 1, ") &&
                   Catch::Matchers::ContainsSubstring(" 4") &&
                   Catch::Matchers::ContainsSubstring(" or "));
}

TEST_CASE_METHOD(GpuTest,
                 "a camera sees the floor below it, the ceiling above it and the wall ahead",
                 "[renderer]") {
    const auto map = make_map(box_map());
    auto renderer = renderer_of(map);
    const Vector3 middle{.x = 2.0F, .y = 2.0F, .z = 1.5F};
    const auto east = Angle::degrees(0.0F);

    const auto down =
        centre_of(render(renderer, camera_at(map, middle, east, Angle::degrees(-90.0F))));
    const auto up =
        centre_of(render(renderer, camera_at(map, middle, east, Angle::degrees(90.0F))));
    const auto ahead = centre_of(render(renderer, camera_at(map, middle, east)));

    CAPTURE(describe(down), describe(up), describe(ahead));
    CHECK(shows(down, red));
    CHECK(shows(up, green));
    CHECK(shows(ahead, blue));
}

TEST_CASE_METHOD(GpuTest, "an image is the size asked for, and opaque", "[renderer]") {
    const auto map = make_map(box_map());
    auto renderer = renderer_of(map);

    const auto image =
        render(renderer, camera_at(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(30.0F)),
               {.width = 40, .height = 30});

    REQUIRE(image.extent() == wulfhilde::Extent{.width = 40, .height = 30});
    for (std::size_t y = 0; y < 30; ++y) {
        for (std::size_t x = 0; x < 40; ++x) {
            REQUIRE(pixel_at(image, x, y).at(3) == 255);
        }
    }
}

TEST_CASE_METHOD(GpuTest, "the wall to a camera's left is at the image's left", "[renderer]") {
    const auto map = make_map(box_map());
    auto renderer = renderer_of(map);

    // Facing east, north is to the left and south to the right.
    const auto image =
        render(renderer, camera_at(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F)),
               {.width = 128, .height = 64});
    const auto left = pixel_at(image, 2, 32);
    const auto right = pixel_at(image, 125, 32);

    CAPTURE(describe(left), describe(right));
    CHECK(shows(left, yellow));
    CHECK(shows(right, magenta));
}

TEST_CASE_METHOD(GpuTest, "the ceiling is at the top of the image and the floor at its bottom",
                 "[renderer]") {
    const auto map = make_map(box_map());
    auto renderer = renderer_of(map);

    // 45° up meets the ceiling before the wall 2 m ahead, and 45° down meets the floor.
    const auto image =
        render(renderer, camera_at(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F)));
    const auto top = pixel_at(image, 32, 1);
    const auto bottom = pixel_at(image, 32, 62);

    CAPTURE(describe(top), describe(bottom));
    CHECK(shows(top, green));
    CHECK(shows(bottom, red));
}

TEST_CASE_METHOD(GpuTest, "a concave sector's floor covers the sector and nothing beside it",
                 "[renderer]") {
    // An L of two arms 4 m wide, 12 m along X and 10 m along Y, seen from 20 m up, where the
    // ceiling and the walls face away and only the floor shows.
    const auto map = make_map({
        .ambient = white_light,
        .sectors =
            {
                sector("hall",
                       {
                           {.x = 0.0F, .y = 0.0F},
                           {.x = 12.0F, .y = 0.0F},
                           {.x = 12.0F, .y = 4.0F},
                           {.x = 4.0F, .y = 4.0F},
                           {.x = 4.0F, .y = 10.0F},
                           {.x = 0.0F, .y = 10.0F},
                       },
                       {"south", "east", "north", "east", "north", "west"}),
            },
        .start = {.sector = "hall", .point = {.x = 2.0F, .y = 2.0F}},
    });
    auto renderer = renderer_of(map);

    // Looking straight down, +X is up in the image and +Y to the left, and with a field of view
    // of 90° the image's edges are 20 m from its centre on the floor, 5 px a metre.
    const auto image = render(renderer,
                              camera_at(map, {.x = 6.0F, .y = 5.0F, .z = 20.0F},
                                        Angle::degrees(0.0F), Angle::degrees(-90.0F)),
                              {.width = 200, .height = 200});
    const auto at = [&image](float x, float y) {
        const auto column = static_cast<std::size_t>(100.0F - ((y - 5.0F) * 5.0F));
        const auto row = static_cast<std::size_t>(100.0F - ((x - 6.0F) * 5.0F));
        return pixel_at(image, column, row);
    };

    CAPTURE(describe(at(10.0F, 2.0F)), describe(at(2.0F, 8.0F)), describe(at(8.0F, 7.0F)));
    CHECK(shows(at(10.0F, 2.0F), red));
    CHECK(shows(at(2.0F, 8.0F), red));
    CHECK(shows(at(2.0F, 2.0F), red));
    CHECK(at(8.0F, 7.0F) == black);
}

TEST_CASE_METHOD(GpuTest, "a surface nearer the eye than 5 cm isn't drawn, and one further is",
                 "[renderer]") {
    const auto map = make_map(box_map());
    auto renderer = renderer_of(map);
    const auto facing_east_wall = [&map](float distance) {
        return camera_at(map, {.x = 4.0F - distance, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F));
    };

    const auto too_near = centre_of(render(renderer, facing_east_wall(0.04F)));
    const auto near_enough = centre_of(render(renderer, facing_east_wall(0.06F)));

    CAPTURE(describe(too_near), describe(near_enough));
    CHECK(too_near == black);
    CHECK(shows(near_enough, blue));
}

TEST_CASE_METHOD(GpuTest, "a wall 4 km away is drawn, since there is no far plane", "[renderer]") {
    const auto map = make_map({
        .ambient = white_light,
        .sectors =
            {
                sector("corridor",
                       {
                           {.x = 0.0F, .y = 0.0F},
                           {.x = 4000.0F, .y = 0.0F},
                           {.x = 4000.0F, .y = 4.0F},
                           {.x = 0.0F, .y = 4.0F},
                       },
                       {"south", "east", "north", "west"}, 4.0F),
            },
        .start = {.sector = "corridor", .point = {.x = 1.0F, .y = 2.0F}},
    });
    auto renderer = renderer_of(map);

    // Through a narrow field of view, the far wall's 4 m are about 11 px of the image's 100.
    const auto image =
        render(renderer,
               camera_at(map, {.x = 1.0F, .y = 2.0F, .z = 2.0F}, Angle::degrees(0.0F),
                         Angle::degrees(0.0F), Angle::degrees(0.5F)),
               {.width = 100, .height = 100});

    CAPTURE(describe(centre_of(image)));
    CHECK(shows(centre_of(image), blue));
}

TEST_CASE_METHOD(GpuTest,
                 "with 4 samples a pixel, edges blend the surfaces either side, and with 1 "
                 "they don't",
                 "[renderer]") {
    const auto map = make_map(box_map());
    // Looking into a corner, tilted, so the edges between the walls, the floor and the ceiling
    // run across pixels at every angle.
    auto camera = camera_at(map, {.x = 1.0F, .y = 1.0F, .z = 1.5F}, Angle::degrees(45.0F),
                            Angle::degrees(-10.0F));
    camera.orientation =
        camera.orientation * wulfhilde::Rotation::from_yaw_pitch_roll(
                                 Angle::degrees(0.0F), Angle::degrees(0.0F), Angle::degrees(10.0F));
    const auto colors_seen = [&](std::size_t sample_count) {
        auto renderer = renderer_of(map, {.validation = true, .sample_count = sample_count});
        const auto image = render(renderer, camera, {.width = 64, .height = 64});
        std::set<Pixel> colors;
        for (std::size_t y = 0; y < 64; ++y) {
            for (std::size_t x = 0; x < 64; ++x) {
                colors.insert(pixel_at(image, x, y));
            }
        }
        return colors.size();
    };

    // Every surface is one flat colour, and the camera sees four of them.
    CHECK(colors_seen(1) == 4);
    CHECK(colors_seen(4) > 8);
}

TEST_CASE_METHOD(GpuTest,
                 "a renderer draws at each sample count the device has, and comes up at no "
                 "other",
                 "[renderer]") {
    const auto map = make_map(box_map());
    std::vector<std::size_t> drawn;
    for (std::size_t sample_count = 1; sample_count <= 64; sample_count *= 2) {
        CAPTURE(sample_count);
        auto renderer = wulfhilde::Renderer::create_headless(
            {.validation = true, .sample_count = sample_count});
        if (!renderer) {
            CHECK(renderer.error().code == wulfhilde::DeviceErrorCode::unsupported_sample_count);
            continue;
        }
        require_value(renderer->set_map(map, assets_of(map)));
        const auto image = render(
            *renderer, camera_at(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F)));
        CHECK(shows(centre_of(image), blue));
        drawn.push_back(sample_count);
    }

    // Vulkan guarantees 1 and 4 for these formats.
    CHECK(std::ranges::contains(drawn, 1U));
    CHECK(std::ranges::contains(drawn, 4U));
}

TEST_CASE_METHOD(GpuTest, "a map set in place of another is drawn instead", "[renderer]") {
    const auto first = make_map(box_map());
    auto description = box_map();
    // The same box, but with its east wall in the west wall's material.
    description.sectors.at(0).loops.at(0).at(1).surface.material = "west";
    const auto second = make_map(std::move(description));
    auto renderer = renderer_of(first);
    const auto camera = camera_at(first, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F));
    const auto before = centre_of(render(renderer, camera));

    require_value(renderer.set_map(second, assets_of(second)));
    const auto after = centre_of(render(renderer, camera));

    CAPTURE(describe(before), describe(after));
    CHECK(shows(before, blue));
    CHECK(shows(after, {false, true, true}));
}

TEST_CASE_METHOD(GpuTest,
                 "a sector's ambient light shades its surfaces, and without any they are "
                 "black",
                 "[renderer]") {
    auto description = box_map();
    description.ambient.reset();
    const auto dark = make_map(std::move(description));
    auto bright_description = box_map();
    bright_description.ambient = wulfhilde::AmbientLight{
        .color = white_light.color,
        .lux = white_light.lux * 4.0F,
    };
    const auto bright = make_map(std::move(bright_description));
    const auto lit = make_map(box_map());
    const auto camera = [](const wulfhilde::Map& map) {
        return camera_at(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F));
    };
    const auto blue_of = [&camera](const wulfhilde::Map& map) {
        auto renderer = renderer_of(map);
        return centre_of(render(renderer, camera(map)));
    };

    const auto unlit = blue_of(dark);
    const auto normal = blue_of(lit);
    const auto brighter = blue_of(bright);

    CAPTURE(describe(unlit), describe(normal), describe(brighter));
    CHECK(unlit == black);
    CHECK(normal.at(2) < brighter.at(2));
}
