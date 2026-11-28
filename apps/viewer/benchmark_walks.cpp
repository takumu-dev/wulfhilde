#include "benchmark_walks.hpp"

#include "command_line.hpp"
#include "controller.hpp"
#include <wulfhilde/core/angle.hpp>
#include <wulfhilde/world/map.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace viewer {
namespace {

/// The controller's ticks in a time, in seconds.
constexpr std::size_t ticks(double seconds) { return static_cast<std::size_t>(seconds * 240.0); }

constexpr Actions forward{.forward = 1.0F};
constexpr Actions right{.strafe = -1.0F};

/// The tour of `sectors`, 19.5 s long: down the hall's arm along X to its far end, across to where
/// the mapping camera stands, and back down the arm; up the arm along Y and through the doorway
/// into the gallery, across it into the bay and back; then back through the doorway to the start,
/// facing as the start faces.
constexpr std::array sectors_tour{
    BenchmarkStep{.actions = forward, .ticks = ticks(2.25)},
    BenchmarkStep{.actions = right, .ticks = ticks(0.25)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(180.0F), .ticks = ticks(1.5)},
    BenchmarkStep{.actions = forward, .ticks = ticks(2.25)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(-90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.actions = forward, .ticks = ticks(3.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(-90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.actions = forward, .ticks = ticks(1.5)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(180.0F), .ticks = ticks(1.5)},
    BenchmarkStep{.actions = forward, .ticks = ticks(1.5)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.actions = forward, .ticks = ticks(2.75)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(90.0F), .ticks = ticks(0.75)},
};

/// The tour of `outdoors`, 11.25 s long: south from the yard across the bridge over the abyss to
/// the ledge, back into the yard, and east into the field under its own sky, turning to look back.
constexpr std::array outdoors_tour{
    BenchmarkStep{.actions = forward, .ticks = ticks(3.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(180.0F), .ticks = ticks(1.5)},
    BenchmarkStep{.actions = forward, .ticks = ticks(2.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(-90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.actions = forward, .ticks = ticks(2.5)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(180.0F), .ticks = ticks(1.5)},
};

/// The tour of `lights`, 6.75 s long: east across the hall and through the doorway into the annex,
/// back into the hall, and round to stand facing the lamps on its north wall.
constexpr std::array lights_tour{
    BenchmarkStep{.actions = forward, .ticks = ticks(2.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(180.0F), .ticks = ticks(1.5)},
    BenchmarkStep{.actions = forward, .ticks = ticks(1.5)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(-90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.ticks = ticks(1.0)},
};

/// The tour of `overlap`, 21.4 s long: from the yard through the hut's door into the hall, looking
/// all round it, and back out across the yard and the bridge to the bank; back into the yard, then
/// west, down into the chasm beside the bridge, and east under the bridge through the passage to
/// the chasm beyond.
constexpr std::array overlap_tour{
    BenchmarkStep{.actions = forward, .ticks = ticks(2.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(360.0F), .ticks = ticks(3.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(180.0F), .ticks = ticks(1.5)},
    BenchmarkStep{.actions = forward, .ticks = ticks(4.5)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(180.0F), .ticks = ticks(1.5)},
    BenchmarkStep{.actions = forward, .ticks = ticks(2.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.actions = forward, .ticks = ticks(1.0)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.actions = forward, .ticks = ticks(1.15)},
    BenchmarkStep{.turn = wulfhilde::Angle::degrees(90.0F), .ticks = ticks(0.75)},
    BenchmarkStep{.actions = forward, .ticks = ticks(2.5)},
};

/// Every benchmark walk, by its map, in the order of the maps and then of their walks.
constexpr std::array walks{
    BenchmarkWalk{.map = "lights", .name = "tour", .steps = lights_tour},
    BenchmarkWalk{.map = "outdoors", .name = "tour", .steps = outdoors_tour},
    BenchmarkWalk{.map = "overlap", .name = "tour", .steps = overlap_tour},
    BenchmarkWalk{.map = "sectors", .name = "tour", .steps = sectors_tour},
};

/// Joins names as prose: "a", "a or b", "a, b or c".
std::string join_names(const std::vector<std::string_view>& names) {
    std::string joined;
    for (std::size_t index = 0; const auto name : names) {
        if (index > 0) {
            joined += index + 1 == names.size() ? " or " : ", ";
        }
        joined += name;
        ++index;
    }
    return joined;
}

} // namespace

std::expected<BenchmarkWalk, UsageError> find_benchmark_walk(std::string_view map,
                                                             std::string_view name) {
    std::vector<std::string_view> names;
    for (const auto& walk : walks) {
        if (walk.map != map) {
            continue;
        }
        if (walk.name == name) {
            return walk;
        }
        names.push_back(walk.name);
    }
    if (names.empty()) {
        return std::unexpected(UsageError{
            .code = UsageErrorCode::unknown_walk,
            .message = std::format("{} has no benchmark walk to play", map),
        });
    }
    return std::unexpected(UsageError{
        .code = UsageErrorCode::unknown_walk,
        .message = std::format("--benchmark names no benchmark walk of {}: {}, only {}", map, name,
                               join_names(names)),
    });
}

std::size_t frame_count(const BenchmarkWalk& walk) noexcept {
    std::size_t total = 0;
    for (const auto& step : walk.steps) {
        total += step.ticks;
    }
    return total / ticks_per_frame;
}

std::vector<WalkedFrame> walk_benchmark(const wulfhilde::Map& map, const BenchmarkWalk& walk) {
    std::vector<WalkedFrame> frames;
    frames.reserve(frame_count(walk));
    auto controller = Controller::at_start(map);
    std::size_t ticks_in_frame = 0;
    auto frame_start = std::chrono::steady_clock::now();
    for (const auto& step : walk.steps) {
        const auto turn = step.turn / static_cast<float>(step.ticks);
        for (std::size_t tick = 0; tick < step.ticks; ++tick) {
            controller.look(turn, {});
            controller.tick(map, step.actions);
            if (++ticks_in_frame < ticks_per_frame) {
                continue;
            }
            ticks_in_frame = 0;
            const auto eye = controller.eye(map);
            const auto now = std::chrono::steady_clock::now();
            frames.push_back({
                .eye = eye,
                .simulation =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(now - frame_start),
            });
            frame_start = now;
        }
    }
    return frames;
}

} // namespace viewer
