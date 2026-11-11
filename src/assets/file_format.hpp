#pragma once

#include "json_file.hpp"

#include <pedantjson/pedantjson.hpp>

#include <string_view>

namespace wulfhilde::detail {

/// What a JSON asset's file begins with: its `format` and the one `version` of it that this version
/// of the engine reads.
struct FileFormat {
    /// The `format`, such as `wulfhilde-map`.
    std::string_view name;

    /// What an asset of the format is, for messages, such as `map`.
    std::string_view noun;

    /// The one `version` read. Before 1.0, any change to the format bumps it, and older files
    /// aren't read.
    int version = 0;
};

/// Checks that a file's outermost value is an object of a format and version, first the format and
/// then the version, reporting only the first of them that isn't.
///
/// \return Whether it is.
[[nodiscard]] bool check_format_and_version(const pedantjson::Value& root, const FileFormat& format,
                                            JsonFile& file);

} // namespace wulfhilde::detail
