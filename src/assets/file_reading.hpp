#pragma once

#include <wulfhilde/assets/asset_problem.hpp>

#include <expected>
#include <filesystem>
#include <string>

namespace wulfhilde::detail {

/// Reads the whole of a file.
///
/// \param path The file's path.
/// \return The file's bytes, or the problem of the whole file that kept it from being read, which
///         names no file.
[[nodiscard]] std::expected<std::string, AssetProblem> read_file(const std::filesystem::path& path);

} // namespace wulfhilde::detail
