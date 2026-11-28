#pragma once

#include "json_file.hpp"
#include "object_reader.hpp"

#include <optional>

namespace wulfhilde::detail {

/// A backdrop as its file writes it, with its image still named by asset path.
struct BackdropDescription {
    /// The image.
    ImageReference image;

    /// The luminance of white, in nits.
    float nits = 0.0F;
};

/// Decodes a backdrop's file into the backdrop's description, reporting every problem in its
/// structure, a brightness below 0, and an image's asset path that breaks the syntax of one.
///
/// A file that isn't a backdrop of this version of the engine is reported as that alone, since
/// nothing else about it would mean anything.
///
/// \param file The file.
/// \return The description, or nothing if the file has any problem.
[[nodiscard]] std::optional<BackdropDescription> decode_backdrop(JsonFile& file);

} // namespace wulfhilde::detail
