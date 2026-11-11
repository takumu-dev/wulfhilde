#include "json_file.hpp"

#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/world/text_position.hpp>

#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <string>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

void JsonFile::report(AssetProblemCode code, const pedantjson::Pointer& pointer, std::size_t offset,
                      std::string message) {
    const auto position = document_.get().position(offset);
    problems_.push_back({
        .offset = offset,
        .problem =
            {
                .code = code,
                .message = std::move(message),
                .file = name_,
                .pointer = std::string{pointer.text()},
                .position = TextPosition{.line = position.line, .column = position.column},
            },
    });
}

std::vector<AssetProblem> JsonFile::take_problems() && {
    std::ranges::stable_sort(problems_, {}, &PlacedProblem::offset);
    std::vector<AssetProblem> problems;
    problems.reserve(problems_.size());
    for (auto& placed : problems_) {
        problems.push_back(std::move(placed.problem));
    }
    return problems;
}

std::expected<pedantjson::Document, AssetProblem> parse_json(const std::string& text) {
    auto document = pedantjson::parse(text);
    if (!document) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::syntax_error,
            .message = std::string{document.error().message()},
            .position =
                TextPosition{
                    .line = document.error().position.line,
                    .column = document.error().position.column,
                },
        });
    }
    return std::move(*document);
}

} // namespace wulfhilde::detail
