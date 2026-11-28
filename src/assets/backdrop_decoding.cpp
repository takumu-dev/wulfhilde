#include "backdrop_decoding.hpp"

#include "file_format.hpp"
#include "json_file.hpp"
#include "object_reader.hpp"

#include <pedantjson/pedantjson.hpp>

#include <optional>
#include <tuple>
#include <utility>

namespace wulfhilde::detail {
namespace {

/// What a backdrop's file begins with.
constexpr FileFormat backdrop_format{
    .name = "wulfhilde-backdrop",
    .noun = "backdrop",
    .version = 1,
};

} // namespace

std::optional<BackdropDescription> decode_backdrop(JsonFile& file) {
    const auto root = file.document().root();
    if (!check_format_and_version(root, backdrop_format, file)) {
        return std::nullopt;
    }

    ObjectReader backdrop(root, pedantjson::Pointer{}, "backdrop", file);
    // Both are checked already. The schema an editor checks the file against is the editor's
    // business.
    std::ignore = backdrop.required("format", read_string);
    std::ignore = backdrop.required("version", read_number);
    std::ignore = backdrop.optional("$schema", read_string);

    auto image = backdrop.required("image", read_image_reference);
    const auto nits = backdrop.required("nits", read_nits);
    backdrop.finish();
    if (!image || !nits || file.has_problems()) {
        return std::nullopt;
    }
    return BackdropDescription{.image = std::move(*image), .nits = *nits};
}

} // namespace wulfhilde::detail
