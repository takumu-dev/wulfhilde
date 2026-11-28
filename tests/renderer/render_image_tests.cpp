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
#include <wulfhilde/world/exposure.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_description.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;
using wulfhilde::Vector3;

// The ambient light of every test map: white, at an illuminance under which the tests' pinned
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

// The box under white ambient light of an illuminance, seen at a map's exposures.
wulfhilde::MapDescription box_under(float lux, wulfhilde::Exposure exposure = {}) {
    auto description = box_map();
    description.ambient = wulfhilde::AmbientLight{.color = white_light.color, .lux = lux};
    description.exposure = exposure;
    return description;
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

// A camera in the middle of the box, facing its blue east wall.
wulfhilde::Camera facing_east(const wulfhilde::Map& map) {
    return camera_at(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F));
}

// The settings of a renderer that exposes automatically, taking at once what each image meters.
constexpr wulfhilde::RendererSettings instant_exposure{
    .validation = true,
    .exposure_adaptation_speed = std::numeric_limits<float>::infinity(),
};

// The settings of a renderer that sees every image at an exposure, in EV100.
constexpr wulfhilde::RendererSettings pinned_at(float ev100) {
    return {.validation = true, .pinned_exposure = ev100};
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

Pixel center_of(const wulfhilde::Image& image) {
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
constexpr std::array cyan{false, true, true};
constexpr Pixel black{0, 0, 0, 255};

std::string describe(const Pixel& pixel) {
    return std::format("({}, {}, {}, {})", pixel.at(0), pixel.at(1), pixel.at(2), pixel.at(3));
}

// The sample counts that the message of a refused count names, after its "only".
std::vector<std::size_t> counts_named_in(std::string_view message) {
    std::vector<std::size_t> counts;
    const auto only = message.find("only ");
    if (only == std::string_view::npos) {
        return counts;
    }
    std::size_t count = 0;
    bool in_number = false;
    for (const char character : message.substr(only)) {
        if (character >= '0' && character <= '9') {
            count = (count * 10) + static_cast<std::size_t>(character - '0');
            in_number = true;
        } else if (in_number) {
            counts.push_back(count);
            count = 0;
            in_number = false;
        }
    }
    if (in_number) {
        counts.push_back(count);
    }
    return counts;
}

// Whether a renderer comes up with a sample count and draws the box with it, checking that one
// that doesn't come up is refused for its sample count alone.
bool draws_with(const wulfhilde::Map& map, std::size_t sample_count) {
    CAPTURE(sample_count);
    auto renderer =
        wulfhilde::Renderer::create_headless({.validation = true, .sample_count = sample_count});
    if (!renderer) {
        CHECK(renderer.error().code == wulfhilde::DeviceErrorCode::unsupported_sample_count);
        return false;
    }
    require_value(renderer->set_map(map, assets_of(map)));
    const auto image =
        render(*renderer, camera_at(map, {.x = 2.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F)));
    CHECK(shows(center_of(image), blue));
    return true;
}

// Checks that a renderer exposing automatically shows a map's box exactly as one pinned at an
// exposure shows the same map at the default exposures.
void check_seen_as_pinned(wulfhilde::MapDescription description, float ev100) {
    auto unranged_description = description;
    unranged_description.exposure = {};
    const auto ranged = make_map(std::move(description));
    const auto unranged = make_map(std::move(unranged_description));
    auto automatic = renderer_of(ranged, instant_exposure);
    auto pinned = renderer_of(unranged, pinned_at(ev100));

    const auto seen = render(automatic, facing_east(ranged));
    const auto expected = render(pinned, facing_east(unranged));

    CAPTURE(describe(center_of(seen)), describe(center_of(expected)));
    CHECK(std::ranges::equal(seen.pixels(), expected.pixels()));
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
        center_of(render(renderer, camera_at(map, middle, east, Angle::degrees(-90.0F))));
    const auto up =
        center_of(render(renderer, camera_at(map, middle, east, Angle::degrees(90.0F))));
    const auto ahead = center_of(render(renderer, camera_at(map, middle, east)));

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

TEST_CASE_METHOD(GpuTest, "a sector's floor leaves a hole where each of its inner loops is",
                 "[renderer]") {
    // A square 12 m on a side around two holes, a square one and an L, seen from 20 m up, where
    // the ceiling faces away, and so do the walls round the holes, which face out of them.
    auto hall = sector("hall",
                       {
                           {.x = 0.0F, .y = 0.0F},
                           {.x = 12.0F, .y = 0.0F},
                           {.x = 12.0F, .y = 12.0F},
                           {.x = 0.0F, .y = 12.0F},
                       },
                       {"south", "east", "north", "west"});
    hall.loops.push_back(sector("square",
                                {
                                    {.x = 2.0F, .y = 2.0F},
                                    {.x = 2.0F, .y = 5.0F},
                                    {.x = 5.0F, .y = 5.0F},
                                    {.x = 5.0F, .y = 2.0F},
                                },
                                {"east", "south", "west", "north"})
                             .loops.front());
    hall.loops.push_back(sector("l",
                                {
                                    {.x = 7.0F, .y = 6.0F},
                                    {.x = 7.0F, .y = 10.0F},
                                    {.x = 8.0F, .y = 10.0F},
                                    {.x = 8.0F, .y = 7.0F},
                                    {.x = 10.0F, .y = 7.0F},
                                    {.x = 10.0F, .y = 6.0F},
                                },
                                {"east", "south", "west", "south", "west", "north"})
                             .loops.front());
    const auto map = make_map({
        .ambient = white_light,
        .sectors = {std::move(hall)},
        .start = {.sector = "hall", .point = {.x = 1.0F, .y = 1.0F}},
    });
    auto renderer = renderer_of(map);

    // Looking straight down, +X is up in the image and +Y to the left, and with a field of view
    // of 90° the image's edges are 20 m from its centre on the floor, 5 px a metre.
    const auto image = render(renderer,
                              camera_at(map, {.x = 6.0F, .y = 6.0F, .z = 20.0F},
                                        Angle::degrees(0.0F), Angle::degrees(-90.0F)),
                              {.width = 200, .height = 200});
    const auto at = [&image](float x, float y) {
        const auto column = static_cast<std::size_t>(100.0F - ((y - 6.0F) * 5.0F));
        const auto row = static_cast<std::size_t>(100.0F - ((x - 6.0F) * 5.0F));
        return pixel_at(image, column, row);
    };

    // In the square hole, and in each arm of the L.
    for (const auto& [x, y] : {
             std::pair{3.5F, 3.5F},
             std::pair{7.5F, 8.5F},
             std::pair{9.0F, 6.5F},
         }) {
        CAPTURE(x, y, describe(at(x, y)));
        CHECK(at(x, y) == black);
    }
    // The floor between the holes, beside them, and in the L's notch.
    for (const auto& [x, y] : {
             std::pair{6.0F, 6.0F},
             std::pair{1.0F, 3.5F},
             std::pair{9.0F, 9.0F},
             std::pair{11.0F, 11.0F},
         }) {
        CAPTURE(x, y, describe(at(x, y)));
        CHECK(shows(at(x, y), red));
    }
}

TEST_CASE_METHOD(GpuTest, "a surface nearer the eye than 5 cm isn't drawn, and one further is",
                 "[renderer]") {
    const auto map = make_map(box_map());
    auto renderer = renderer_of(map);
    const auto facing_east_wall = [&map](float distance) {
        return camera_at(map, {.x = 4.0F - distance, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F));
    };

    const auto too_near = center_of(render(renderer, facing_east_wall(0.04F)));
    const auto near_enough = center_of(render(renderer, facing_east_wall(0.06F)));

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

    CAPTURE(describe(center_of(image)));
    CHECK(shows(center_of(image), blue));
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
    // A count that no device has is refused with the counts the device reports.
    const auto refused =
        wulfhilde::Renderer::create_headless({.validation = true, .sample_count = 3});
    REQUIRE(!refused.has_value());
    const auto reported = counts_named_in(refused.error().message);

    std::vector<std::size_t> drawn;
    for (std::size_t sample_count = 1; sample_count <= 64; sample_count *= 2) {
        if (draws_with(map, sample_count)) {
            drawn.push_back(sample_count);
        }
    }

    CAPTURE(refused.error().message);
    CHECK(drawn == reported);
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
    const auto before = center_of(render(renderer, camera));

    require_value(renderer.set_map(second, assets_of(second)));
    const auto after = center_of(render(renderer, camera));

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
        return center_of(render(renderer, camera(map)));
    };

    const auto unlit = blue_of(dark);
    const auto normal = blue_of(lit);
    const auto brighter = blue_of(bright);

    CAPTURE(describe(unlit), describe(normal), describe(brighter));
    CHECK(unlit == black);
    CHECK(normal.at(2) < brighter.at(2));
}

TEST_CASE_METHOD(GpuTest, "automatic exposure shows a box under 100 times the light as bright",
                 "[renderer]") {
    const auto blue_under = [](float lux, const wulfhilde::RendererSettings& settings) {
        const auto map = make_map(box_under(lux));
        auto renderer = renderer_of(map, settings);
        return center_of(render(renderer, facing_east(map)));
    };

    const auto dim = blue_under(250.0F, instant_exposure);
    const auto bright = blue_under(25'000.0F, instant_exposure);
    const auto pinned_dim = blue_under(250.0F, pinned_at(6.0F));
    const auto pinned_bright = blue_under(25'000.0F, pinned_at(6.0F));

    CAPTURE(describe(dim), describe(bright), describe(pinned_dim), describe(pinned_bright));
    CHECK(shows(dim, blue));
    CHECK(std::abs(int{dim.at(2)} - int{bright.at(2)}) <= 2);
    // At one exposure, the brighter box is far brighter: its blue is as bright as it gets, and AgX
    // lifts its red toward white.
    CHECK(pinned_bright.at(0) > pinned_dim.at(0) + 64);
}

TEST_CASE_METHOD(GpuTest,
                 "automatic exposure stays within the map's range, so a range of one EV100 shows "
                 "the map at that exposure, however bright its light",
                 "[renderer]") {
    for (const float lux : {2.5F, 250.0F, 25'000.0F}) {
        CAPTURE(lux);
        check_seen_as_pinned(box_under(lux, {.min = 6.0F, .max = 6.0F}), 6.0F);
    }
}

TEST_CASE_METHOD(GpuTest, "a map's compensation brightens its metered exposure by as many stops",
                 "[renderer]") {
    check_seen_as_pinned(box_under(250.0F, {.min = 6.0F, .max = 6.0F, .compensation = 1.0F}), 5.0F);
}

TEST_CASE_METHOD(GpuTest,
                 "exposure moves toward what each image meters at the adaptation speed, from what "
                 "the first image after a map is set meters",
                 "[renderer]") {
    // Two boxes apart, one under 100 times the light of the other.
    auto description = box_map();
    description.sectors.at(0).ambient = white_light;
    auto bright_box = sector("bright",
                             {
                                 {.x = 10.0F, .y = 0.0F},
                                 {.x = 14.0F, .y = 0.0F},
                                 {.x = 14.0F, .y = 4.0F},
                                 {.x = 10.0F, .y = 4.0F},
                             },
                             {"south", "east", "north", "west"});
    bright_box.ambient = wulfhilde::AmbientLight{.color = white_light.color, .lux = 25'000.0F};
    description.sectors.push_back(std::move(bright_box));
    const auto map = make_map(std::move(description));
    const auto dim_camera = facing_east(map);
    auto bright_camera = dim_camera;
    bright_camera.location = {
        .sector = map.find_sector("bright").value_or(wulfhilde::SectorId(0)),
        .point = {.x = 12.0F, .y = 2.0F, .z = 1.5F},
    };
    const auto dim_then_bright = [&](float speed) {
        auto renderer = renderer_of(map, {.validation = true, .exposure_adaptation_speed = speed});
        const auto dim = center_of(render(renderer, dim_camera));
        const auto bright = center_of(render(renderer, bright_camera));
        return std::pair{dim, bright};
    };

    const auto [instant_dim, instant_bright] =
        dim_then_bright(std::numeric_limits<float>::infinity());
    const auto [frozen_dim, frozen_bright] = dim_then_bright(0.0F);

    CAPTURE(describe(instant_dim), describe(instant_bright), describe(frozen_dim),
            describe(frozen_bright));
    CHECK(std::abs(int{instant_dim.at(2)} - int{instant_bright.at(2)}) <= 2);
    CHECK(frozen_dim == instant_dim);
    // Never moving from the dim box's exposure, the bright box shows far brighter, its red lifted
    // toward white.
    CHECK(frozen_bright.at(0) > frozen_dim.at(0) + 64);
}

namespace {

// The box with a doorway 1 m wide in the middle of its east wall, from (4, 1.5) to (4, 2.5), into
// a second box beyond it from (4, 0) to (8, 4) between heights of its own, every wall of which is
// yellow. The doorway's strips take the materials named, or the blue of the east wall without.
wulfhilde::MapDescription doorway_map(float floor_beyond, float ceiling_beyond,
                                      std::optional<std::string> upper = {},
                                      std::optional<std::string> lower = {}) {
    auto description = box_map();
    auto& box = description.sectors.at(0);
    box = sector("box",
                 {
                     {.x = 0.0F, .y = 0.0F},
                     {.x = 4.0F, .y = 0.0F},
                     {.x = 4.0F, .y = 1.5F},
                     {.x = 4.0F, .y = 2.5F},
                     {.x = 4.0F, .y = 4.0F},
                     {.x = 0.0F, .y = 4.0F},
                 },
                 {"south", "east", "east", "east", "north", "west"});
    auto& door = box.loops.at(0).at(2);
    door.id = "box-door";
    if (upper) {
        door.upper = wulfhilde::SurfaceDescription{.material = *upper};
    }
    if (lower) {
        door.lower = wulfhilde::SurfaceDescription{.material = *lower};
    }

    auto beyond = sector("beyond",
                         {
                             {.x = 4.0F, .y = 0.0F},
                             {.x = 8.0F, .y = 0.0F},
                             {.x = 8.0F, .y = 4.0F},
                             {.x = 4.0F, .y = 4.0F},
                             {.x = 4.0F, .y = 2.5F},
                             {.x = 4.0F, .y = 1.5F},
                         },
                         {"north", "north", "north", "north", "north", "north"}, ceiling_beyond);
    beyond.floor.height = floor_beyond;
    beyond.loops.at(0).at(4).id = "beyond-door";
    description.sectors.push_back(std::move(beyond));
    description.portals.push_back({.a = {.id = "box-door"}, .b = {.id = "beyond-door"}});
    return description;
}

// The doorway map without strips, and a third sector north of the one beyond the doorway, from
// (4, 4) to (8, 8), through a second doorway 1 m wide in the north wall of the sector beyond, from
// x = 6.5 to 7.5. Every wall of the third sector is cyan.
wulfhilde::MapDescription two_doorways_map() {
    auto description = doorway_map(0.0F, 3.0F);
    auto& beyond = description.sectors.at(1);
    beyond = sector("beyond",
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
                    std::vector<std::string>(8, "north"));
    beyond.loops.at(0).at(3).id = "beyond-north";
    beyond.loops.at(0).at(6).id = "beyond-door";

    auto third = sector("third",
                        {
                            {.x = 4.0F, .y = 4.0F},
                            {.x = 6.5F, .y = 4.0F},
                            {.x = 7.5F, .y = 4.0F},
                            {.x = 8.0F, .y = 4.0F},
                            {.x = 8.0F, .y = 8.0F},
                            {.x = 4.0F, .y = 8.0F},
                        },
                        std::vector<std::string>(6, "west"));
    third.loops.at(0).at(1).id = "third-south";
    description.sectors.push_back(std::move(third));
    description.portals.push_back({.a = {.id = "beyond-north"}, .b = {.id = "third-south"}});
    return description;
}

} // namespace

TEST_CASE_METHOD(GpuTest, "a camera sees the sector beyond an opening through it", "[renderer]") {
    const auto map = make_map(doorway_map(0.0F, 3.0F));
    auto renderer = renderer_of(map);

    // The doorway is 2 m ahead and 1 m wide, so 8 px either side of the image's middle.
    const auto image = render(renderer, facing_east(map));
    const auto through = center_of(image);
    const auto beside = pixel_at(image, 16, 32);

    CAPTURE(describe(through), describe(beside));
    CHECK(shows(through, yellow));
    CHECK(shows(beside, blue));
}

TEST_CASE_METHOD(GpuTest,
                 "an opening into a sector with a higher floor and a lower ceiling has a lower and "
                 "an upper strip, each with its own material",
                 "[renderer]") {
    // The opening runs from 1 m to 2 m. Seen from 1.5 m high and 2 m away, the upper strip
    // spans rows 8 to 24 and the lower strip rows 40 to 56.
    const auto map = make_map(doorway_map(1.0F, 2.0F, "south", "west"));
    auto renderer = renderer_of(map);

    const auto image = render(renderer, facing_east(map));
    const auto upper = pixel_at(image, 32, 16);
    const auto through = center_of(image);
    const auto lower = pixel_at(image, 32, 48);

    CAPTURE(describe(upper), describe(through), describe(lower));
    CHECK(shows(upper, magenta));
    CHECK(shows(through, yellow));
    CHECK(shows(lower, cyan));
}

TEST_CASE_METHOD(GpuTest, "strips without materials of their own take their wall's", "[renderer]") {
    const auto map = make_map(doorway_map(1.0F, 2.0F));
    auto renderer = renderer_of(map);

    const auto image = render(renderer, facing_east(map));
    const auto upper = pixel_at(image, 32, 16);
    const auto lower = pixel_at(image, 32, 48);

    CAPTURE(describe(upper), describe(lower));
    CHECK(shows(upper, blue));
    CHECK(shows(lower, blue));
}

TEST_CASE_METHOD(GpuTest, "a camera sees a sector through two openings in a row", "[renderer]") {
    const auto map = make_map(two_doorways_map());
    auto renderer = renderer_of(map);

    // Looking through the middle of the first doorway, at (4, 2), and of the second, at (7, 4),
    // the middle of the image reaches the third sector's east wall.
    const auto image = render(renderer, camera_at(map, {.x = 2.5F, .y = 1.0F, .z = 1.5F},
                                                  Angle::radians(std::atan2(2.0F, 3.0F))));
    const auto through = center_of(image);

    CAPTURE(describe(through));
    CHECK(shows(through, cyan));
}

TEST_CASE_METHOD(GpuTest, "a camera standing in an opening sees the sector beyond it",
                 "[renderer]") {
    const auto map = make_map(doorway_map(0.0F, 3.0F));
    auto renderer = renderer_of(map);

    // The camera's sector is all behind it, and the sector beyond fills the image.
    const auto image =
        render(renderer, camera_at(map, {.x = 4.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(0.0F)));
    const auto through = center_of(image);

    CAPTURE(describe(through));
    CHECK(shows(through, yellow));
}
