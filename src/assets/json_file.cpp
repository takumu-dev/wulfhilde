#include "json_file.hpp"

#include <wulfhilde/world/map_problem.hpp>
#include <wulfhilde/world/text_position.hpp>

#include <pedantjson/pedantjson.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

void JsonFile::report(MapProblemCode code, const pedantjson::Pointer& pointer, std::size_t offset,
                      std::string message) {
    place({.code = code, .message = std::move(message), .pointer = std::string{pointer.text()}},
          offset);
}

void JsonFile::report_at_pointer(MapProblem problem) {
    // The map's own checks write their pointers themselves, and every one of them is a pointer;
    // the whole document is the nearest written value to anything.
    const auto pointer =
        pedantjson::Pointer::parse(problem.pointer).value_or(pedantjson::Pointer{});
    const auto offset = document_.get().nearest(pointer).offset();
    place(std::move(problem), offset);
}

std::vector<MapProblem> JsonFile::take_problems() && {
    std::ranges::stable_sort(problems_, {}, &PlacedProblem::offset);
    std::vector<MapProblem> problems;
    problems.reserve(problems_.size());
    for (auto& placed : problems_) {
        problems.push_back(std::move(placed.problem));
    }
    return problems;
}

void JsonFile::place(MapProblem problem, std::size_t offset) {
    const auto position = document_.get().position(offset);
    problem.position = TextPosition{.line = position.line, .column = position.column};
    problems_.push_back({.offset = offset, .problem = std::move(problem)});
}

} // namespace wulfhilde::detail
