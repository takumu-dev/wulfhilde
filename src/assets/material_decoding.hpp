#pragma once

#include "json_file.hpp"
#include "object_reader.hpp"
#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>

#include <optional>
#include <string>

namespace wulfhilde::detail {

/// A texture as a material's file names it: an image's asset path, and the value that writes it.
struct TextureReference {
    /// The image's asset path, whose syntax is checked.
    std::string path;

    /// The value that writes it, where a problem with the image as this property reads it lies.
    Field field;
};

/// A channel as a single-value property names it, and the value that writes it.
struct ChannelReference {
    /// The channel.
    Channel channel = Channel::r;

    /// The value that writes it.
    Field field;
};

/// A material's base colour, as its file writes it.
struct ColorDescription {
    /// What the texture is multiplied by.
    Color factor{.r = 1.0F, .g = 1.0F, .b = 1.0F};

    /// The texture, or nothing for none.
    std::optional<TextureReference> texture{};
};

/// A single-value property, as a material's file writes it.
struct ValueDescription {
    /// What the texture is multiplied by.
    float factor = 1.0F;

    /// The texture, or nothing for none.
    std::optional<TextureReference> texture{};

    /// The channel of the texture it reads, or nothing if the file names none.
    std::optional<ChannelReference> channel{};
};

/// A material's normal texture, as its file writes it.
struct NormalDescription {
    /// The texture.
    TextureReference texture;

    /// What the texture's u and v components are multiplied by.
    float scale = 1.0F;
};

/// A material's ambient occlusion, as its file writes it.
struct OcclusionDescription {
    /// The occlusion.
    ValueDescription value;

    /// How strongly it applies.
    float strength = 1.0F;
};

/// A material's emission, as its file writes it.
struct EmissionDescription {
    /// The luminance of white, in nits.
    float nits = 0.0F;

    /// The colour, as written or made from the temperature.
    Color color{.r = 1.0F, .g = 1.0F, .b = 1.0F};

    /// The texture, or nothing for none.
    std::optional<TextureReference> texture{};
};

/// A material as its file writes it, with its textures still named by asset path.
struct MaterialDescription {
    /// The base colour.
    ColorDescription color{};

    /// The roughness.
    ValueDescription roughness{};

    /// The metalness.
    ValueDescription metallic{};

    /// The normal texture, or nothing for none.
    std::optional<NormalDescription> normal{};

    /// The ambient occlusion, or nothing for none.
    std::optional<OcclusionDescription> occlusion{};

    /// The emission, or nothing for none.
    std::optional<EmissionDescription> emission{};

    /// The size of one repeat of the textures, or nothing if the file writes none.
    std::optional<Vector2> size{};

    /// Game-defined data: the JSON text of an object, or empty for none.
    std::string properties{};
};

/// Decodes a material's file into the material's description, reporting every problem in its
/// structure, every value outside its range, and every texture's asset path that breaks the syntax
/// of one.
///
/// A file that isn't a material of this version of the engine is reported as that alone, since
/// nothing else about it would mean anything.
///
/// \param file The file.
/// \return The description, which is complete only if no problem was reported, or nothing if the
///         file isn't a material of this version.
[[nodiscard]] std::optional<MaterialDescription> decode_material(JsonFile& file);

} // namespace wulfhilde::detail
