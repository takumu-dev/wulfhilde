#include "file_reading.hpp"

#include <wulfhilde/assets/asset_problem.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>

namespace wulfhilde::detail {

std::expected<std::string, AssetProblem> read_file(const std::filesystem::path& path) {
    // The status of what a symbolic link names, not of the link.
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (status.type() == std::filesystem::file_type::not_found) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::missing_file,
            .message = "there is no such file",
        });
    }
    if (error) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::unreadable_file,
            .message = std::format("the file can't be read: {}", error.message()),
        });
    }
    if (!std::filesystem::is_regular_file(status)) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::unreadable_file,
            .message = "the file can't be read, since it isn't a regular file",
        });
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::unreadable_file,
            .message = "the file can't be opened",
        });
    }
    std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) {
        return std::unexpected(AssetProblem{
            .code = AssetProblemCode::unreadable_file,
            .message = "the file can't be read to its end",
        });
    }
    return bytes;
}

} // namespace wulfhilde::detail
