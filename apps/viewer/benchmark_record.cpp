#include "benchmark_record.hpp"

#include <wulfhilde/core/assertion.hpp>
#include <wulfhilde/renderer/frame_measurement.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <iterator>
#include <ranges>
#include <ratio>
#include <string>
#include <string_view>
#include <vector>

namespace viewer {
namespace {

/// Writes a text as a JSON string, quoted and escaped.
std::string json_string(std::string_view text) {
    std::string quoted = "\"";
    for (const char character : text) {
        switch (character) {
        case '"':
            quoted += "\\\"";
            break;
        case '\\':
            quoted += "\\\\";
            break;
        case '\n':
            quoted += "\\n";
            break;
        case '\r':
            quoted += "\\r";
            break;
        case '\t':
            quoted += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                std::format_to(std::back_inserter(quoted), "\\u{:04x}",
                               static_cast<unsigned char>(character));
            } else {
                quoted += character;
            }
        }
    }
    quoted += '"';
    return quoted;
}

/// Writes a time in milliseconds, to the microsecond.
std::string milliseconds(std::chrono::nanoseconds time) {
    return std::format("{:.3f}", std::chrono::duration<double, std::milli>(time).count());
}

std::string count(std::size_t value) { return std::format("{}", value); }

/// Writes a measure of every frame as an object of its spread and its frames' values.
template <typename T, typename Write>
std::string spread_object(const std::vector<T>& values, Write write) {
    const auto spread = spread_of<T>(values);
    std::string object = std::format(R"({{"p50": {}, "p99": {}, "max": {}, "frames": [)",
                                     write(spread.p50), write(spread.p99), write(spread.max));
    for (std::size_t index = 0; const auto& value : values) {
        object += index == 0 ? "" : ", ";
        object += write(value);
        ++index;
    }
    object += "]}";
    return object;
}

/// Takes one measure from every frame.
template <typename Measure>
auto each_frame(const std::vector<BenchmarkFrame>& frames, Measure measure) {
    return frames | std::views::transform(measure) | std::ranges::to<std::vector>();
}

} // namespace

std::string format_record(const BenchmarkRecord& record) {
    WULFHILDE_ASSERT(!record.frames.empty(), "a benchmark's record has a frame or more");
    const auto& frames = record.frames;
    const auto& load = record.load;
    std::string json = "{\n";
    const auto member = [&json](std::string_view name, const std::string& value) {
        std::format_to(std::back_inserter(json), "  \"{}\": {},\n", name, value);
    };
    member("map", json_string(record.map));
    member("walk", json_string(record.walk));
    member("width", count(record.extent.width));
    member("height", count(record.extent.height));
    member("samples", count(record.samples));
    member("anisotropy", count(record.anisotropy));
    member("device", json_string(record.device));
    member("driver", json_string(record.driver));
    member("os", json_string(record.os));
    member("conditions", json_string(record.conditions));
    member("load_ms", std::format(R"({{"parse": {}, "map_create": {}, "assets": {}, "upload": {}, )"
                                  R"("first_frame": {}, "total": {}}})",
                                  milliseconds(load.parse), milliseconds(load.map_create),
                                  milliseconds(load.assets), milliseconds(load.upload),
                                  milliseconds(load.first_frame), milliseconds(load.total)));
    std::size_t peak_memory = 0;
    for (const auto& frame : frames) {
        peak_memory = std::max(peak_memory, frame.measurement.device_memory);
    }
    member("peak_gpu_memory_bytes", count(peak_memory));
    member("frame_count", count(frames.size()));
    member("cpu_ms", spread_object(each_frame(frames, &BenchmarkFrame::cpu), milliseconds));
    member("gpu_ms", spread_object(each_frame(frames,
                                              [](const BenchmarkFrame& frame) {
                                                  return frame.measurement.gpu;
                                              }),
                                   milliseconds));

    json += "  \"gpu_ms_by_pass\": {\n";
    const auto& passes = frames.front().measurement.passes;
    for (std::size_t index = 0; const auto& pass : passes) {
        const auto times = each_frame(frames, [index](const BenchmarkFrame& frame) {
            return frame.measurement.passes.at(index).time;
        });
        std::format_to(std::back_inserter(json), "    \"{}\": {}{}\n", pass.name,
                       spread_object(times, milliseconds), index + 1 == passes.size() ? "" : ",");
        ++index;
    }
    json += "  },\n";

    member("views", spread_object(each_frame(frames,
                                             [](const BenchmarkFrame& frame) {
                                                 return frame.measurement.views;
                                             }),
                                  count));
    member("shadow_maps", spread_object(each_frame(frames,
                                                   [](const BenchmarkFrame& frame) {
                                                       return frame.measurement.shadow_maps;
                                                   }),
                                        count));
    std::format_to(
        std::back_inserter(json), "  \"draws\": {}\n}}\n",
        spread_object(
            each_frame(frames, [](const BenchmarkFrame& frame) { return frame.measurement.draws; }),
            count));
    return json;
}

} // namespace viewer
