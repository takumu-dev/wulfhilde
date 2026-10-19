#include <wulfhilde/assets/load_map.hpp>

#include "json_file.hpp"
#include "map_decoding.hpp"
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/world/asset_path.hpp>
#include <wulfhilde/world/map.hpp>
#include <wulfhilde/world/map_load_error.hpp>
#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/text_position.hpp>

#include <pedantjson/pedantjson.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace wulfhilde {
namespace {

/// Reads the whole text of a file.
///
/// \return The text, or the problem of the whole file that kept it from being read.
std::expected<std::string, MapProblem> read_text(const std::filesystem::path& path) {
    // The status of what a symbolic link names, not of the link.
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (status.type() == std::filesystem::file_type::not_found) {
        return std::unexpected(
            MapProblem{.code = MapProblemCode::missing_file, .message = "there is no such file"});
    }
    if (error) {
        return std::unexpected(MapProblem{
            .code = MapProblemCode::unreadable_file,
            .message = std::format("the file can't be read: {}", error.message()),
        });
    }
    if (!std::filesystem::is_regular_file(status)) {
        return std::unexpected(MapProblem{
            .code = MapProblemCode::unreadable_file,
            .message = "the file can't be read, since it isn't a regular file",
        });
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(MapProblem{
            .code = MapProblemCode::unreadable_file,
            .message = "the file can't be opened",
        });
    }
    std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        return std::unexpected(MapProblem{
            .code = MapProblemCode::unreadable_file,
            .message = "the file can't be read to its end",
        });
    }
    return text;
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
    auto text = read_text(root.directory() / error.file);
    if (!text) {
        return fail(std::move(text.error()));
    }

    // Phase 1: the syntax. The document refers to its own copy of the text, and properties are
    // copied from this one.
    const auto document = pedantjson::parse(*text);
    if (!document) {
        return fail({
            .code = MapProblemCode::syntax_error,
            .message = std::string{document.error().message()},
            .position =
                TextPosition{
                    .line = document.error().position.line,
                    .column = document.error().position.column,
                },
        });
    }

    // Phase 2: the structure, beginning with whether the file is a map of this version at all.
    detail::JsonFile file(*text, *document);
    auto description = detail::decode_map(file);
    if (!description || file.has_problems()) {
        error.problems = std::move(file).take_problems();
        return std::unexpected(std::move(error));
    }

    // Phase 3: the map's own rules, positioned by the values they point at.
    auto map = Map::create(std::move(*description));
    if (!map) {
        for (auto& problem : map.error().problems) {
            file.report_at_pointer(std::move(problem));
        }
        error.problems = std::move(file).take_problems();
        return std::unexpected(std::move(error));
    }
    return std::move(*map);
}

} // namespace wulfhilde
