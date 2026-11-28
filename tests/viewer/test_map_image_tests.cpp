// The viewer's acceptance suite, in images: each test map rendered from its named cameras, and
// compared with the reference at references/<map>/<camera>.png.
#include "gpu_test.hpp"
#include "image_comparison.hpp"
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/core/rotation.hpp>
#include <wulfhilde/core/vector3.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string_view>
#include <vector>

namespace {

using wulfhilde::Angle;

// A named view of a test map, from which its reference is rendered.
struct TestCamera {
    std::string_view name;

    // The id of the sector the eye is in, and the eye's point in its coordinates.
    std::string_view sector;
    wulfhilde::Vector3 point;

    // The facing, turned from +X as everywhere in the engine.
    Angle yaw;
    Angle pitch;
    Angle roll = Angle::degrees(0.0F);

    Angle vertical_field_of_view = Angle::degrees(60.0F);
    wulfhilde::Extent extent{.width = 640, .height = 360};
};

// A test map's asset path, with its cameras.
struct TestMapCameras {
    std::string_view map;
    std::vector<TestCamera> cameras;
};

const std::vector<TestMapCameras>& test_map_cameras() {
    static const std::vector<TestMapCameras> maps{
        {
            .map = "sectors",
            .cameras =
                {
                    // From the far end of the arm along X, back toward the corner where the arm
                    // along Y opens, showing walls side by side, the floor and the ceiling.
                    {
                        .name = "mapping",
                        .sector = "hall",
                        .point = {.x = 11.0F, .y = 1.0F, .z = 1.65F},
                        .yaw = Angle::degrees(165.0F),
                        .pitch = Angle::degrees(-5.0F),
                    },
                    // From the arm along X, up the stair onto the ledge, with the low doorway
                    // into the nook beside it.
                    {
                        .name = "stairs",
                        .sector = "hall",
                        .point = {.x = 9.0F, .y = 0.6F, .z = 1.65F},
                        .yaw = Angle::degrees(105.0F),
                        .pitch = Angle::degrees(-8.0F),
                    },
                    // From the foot of the arm along Y, up it to the dais on the left and the
                    // pillar on the right, with the gallery's doorway between.
                    {
                        .name = "holes",
                        .sector = "hall",
                        .point = {.x = 2.0F, .y = 4.2F, .z = 1.65F},
                        .yaw = Angle::degrees(85.0F),
                        .pitch = Angle::degrees(-12.0F),
                    },
                },
        },
        {
            .map = "lights",
            .cameras =
                {
                    // Across the hall at its north wall, where the same lamp material glows at its
                    // own 400 nits on the right and at the surface's 80 on the left, above a floor
                    // that they leave unlit.
                    {
                        .name = "emission",
                        .sector = "hall",
                        .point = {.x = 4.0F, .y = 2.0F, .z = 1.65F},
                        .yaw = Angle::degrees(90.0F),
                        .pitch = Angle::degrees(-5.0F),
                    },
                    // From the hall, under the map's ambient light, through the doorway into the
                    // annex under its own dim, warm light, which steps down at the opening.
                    {
                        .name = "ambient",
                        .sector = "hall",
                        .point = {.x = 5.5F, .y = 3.25F, .z = 1.65F},
                        .yaw = Angle::degrees(0.0F),
                        .pitch = Angle::degrees(-15.0F),
                    },
                },
        },
        {
            .map = "overlap",
            .cameras =
                {
                    // From the floor of the chasm, east at the bridge: the passage's mouth under
                    // its walled half, the pier under its open half, and the yard above the
                    // chasm's north wall, seen through an opening into space the hall shares.
                    {
                        .name = "bridge",
                        .sector = "chasm-west",
                        .point = {.x = 2.0F, .y = -2.5F, .z = -1.35F},
                        .yaw = Angle::degrees(10.0F),
                        .pitch = Angle::degrees(15.0F),
                    },
                    // From the yard, at the hut's door, through which the hall shows, wider than
                    // the hut, without showing beside it.
                    {
                        .name = "hut_outside",
                        .sector = "yard",
                        .point = {.x = 6.5F, .y = 3.0F, .z = 1.65F},
                        .yaw = Angle::degrees(75.0F),
                        .pitch = Angle::degrees(5.0F),
                    },
                    // From the hall's north-west, back at the door, through which the yard shows,
                    // with the hall running on past the hut's walls on either side.
                    {
                        .name = "hut_inside",
                        .sector = "hall",
                        .point = {.x = 5.0F, .y = 16.0F, .z = 1.65F},
                        .yaw = Angle::degrees(-70.0F),
                        .pitch = Angle::degrees(-5.0F),
                    },
                },
        },
        {
            .map = "outdoors",
            .cameras =
                {
                    // From the yard's west side, east and up across the opening into the field,
                    // under the yard's blue sky and the field's dimmer amber one.
                    {
                        .name = "skies",
                        .sector = "yard",
                        .point = {.x = 1.0F, .y = 4.0F, .z = 1.65F},
                        .yaw = Angle::degrees(0.0F),
                        .pitch = Angle::degrees(35.0F),
                    },
                    // From the yard's edge, down into the abyss beside the bridge, whose sky floor
                    // shows the blue backdrop's lower half.
                    {
                        .name = "abyss",
                        .sector = "yard",
                        .point = {.x = 1.5F, .y = 0.25F, .z = 1.65F},
                        .yaw = Angle::degrees(-65.0F),
                        .pitch = Angle::degrees(-55.0F),
                    },
                },
        },
        {
            .map = "slopes",
            .cameras =
                {
                    // From the hall, north-east up the ramp onto the landing, the floor's grid
                    // running along the ramp's diagonal hinge and its side walls' bottoms sloping.
                    {
                        .name = "ramp",
                        .sector = "hall",
                        .point = {.x = 8.0F, .y = 4.5F, .z = 1.65F},
                        .yaw = Angle::degrees(45.0F),
                        .pitch = Angle::degrees(-10.0F),
                    },
                    // From the hall, north and up into the attic, whose ceiling slopes down from
                    // the hall's height to its far wall.
                    {
                        .name = "ceiling",
                        .sector = "hall",
                        .point = {.x = 6.0F, .y = 5.5F, .z = 1.65F},
                        .yaw = Angle::degrees(90.0F),
                        .pitch = Angle::degrees(20.0F),
                    },
                    // From the hall, west into the sector whose floor rises west and whose
                    // ceiling falls north, under the triangle of the upper strip that its falling
                    // ceiling leaves on the hall's side.
                    {
                        .name = "opposed",
                        .sector = "hall",
                        .point = {.x = 4.0F, .y = 4.0F, .z = 1.65F},
                        .yaw = Angle::degrees(180.0F),
                        .pitch = Angle::degrees(0.0F),
                    },
                },
        },
    };
    return maps;
}

ImageReferences references() {
    return {
        .references = WULFHILDE_VIEWER_REFERENCES,
        .failures = WULFHILDE_IMAGE_FAILURES,
    };
}

// Renders a map from one of its cameras, and compares the image with the camera's reference.
void check_camera(wulfhilde::Renderer& renderer, const wulfhilde::Map& map, std::string_view name,
                  const TestCamera& camera) {
    const auto sector = map.find_sector(camera.sector);
    REQUIRE(sector.has_value());
    const auto image = renderer.render_image(
        {
            .location = {.sector = sector.value_or(wulfhilde::SectorId(0)), .point = camera.point},
            .orientation =
                wulfhilde::Rotation::from_yaw_pitch_roll(camera.yaw, camera.pitch, camera.roll),
            .vertical_field_of_view = camera.vertical_field_of_view,
        },
        camera.extent);
    require_value(image);

    const auto failure = check_reference_image(*image, name, references());

    INFO(failure.value_or(""));
    CHECK(!failure);
}

} // namespace

TEST_CASE_METHOD(GpuTest, "every test map's cameras see what their references show", "[viewer]") {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());
    auto renderer = wulfhilde::Renderer::create_headless(image_test_settings);
    require_value(renderer);

    for (const auto& [map_path, cameras] : test_map_cameras()) {
        const auto map = wulfhilde::load_map(*root, map_path);
        REQUIRE(map.has_value());
        const auto assets = wulfhilde::load_map_assets(*root, *map);
        REQUIRE(assets.has_value());
        require_value(renderer->set_map(*map, *assets));

        for (const auto& camera : cameras) {
            const auto name = std::format("{}/{}", map_path, camera.name);
            DYNAMIC_SECTION(name) { check_camera(*renderer, *map, name, camera); }
        }
    }
}
