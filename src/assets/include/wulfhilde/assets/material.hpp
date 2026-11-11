/// \file
/// A material: how a surface responds to light, as its asset describes it.

#pragma once

#include <wulfhilde/core/color.hpp>
#include <wulfhilde/core/vector2.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace wulfhilde {

/// One channel of an image, which a single-value property reads.
enum class Channel : std::uint8_t {
    r, ///< Red, or the grey of a grey image.
    g, ///< Green, or the grey of a grey image.
    b, ///< Blue, or the grey of a grey image.
    a, ///< Alpha.
};

/// A material's base colour: a factor times an optional texture.
struct ColorProperty {
    /// What the texture is multiplied by, or the colour itself without one, in linear Rec.709.
    Color factor{.r = 1.0F, .g = 1.0F, .b = 1.0F};

    /// The index of the texture's image in `MapAssets::images`, or nothing for none.
    std::optional<std::size_t> texture{};

    /// Compares two properties exactly.
    bool operator==(const ColorProperty&) const = default;
};

/// A property of one value, such as roughness: a factor times one channel of an optional texture.
struct ValueProperty {
    /// What the texture is multiplied by, or the value itself without one, from 0 to 1.
    float factor = 1.0F;

    /// The index of the texture's image in `MapAssets::images`, or nothing for none.
    std::optional<std::size_t> texture{};

    /// The channel of the texture that holds the value: the one the material names, or `r` for a
    /// grey image.
    Channel channel = Channel::r;

    /// Compares two properties exactly.
    bool operator==(const ValueProperty&) const = default;
};

/// A material's normal texture, which tilts the surface's normal in tangent space.
///
/// It follows glTF and OpenGL: red is +u, green is +v, and blue points out of the surface.
struct NormalProperty {
    /// The index of the texture's image in `MapAssets::images`.
    std::size_t texture = 0;

    /// What the texture's u and v components are multiplied by.
    float scale = 1.0F;

    /// Compares two properties exactly.
    bool operator==(const NormalProperty&) const = default;
};

/// A material's ambient occlusion: how much of the ambient light reaches each point of it.
struct OcclusionProperty {
    /// The occlusion, from 0, where no ambient light reaches, to 1, where it all does.
    ValueProperty value{};

    /// How strongly the occlusion applies, from 0 for not at all to 1 for fully, as in glTF.
    float strength = 1.0F;

    /// Compares two properties exactly.
    bool operator==(const OcclusionProperty&) const = default;
};

/// The light a material gives out of itself, which lights nothing else.
///
/// It emits `nits` times `color` times the texture's texel, in linear Rec.709, so a white texel of
/// a white emission shines at `nits`.
struct Emission {
    /// The luminance of white, in nits (candelas per square metre).
    float nits = 0.0F;

    /// The colour, as the material writes it or made from its temperature, whose luminance is that
    /// of white for a temperature.
    Color color{.r = 1.0F, .g = 1.0F, .b = 1.0F};

    /// The index of the texture's image in `MapAssets::images`, or nothing for none.
    std::optional<std::size_t> texture{};

    /// Compares two emissions exactly.
    bool operator==(const Emission&) const = default;
};

/// How a surface responds to light, with glTF's metallic-roughness properties, as a material's
/// asset describes it.
///
/// A material is opaque and seen from the front only. Its textures all repeat at its `size`, and
/// each refers to an image of the `MapAssets` that holds the material.
struct Material {
    /// The base colour.
    ColorProperty color{};

    /// The roughness, from 0 for a mirror to 1 for fully rough.
    ValueProperty roughness{};

    /// The metalness, from 0 for a dielectric to 1 for a metal.
    ValueProperty metallic{};

    /// The normal texture, or nothing for a flat surface.
    std::optional<NormalProperty> normal{};

    /// The ambient occlusion, or nothing for none.
    std::optional<OcclusionProperty> occlusion{};

    /// The light it gives out, or nothing for none.
    std::optional<Emission> emission{};

    /// The real-world size, in metres along u and v, of one repeat of its textures, or nothing for
    /// a material without a texture. The bottom-left corner of each texture lies at the origin of u
    /// and v, with u to the right and v up.
    std::optional<Vector2> size{};

    /// Game-defined data that the engine keeps and ignores: the JSON text of an object, or empty
    /// for none.
    std::string properties{};

    /// Compares two materials exactly.
    bool operator==(const Material&) const = default;
};

} // namespace wulfhilde
