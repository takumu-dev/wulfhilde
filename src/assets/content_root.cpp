#include <wulfhilde/assets/content_root.hpp>

#include <wulfhilde/assets/asset_error.hpp>

#include <expected>
#include <filesystem>
#include <format>
#include <system_error>

namespace wulfhilde {

std::expected<ContentRoot, AssetError> ContentRoot::open(const std::filesystem::path& directory) {
    // The status of what a symbolic link names, not of the link.
    std::error_code error;
    const auto status = std::filesystem::status(directory, error);
    if (status.type() == std::filesystem::file_type::not_found) {
        return std::unexpected(AssetError{
            .code = AssetErrorCode::missing_content_root,
            .message = std::format(R"(no directory is at "{}")", directory.string()),
        });
    }
    if (!std::filesystem::is_directory(status)) {
        return std::unexpected(AssetError{
            .code = AssetErrorCode::not_a_directory,
            .message = error ? std::format(R"("{}" can't be opened as a directory: {})",
                                           directory.string(), error.message())
                             : std::format(R"("{}" isn't a directory)", directory.string()),
        });
    }
    // A relative directory that exists is under a working directory that exists.
    return ContentRoot(std::filesystem::absolute(directory, error));
}

} // namespace wulfhilde
