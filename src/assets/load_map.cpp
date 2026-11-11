#include <wulfhilde/assets/load_map.hpp>

#include "file_reading.hpp"
#include "json_file.hpp"
#include "map_decoding.hpp"
#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/world/asset_path.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_load_error.hpp>
#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/text_position.hpp>

#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde {
namespace {

/// Returns the code of a map's problem that a problem a map's file may have is.
MapProblemCode map_problem_code(AssetProblemCode code) {
    switch (code) {
    case AssetProblemCode::missing_file:
        return MapProblemCode::missing_file;
    case AssetProblemCode::unreadable_file:
        return MapProblemCode::unreadable_file;
    case AssetProblemCode::syntax_error:
        return MapProblemCode::syntax_error;
    case AssetProblemCode::unknown_format:
        return MapProblemCode::unknown_format;
    case AssetProblemCode::unsupported_version:
        return MapProblemCode::unsupported_version;
    case AssetProblemCode::wrong_type:
        return MapProblemCode::wrong_type;
    case AssetProblemCode::wrong_length:
        return MapProblemCode::wrong_length;
    case AssetProblemCode::missing_member:
        return MapProblemCode::missing_member;
    case AssetProblemCode::unknown_member:
        return MapProblemCode::unknown_member;
    case AssetProblemCode::duplicate_member:
        return MapProblemCode::duplicate_member;
    case AssetProblemCode::number_out_of_range:
        return MapProblemCode::number_out_of_range;
    case AssetProblemCode::conflicting_members:
    case AssetProblemCode::member_without_texture:
    case AssetProblemCode::unknown_channel:
    case AssetProblemCode::invalid_asset_path:
    case AssetProblemCode::color_out_of_range:
    case AssetProblemCode::light_level_out_of_range:
    case AssetProblemCode::value_out_of_range:
    case AssetProblemCode::invalid_image:
    case AssetProblemCode::image_too_large:
    case AssetProblemCode::alpha_in_color_texture:
    case AssetProblemCode::grey_normal_texture:
    case AssetProblemCode::missing_channel:
    case AssetProblemCode::no_such_channel:
        break;
    }
    // Only the problems of materials and images are left, which the map's decoder never reports.
    std::unreachable();
}

/// Returns a problem of a map's file as a problem of the map.
MapProblem map_problem(AssetProblem problem) {
    return {
        .code = map_problem_code(problem.code),
        .message = std::move(problem.message),
        .pointer = std::move(problem.pointer),
        .position = problem.position,
    };
}

/// Places each problem that `Map::create` found at the value its pointer names or, if the file
/// never wrote that value, at the nearest value on the way to it that the file did, and orders them
/// by their positions.
std::vector<MapProblem> place_problems(std::vector<MapProblem> problems,
                                       const pedantjson::Document& document) {
    std::vector<std::pair<std::size_t, MapProblem>> placed;
    placed.reserve(problems.size());
    for (auto& problem : problems) {
        // The map's own checks write their pointers themselves, and every one of them is a
        // pointer; the whole document is the nearest written value to anything.
        const auto pointer =
            pedantjson::Pointer::parse(problem.pointer).value_or(pedantjson::Pointer{});
        const auto offset = document.nearest(pointer).offset();
        const auto position = document.position(offset);
        problem.position = TextPosition{.line = position.line, .column = position.column};
        placed.emplace_back(offset, std::move(problem));
    }
    std::ranges::stable_sort(placed, {}, &std::pair<std::size_t, MapProblem>::first);
    std::vector<MapProblem> ordered;
    ordered.reserve(placed.size());
    for (auto& [offset, problem] : placed) {
        ordered.push_back(std::move(problem));
    }
    return ordered;
}

} // namespace

std::expected<Map, MapLoadError> load_map(const ContentRoot& root, std::string_view path) {
    MapLoadError error{.file = std::format("{}{}", path, map_extension)};
    const auto fail = [&error](MapProblem problem) {
        error.problems.push_back(std::move(problem));
        return std::unexpected(std::move(error));
    };

    // A path that isn't an asset path may lead out of the content root.
    if (!is_asset_path(path)) {
        return fail({
            .code = MapProblemCode::invalid_asset_path,
            .message = std::format(R"("{}" isn't an asset path, which is segments of a-z, 0-9, _ )"
                                   "and - separated by / and without an extension",
                                   path),
        });
    }
    auto text = detail::read_file(root.directory() / error.file);
    if (!text) {
        return fail(map_problem(std::move(text.error())));
    }

    // Phase 1: the syntax. The document refers to its own copy of the text, and properties are
    // copied from this one.
    const auto document = detail::parse_json(*text);
    if (!document) {
        return fail(map_problem(document.error()));
    }

    // Phase 2: the structure, beginning with whether the file is a map of this version at all.
    detail::JsonFile file(error.file, *text, *document);
    auto description = detail::decode_map(file);
    if (!description || file.has_problems()) {
        for (auto& problem : std::move(file).take_problems()) {
            error.problems.push_back(map_problem(std::move(problem)));
        }
        return std::unexpected(std::move(error));
    }

    // Phase 3: the map's own rules, positioned by the values they point at.
    auto map = Map::create(std::move(*description));
    if (!map) {
        error.problems = place_problems(std::move(map.error().problems), *document);
        return std::unexpected(std::move(error));
    }
    return std::move(*map);
}

} // namespace wulfhilde
