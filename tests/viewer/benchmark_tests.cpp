// A benchmark played on a test map's walk, measured by the renderer, and written as its record.
#include "benchmark.hpp"

#include "benchmark_record.hpp"
#include "benchmark_walks.hpp"
#include "controller.hpp"
#include "gpu_test.hpp"
#include "scripted_walk.hpp"
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/load_map_assets.hpp>
#include <wulfhilde/renderer/extent.hpp>
#include <wulfhilde/renderer/renderer.hpp>
#include <wulfhilde/renderer/renderer_settings.hpp>
#include <wulfhilde/world/map.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <pedantjson/pedantjson.hpp>

#include <chrono>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// The settings of a benchmark's renderer, under validation as every gpu test's is.
constexpr wulfhilde::RendererSettings benchmark_settings{
    .validation = true,
    .measures_frames = true,
};

// A renderer that measures frames, under validation, with a test map and its assets set, which
// must all succeed.
wulfhilde::Renderer benchmark_renderer_of(const wulfhilde::Map& map) {
    const auto root = wulfhilde::ContentRoot::open(WULFHILDE_TEST_CONTENT);
    REQUIRE(root.has_value());
    const auto assets = wulfhilde::load_map_assets(*root, map);
    REQUIRE(assets.has_value());
    auto renderer = wulfhilde::Renderer::create_headless(benchmark_settings);
    require_value(renderer);
    require_value(renderer->set_map(map, *assets));
    return *std::move(renderer);
}

// What a member of an object reads as, if the object has the member and it reads.
template <typename Read>
auto read_member(const pedantjson::Object& object, std::string_view name, Read read) {
    return object.find(name).and_then(
        [&read](const pedantjson::Member& member) { return std::invoke(read, member.value); });
}

// A value that must be an object, or an empty object, failing the test, if it isn't one.
pedantjson::Object object_of(const pedantjson::Value& value) {
    const auto object = value.as_object();
    CHECK(object.has_value());
    return object.value_or(pedantjson::Object{});
}

// A member of an object that must be an object, or an empty object if it isn't.
pedantjson::Object object_member(const pedantjson::Object& object, std::string_view name) {
    CAPTURE(name);
    const auto member = read_member(object, name, &pedantjson::Value::as_object);
    CHECK(member.has_value());
    return member.value_or(pedantjson::Object{});
}

// A member of an object that must be a number, or not a number if it isn't.
double number_member(const pedantjson::Object& object, std::string_view name) {
    CAPTURE(name);
    const auto member = read_member(object, name, &pedantjson::Value::as_number);
    CHECK(member.has_value());
    return member
        .transform([](const pedantjson::Number& number) {
            return number.to<double>().value_or(std::numeric_limits<double>::quiet_NaN());
        })
        .value_or(std::numeric_limits<double>::quiet_NaN());
}

// A member of an object that must be a string, or an empty string if it isn't.
std::string string_member(const pedantjson::Object& object, std::string_view name) {
    CAPTURE(name);
    const auto member = read_member(object, name, &pedantjson::Value::as_string);
    CHECK(member.has_value());
    return std::string(member.value_or(""));
}

// Checks that a measure's spread is in order, no value below zero, with a value for every frame.
void check_spread(const pedantjson::Object& measure, std::size_t frames) {
    const auto p50 = number_member(measure, "p50");
    const auto p99 = number_member(measure, "p99");
    const auto max = number_member(measure, "max");
    CHECK(0.0 <= p50);
    CHECK(p50 <= p99);
    CHECK(p99 <= max);
    const auto values = read_member(measure, "frames", &pedantjson::Value::as_array);
    CHECK(values.value_or(pedantjson::Array{}).size() == frames);
}

// Checks what a record says of what was measured, on what, and how.
void check_description(const pedantjson::Object& record) {
    for (const std::string_view name : {"map", "walk", "device", "driver", "os", "conditions"}) {
        CHECK(!string_member(record, name).empty());
    }
}

// Checks what a record says of how its frames were drawn, and how many.
void check_frames(const pedantjson::Object& record, std::size_t frame_count) {
    CHECK(number_member(record, "width") == 160.0);
    CHECK(number_member(record, "height") == 90.0);
    CHECK(number_member(record, "samples") == 4.0);
    CHECK(number_member(record, "anisotropy") == 16.0);
    CHECK(number_member(record, "peak_gpu_memory_bytes") > 0.0);
    CHECK(number_member(record, "frame_count") == static_cast<double>(frame_count));
}

// Checks a record's stages of loading, and the spread of each measure of its frames.
void check_measures(const pedantjson::Object& record, std::size_t frame_count) {
    const auto load = object_member(record, "load_ms");
    for (const std::string_view name :
         {"parse", "map_create", "assets", "upload", "first_frame", "total"}) {
        CHECK(number_member(load, name) >= 0.0);
    }
    for (const std::string_view name : {"cpu_ms", "gpu_ms", "views", "shadow_maps", "draws"}) {
        CAPTURE(name);
        check_spread(object_member(record, name), frame_count);
    }
    const auto passes = object_member(record, "gpu_ms_by_pass");
    CHECK(passes.size() == 4);
    for (const auto& pass : passes) {
        CAPTURE(pass.key.name());
        check_spread(object_of(pass.value), frame_count);
    }
}

} // namespace

// The full tour at 1920x1080 takes minutes on lavapipe, so it plays at a small size here, and at
// 1920x1080 only where WULFHILDE_BENCHMARK_TESTS is on.
TEST_CASE_METHOD(GpuTest, "the sectors tour plays headless and writes a valid record", "[viewer]") {
    constexpr wulfhilde::Extent extent{.width = 160, .height = 90};
    const auto map = load_test_map("sectors");
    auto renderer = benchmark_renderer_of(map);
    const auto walk = viewer::find_benchmark_walk("sectors", "tour");
    REQUIRE(walk.has_value());

    auto frames = viewer::play_benchmark(renderer, viewer::walk_benchmark(map, *walk), extent);
    require_value(frames);
    const auto json = viewer::format_record({
        .map = "sectors",
        .walk = "tour",
        .extent = extent,
        .samples = benchmark_settings.sample_count,
        .anisotropy = benchmark_settings.anisotropy,
        .device = std::string(renderer.device_name()),
        .driver = std::string(renderer.driver()),
        .os = viewer::operating_system(),
        .conditions = "in a test",
        .frames = std::move(*frames),
    });

    auto document = pedantjson::parse(json);
    if (!document) {
        FAIL(std::format("the record isn't JSON: {}\n{}", document.error().message(), json));
    }
    const auto record = object_of(document->root());
    check_description(record);
    check_frames(record, viewer::frame_count(*walk));
    check_measures(record, viewer::frame_count(*walk));
}

TEST_CASE_METHOD(GpuTest,
                 "a benchmark frame's CPU time is its simulation's and the renderer's submission's",
                 "[viewer]") {
    const auto map = load_test_map("sectors");
    auto renderer = benchmark_renderer_of(map);
    const std::vector<viewer::WalkedFrame> walked{
        {
            .eye = viewer::Controller::at_start(map).eye(map),
            .simulation = std::chrono::milliseconds{5},
        },
    };

    const auto frames = viewer::play_benchmark(renderer, walked, {.width = 16, .height = 9});

    require_value(frames);
    REQUIRE(frames->size() == 1);
    const auto& frame = frames->front();
    CHECK(frame.cpu == std::chrono::milliseconds{5} + frame.measurement.submission);
    CHECK(frame.measurement.submission > std::chrono::nanoseconds{});
}
