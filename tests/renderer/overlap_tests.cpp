// Sectors that share space, seen at once: each seen through an opening into the space it shares is
// seen in a view of its own, confined to the opening and to what lies beyond it.
#include "gpu_test.hpp"
#include <wulfhilde/assets/map_assets.hpp>
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
#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using wulfhilde::Angle;
using wulfhilde::Vector2;
using wulfhilde::Vector3;

// The white ambient light of every test map, under which the tests' pinned exposure shows a
// mid-grey surface as a mid-grey.
constexpr wulfhilde::AmbientLight white_light{
    .color = {.r = 1.0F, .g = 1.0F, .b = 1.0F},
    .lux = 250.0F,
};

// One wall of a sector's loop: where it starts, its material, and its id if it is a side of a
// portal.
struct WallSpec {
    Vector2 start;
    std::string_view material;
    std::string_view id{};
};

// A sector bounded by a loop of walls, between a floor and a ceiling with materials of their own.
wulfhilde::SectorDescription sector(std::string id, float floor, std::string floor_material,
                                    float ceiling, std::string ceiling_material,
                                    const std::vector<WallSpec>& walls) {
    wulfhilde::SectorDescription description{
        .id = std::move(id),
        .floor = {.height = floor, .material = std::move(floor_material)},
        .ceiling = {.height = ceiling, .material = std::move(ceiling_material)},
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

// Makes a map from a description that must be valid.
wulfhilde::Map make_map(wulfhilde::MapDescription description) {
    auto map = wulfhilde::Map::create(std::move(description));
    if (!map) {
        FAIL(std::format("the description has problems:\n{}", map.error()));
    }
    return *std::move(map);
}

// The yard and the hall, which share space. The yard, from (0, 0) to (10, 12), has its floor
// 0.5 m up and its ceiling 4 m up, and a notch in its north side from x = 4 to 6, down to y = 6,
// where a hut stands. A door in the hut's south face, from x = 4.5 to 5.5625, leads into the hall,
// whose floor is at 0 m and ceiling 3 m up. The hall is bigger than the hut, and shares the yard's
// space: a room from (2, 6) to (8, 10), wider than the hut, with an arm down its west side from
// x = 1 to 2 to a band across the yard from (1, 3) to (9, 4), between the door and the yard's
// south wall.
//
// The yard's floor is green, its walls and ceiling cyan, and the hut's walls blue. The hall's
// floor is red, its ceiling magenta, and its walls yellow, apart from the band's south wall,
// which faces the yard's south wall and is magenta.
wulfhilde::MapDescription hut_map() {
    return {
        .ambient = white_light,
        .sectors =
            {
                sector(
                    "yard", 0.5F, "yard-floor", 4.0F, "yard-ceiling",
                    {
                        {.start = {.x = 0.0F, .y = 0.0F}, .material = "yard-wall"},
                        {.start = {.x = 10.0F, .y = 0.0F}, .material = "yard-wall"},
                        {.start = {.x = 10.0F, .y = 12.0F}, .material = "yard-wall"},
                        {.start = {.x = 6.0F, .y = 12.0F}, .material = "hut"},
                        {.start = {.x = 6.0F, .y = 6.0F}, .material = "hut"},
                        {.start = {.x = 5.5625F, .y = 6.0F}, .material = "hut", .id = "yard-door"},
                        {.start = {.x = 4.5F, .y = 6.0F}, .material = "hut"},
                        {.start = {.x = 4.0F, .y = 6.0F}, .material = "hut"},
                        {.start = {.x = 4.0F, .y = 12.0F}, .material = "yard-wall"},
                        {.start = {.x = 0.0F, .y = 12.0F}, .material = "yard-wall"},
                    }),
                sector("hall", 0.0F, "hall-floor", 3.0F, "hall-ceiling",
                       {
                           {.start = {.x = 1.0F, .y = 3.0F}, .material = "hall-wall"},
                           {.start = {.x = 9.0F, .y = 3.0F}, .material = "hall-wall"},
                           {.start = {.x = 9.0F, .y = 4.0F}, .material = "band"},
                           {.start = {.x = 2.0F, .y = 4.0F}, .material = "hall-wall"},
                           {.start = {.x = 2.0F, .y = 6.0F}, .material = "hall-wall"},
                           {
                               .start = {.x = 4.5F, .y = 6.0F},
                               .material = "hall-wall",
                               .id = "hall-door",
                           },
                           {.start = {.x = 5.5625F, .y = 6.0F}, .material = "hall-wall"},
                           {.start = {.x = 8.0F, .y = 6.0F}, .material = "hall-wall"},
                           {.start = {.x = 8.0F, .y = 10.0F}, .material = "hall-wall"},
                           {.start = {.x = 1.0F, .y = 10.0F}, .material = "hall-wall"},
                       }),
            },
        .portals = {{.a = {.id = "yard-door"}, .b = {.id = "hall-door"}}},
        .start = {.sector = "yard", .point = {.x = 5.0F, .y = 2.0F, .z = 0.5F}},
    };
}

// A passage under a bridge, which cross seen from above but share no space. The yard, from (0, 0)
// to (4, 8), 6 m high, opens east into the passage, from (4, 1) to (10, 3) and 3 m high, and into
// the ledge, from (4, 5) to (6, 7), whose floor is 3 m up. The ledge opens east onto the bridge,
// which runs from (6, 0) to (8, 7) over the passage, at a floor of its own and up to 6 m.
wulfhilde::MapDescription bridge_map(float bridge_floor) {
    constexpr std::string_view grey = "hall-wall";
    return {
        .ambient = white_light,
        .sectors =
            {
                sector(
                    "yard", 0.0F, "hall-floor", 6.0F, "hall-ceiling",
                    {
                        {.start = {.x = 0.0F, .y = 0.0F}, .material = grey},
                        {.start = {.x = 4.0F, .y = 0.0F}, .material = grey},
                        {.start = {.x = 4.0F, .y = 1.0F}, .material = grey, .id = "yard-passage"},
                        {.start = {.x = 4.0F, .y = 3.0F}, .material = grey},
                        {.start = {.x = 4.0F, .y = 5.0F}, .material = grey, .id = "yard-ledge"},
                        {.start = {.x = 4.0F, .y = 7.0F}, .material = grey},
                        {.start = {.x = 4.0F, .y = 8.0F}, .material = grey},
                        {.start = {.x = 0.0F, .y = 8.0F}, .material = grey},
                    }),
                sector(
                    "passage", 0.0F, "hall-floor", 3.0F, "hall-ceiling",
                    {
                        {.start = {.x = 4.0F, .y = 1.0F}, .material = grey},
                        {.start = {.x = 10.0F, .y = 1.0F}, .material = grey},
                        {.start = {.x = 10.0F, .y = 3.0F}, .material = grey},
                        {.start = {.x = 4.0F, .y = 3.0F}, .material = grey, .id = "passage-yard"},
                    }),
                sector(
                    "ledge", 3.0F, "hall-floor", 6.0F, "hall-ceiling",
                    {
                        {.start = {.x = 4.0F, .y = 5.0F}, .material = grey},
                        {.start = {.x = 6.0F, .y = 5.0F}, .material = grey, .id = "ledge-bridge"},
                        {.start = {.x = 6.0F, .y = 7.0F}, .material = grey},
                        {.start = {.x = 4.0F, .y = 7.0F}, .material = grey, .id = "ledge-yard"},
                    }),
                sector(
                    "bridge", bridge_floor, "hall-floor", 6.0F, "hall-ceiling",
                    {
                        {.start = {.x = 6.0F, .y = 0.0F}, .material = grey},
                        {.start = {.x = 8.0F, .y = 0.0F}, .material = grey},
                        {.start = {.x = 8.0F, .y = 7.0F}, .material = grey},
                        {.start = {.x = 6.0F, .y = 7.0F}, .material = grey, .id = "bridge-ledge"},
                        {.start = {.x = 6.0F, .y = 5.0F}, .material = grey},
                    }),
            },
        .portals =
            {
                {.a = {.id = "yard-passage"}, .b = {.id = "passage-yard"}},
                {.a = {.id = "yard-ledge"}, .b = {.id = "ledge-yard"}},
                {.a = {.id = "ledge-bridge"}, .b = {.id = "bridge-ledge"}},
            },
        .start = {.sector = "yard", .point = {.x = 1.0F, .y = 4.0F}},
    };
}

// The base colour of each test material, by its asset path.
const std::map<std::string, wulfhilde::Color, std::less<>>& material_colors() {
    static const std::map<std::string, wulfhilde::Color, std::less<>> colors{
        {"yard-floor", {.r = 0.05F, .g = 0.8F, .b = 0.05F}},
        {"yard-ceiling", {.r = 0.05F, .g = 0.8F, .b = 0.8F}},
        {"yard-wall", {.r = 0.05F, .g = 0.8F, .b = 0.8F}},
        {"hut", {.r = 0.05F, .g = 0.05F, .b = 0.8F}},
        {"hall-floor", {.r = 0.8F, .g = 0.05F, .b = 0.05F}},
        {"hall-ceiling", {.r = 0.8F, .g = 0.05F, .b = 0.8F}},
        {"hall-wall", {.r = 0.8F, .g = 0.8F, .b = 0.05F}},
        {"band", {.r = 0.8F, .g = 0.05F, .b = 0.8F}},
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

// A camera in a sector of a map, facing by a yaw and a pitch, seeing 90 degrees from the image's
// top to its bottom.
wulfhilde::Camera camera_in(const wulfhilde::Map& map, std::string_view sector, Vector3 point,
                            Angle yaw, Angle pitch = Angle{}) {
    return {
        .location =
            {
                .sector = map.find_sector(sector).value_or(wulfhilde::SectorId{0}),
                .point = point,
            },
        .orientation = wulfhilde::Rotation::from_yaw_pitch_roll(yaw, pitch, Angle{}),
        .vertical_field_of_view = Angle::degrees(90.0F),
    };
}

// A camera in the yard, 4 m south of the hut's door, facing it.
wulfhilde::Camera facing_the_door(const wulfhilde::Map& map) {
    return camera_in(map, "yard", {.x = 5.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(90.0F));
}

// A camera in the hall, 3.5 m north of the door, facing it and looking down at 45 degrees.
wulfhilde::Camera facing_out(const wulfhilde::Map& map) {
    return camera_in(map, "hall", {.x = 5.0F, .y = 9.5F, .z = 1.5F}, Angle::degrees(-90.0F),
                     Angle::degrees(-45.0F));
}

// Renders an image 64 px square, which must succeed.
wulfhilde::Image render(wulfhilde::Renderer& renderer, const wulfhilde::Camera& camera) {
    auto image = renderer.render_image(camera, {.width = 64, .height = 64});
    require_value(image);
    return *std::move(image);
}

// Measures a frame, which must succeed.
wulfhilde::FrameMeasurement measure(const wulfhilde::Map& map, const wulfhilde::Camera& camera) {
    auto renderer = renderer_of(map, {.validation = true, .measures_frames = true});
    auto measurement = renderer.measure_frame(camera, {.width = 64, .height = 64});
    require_value(measurement);
    return *std::move(measurement);
}

using Pixel = std::array<std::uint8_t, 4>;

Pixel pixel_at(const wulfhilde::Image& image, std::size_t x, std::size_t y) {
    const auto start = ((y * image.extent().width) + x) * wulfhilde::Image::bytes_per_pixel;
    Pixel pixel{};
    std::ranges::copy(image.pixels().subspan(start, pixel.size()), pixel.begin());
    return pixel;
}

// Whether a pixel's channels that `channels` names are each well above every other, as a surface
// of one of the test colours shows under white light.
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
constexpr std::array blue{false, false, true};
constexpr std::array magenta{true, false, true};
constexpr std::array yellow{true, true, false};
constexpr std::array cyan{false, true, true};

std::string describe(const Pixel& pixel) {
    return std::format("({}, {}, {}, {})", pixel.at(0), pixel.at(1), pixel.at(2), pixel.at(3));
}

} // namespace

TEST_CASE_METHOD(GpuTest,
                 "a sector seen through an opening into space it shares with the near sector is "
                 "seen in a view of its own",
                 "[renderer]") {
    const auto map = make_map(hut_map());

    const auto into_hall = measure(map, facing_the_door(map));
    const auto out_of_hall = measure(map, facing_out(map));
    const auto away = measure(
        map, camera_in(map, "yard", {.x = 5.0F, .y = 2.0F, .z = 1.5F}, Angle::degrees(-90.0F)));

    CHECK(into_hall.views == 2);
    CHECK(out_of_hall.views == 2);
    CHECK(away.views == 1);
}

TEST_CASE_METHOD(GpuTest,
                 "a sector seen through an opening into space it shares with the near sector shows "
                 "only within the opening",
                 "[renderer]") {
    const auto map = make_map(hut_map());
    auto renderer = renderer_of(map);

    // Beside the hut, up and to the right, the yard's north wall stands 10 m ahead, behind where
    // the hall's ceiling runs over the yard.
    const auto beside = pixel_at(render(renderer, facing_the_door(map)), 42, 25);

    CAPTURE(describe(beside));
    CHECK(shows(beside, cyan));
}

TEST_CASE_METHOD(GpuTest,
                 "the near sector seen out through an opening into space it shares with the far "
                 "sector shows only within the opening",
                 "[renderer]") {
    const auto map = make_map(hut_map());
    auto renderer = renderer_of(map);

    // Down and to the left, the hall's floor lies below where the yard's floor runs through the
    // hall, half a metre up.
    const auto beside = pixel_at(render(renderer, facing_out(map)), 6, 27);

    CAPTURE(describe(beside));
    CHECK(shows(beside, red));
}

TEST_CASE_METHOD(GpuTest,
                 "what lies beyond an opening into shared space is seen through it, not what lies "
                 "between the eye and the opening, nor what the near sector holds behind it",
                 "[renderer]") {
    const auto map = make_map(hut_map());
    auto renderer = renderer_of(map);

    // From the yard, straight through the door to the hall's north wall, past the band's
    // magenta wall, which crosses the yard between the eye and the door.
    const auto into_hall = pixel_at(render(renderer, facing_the_door(map)), 32, 32);
    // From the hall, straight through the door to the yard's south wall, past the band's yellow
    // north wall, which the hall holds beyond the door.
    const auto out_of_hall =
        pixel_at(render(renderer, camera_in(map, "hall", {.x = 5.0F, .y = 9.5F, .z = 1.5F},
                                            Angle::degrees(-90.0F))),
                 32, 32);

    CAPTURE(describe(into_hall), describe(out_of_hall));
    CHECK(shows(into_hall, yellow));
    CHECK(shows(out_of_hall, cyan));
}

TEST_CASE_METHOD(GpuTest,
                 "an opening into shared space whose corners lie behind the eye shows what lies "
                 "beyond it",
                 "[renderer]") {
    const auto map = make_map(hut_map());
    auto renderer = renderer_of(map);

    // From 0.3 m in front of the door and 0.1 m above the yard's floor, looking up at 80 degrees,
    // the door's bottom corners lie behind the eye. Below the middle of the image, the door is
    // seen less than half a metre ahead, and the hall's ceiling through it.
    const auto through =
        pixel_at(render(renderer, camera_in(map, "yard", {.x = 5.0F, .y = 5.7F, .z = 0.6F},
                                            Angle::degrees(90.0F), Angle::degrees(80.0F))),
                 32, 48);

    CAPTURE(describe(through));
    CHECK(shows(through, magenta));
}

TEST_CASE_METHOD(GpuTest,
                 "a pixel split between an opening into shared space and the wall beside it blends "
                 "the two",
                 "[renderer]") {
    const auto map = make_map(hut_map());
    auto renderer = renderer_of(map);

    // The door's east edge runs down the middle of column 36: the hall's yellow wall shows to its
    // left, and the hut's blue face to its right.
    const auto image = render(renderer, facing_the_door(map));
    const auto through = pixel_at(image, 35, 32);
    const auto split = pixel_at(image, 36, 32);
    const auto beside = pixel_at(image, 37, 32);

    CAPTURE(describe(through), describe(split), describe(beside));
    REQUIRE(shows(through, yellow));
    REQUIRE(shows(beside, blue));
    for (std::size_t channel = 0; channel < 3; ++channel) {
        CAPTURE(channel);
        CHECK(split.at(channel) > std::min(through.at(channel), beside.at(channel)));
        CHECK(split.at(channel) < std::max(through.at(channel), beside.at(channel)));
    }
}

TEST_CASE_METHOD(GpuTest,
                 "a bridge over a passage, sharing no space with it, is seen in the same view, and "
                 "one sunk into the passage's space starts a view of its own, as does the passage",
                 "[renderer]") {
    const auto over = make_map(bridge_map(3.0F));
    const auto sunk = make_map(bridge_map(2.5F));
    // From the yard's west side, east and up, seeing the passage's opening and, through the
    // ledge's, the bridge.
    const auto camera = [](const wulfhilde::Map& map) {
        return camera_in(map, "yard", {.x = 1.0F, .y = 4.0F, .z = 2.0F}, Angle{});
    };

    CHECK(measure(over, camera(over)).views == 1);
    CHECK(measure(sunk, camera(sunk)).views == 3);
}
