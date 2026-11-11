#include <wulfhilde/assets/load_map_assets.hpp>

#include "file_reading.hpp"
#include "image_decoding.hpp"
#include "json_file.hpp"
#include "material_decoding.hpp"
#include "mip_chain.hpp"
#include "object_reader.hpp"
#include <wulfhilde/assets/asset_error.hpp>
#include <wulfhilde/assets/asset_problem.hpp>
#include <wulfhilde/assets/content_root.hpp>
#include <wulfhilde/assets/map_assets.hpp>
#include <wulfhilde/assets/material.hpp>
#include <wulfhilde/assets/texture_image.hpp>
#include <wulfhilde/world/ids.hpp>
#include <wulfhilde/world/map.hpp>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde {
namespace {

/// Returns the JSON pointer of the first surface of a map that carries each of its materials, by
/// the material's ID.
///
/// The map interns its materials in the order of its description, floor, ceiling and then walls of
/// each sector, which is the order of its file; this walks its surfaces in the same order.
std::vector<std::string> first_references(const Map& map) {
    std::vector<std::string> references(map.materials().size());
    const auto note = [&references](MaterialId material, auto pointer) {
        auto& reference = references.at(material.index());
        if (reference.empty()) {
            reference = pointer();
        }
    };
    for (std::size_t sector_index = 0; const auto& sector : map.sectors()) {
        note(sector.floor.surface.material,
             [sector_index] { return std::format("/sectors/{}/floor/material", sector_index); });
        note(sector.ceiling.surface.material,
             [sector_index] { return std::format("/sectors/{}/ceiling/material", sector_index); });
        for (std::size_t loop_index = 0; const auto& loop : sector.loops) {
            for (std::size_t wall_index = 0; const auto wall : loop) {
                note(map.wall(wall).surface.material, [sector_index, loop_index, wall_index] {
                    return std::format("/sectors/{}/loops/{}/{}/surface/material", sector_index,
                                       loop_index, wall_index);
                });
                ++wall_index;
            }
            ++loop_index;
        }
        ++sector_index;
    }
    return references;
}

/// Gives a problem of a whole asset's file the file's name, and names the first place that refers
/// to the asset, so that it is reported once and can be traced to where the asset is used.
AssetProblem name_whole_file_problem(AssetProblem problem, std::string file,
                                     std::string_view referrer) {
    problem.file = std::move(file);
    problem.message = std::format("{}, named first by {}", problem.message, referrer);
    return problem;
}

/// Returns the message of an asset that isn't there.
std::string missing_message(std::string_view file) {
    return std::format("there is no file at {:?}", file);
}

/// The images a map's materials read: each decoded once, and each made into a texture image once
/// for each way it is read.
class Textures {
  public:
    /// Starts with no images.
    ///
    /// \param root The content root that holds them.
    explicit Textures(const ContentRoot& root) : root_(root) {}

    /// Returns an image, decoding it the first time it is asked for.
    ///
    /// The first ask reports an image that is missing at the place that asks for it, and one that
    /// is broken in its own file, naming that place. Later asks report nothing.
    ///
    /// \param reference The texture that asks for it first, which names its asset path.
    /// \param file The material's file that names the texture.
    /// \return The image, or nothing if it is missing or broken.
    const detail::DecodedImage* image(const detail::TextureReference& reference,
                                      detail::JsonFile& file) {
        auto [found, inserted] = decoded_.try_emplace(reference.path);
        if (inserted) {
            const auto name = std::format("{}{}", reference.path, image_extension);
            auto decoded =
                detail::read_file(root_.get().directory() / name)
                    .and_then([](const std::string& bytes) { return detail::decode_png(bytes); });
            if (decoded) {
                found->second = std::move(*decoded);
            } else if (decoded.error().code == AssetProblemCode::missing_file) {
                detail::report_at(reference.field, AssetProblemCode::missing_file,
                                  missing_message(name), file);
            } else {
                const auto& field = reference.field;
                const auto position = file.document().position(field.value.offset());
                problems_.push_back(name_whole_file_problem(
                    std::move(decoded.error()), name,
                    std::format("{} at {}:{} ({})", file.name(), position.line, position.column,
                                field.pointer.text())));
            }
        }
        return found->second ? &*found->second : nullptr;
    }

    /// Returns the index of an image's texture image for one way of reading it, making it the
    /// first time that way is asked for.
    ///
    /// \param path The image's asset path.
    /// \param image The image, which `image` returned.
    /// \param meaning What the way of reading it takes its texels to hold.
    std::size_t texture(const std::string& path, const detail::DecodedImage& image,
                        detail::TexelMeaning meaning) {
        const auto [found, inserted] = textures_.try_emplace({path, meaning}, images_.size());
        if (inserted) {
            images_.push_back({
                .path = path,
                .color_space =
                    meaning == detail::TexelMeaning::color ? ColorSpace::srgb : ColorSpace::linear,
                .levels = detail::build_mip_chain(
                    {.width = image.width, .height = image.height, .texels = image.pixels},
                    meaning),
            });
        }
        return found->second;
    }

    /// Takes the problems found since the last take, in the order they were found.
    [[nodiscard]] std::vector<AssetProblem> take_problems() { return std::exchange(problems_, {}); }

    /// Takes the texture images, in the order they were first asked for.
    [[nodiscard]] std::vector<TextureImage> take_images() && { return std::move(images_); }

  private:
    std::reference_wrapper<const ContentRoot> root_;
    std::map<std::string, std::optional<detail::DecodedImage>, std::less<>> decoded_;
    std::map<std::pair<std::string, detail::TexelMeaning>, std::size_t> textures_;
    std::vector<TextureImage> images_;
    std::vector<AssetProblem> problems_;
};

/// The channel of a texture that a single-value property reads.
struct ChannelRead {
    /// The channel the file names, if it names one.
    std::reference_wrapper<const std::optional<detail::ChannelReference>> written;

    /// Where the channel read goes.
    std::reference_wrapper<Channel> read;
};

/// A texture that a material's property reads, with where its image's index goes.
struct TextureRead {
    /// The texture as the file names it.
    std::reference_wrapper<const detail::TextureReference> reference;

    /// What the property takes its texels to hold.
    detail::TexelMeaning meaning{};

    /// What the property is, for messages, such as `roughness`.
    std::string_view noun;

    /// Where the image's index goes.
    std::reference_wrapper<std::optional<std::size_t>> index;

    /// Whether the property is the material's opaque base colour, which no alpha channel may
    /// make see-through.
    bool opaque = false;

    /// The channel it reads, for a single-value property, or nothing for another.
    std::optional<ChannelRead> channel{};
};

/// Checks that an image can be read as a property reads it, reporting why not at the property's
/// texture or channel.
///
/// \return Whether it can.
bool check_texture(const TextureRead& read, const detail::DecodedImage& image,
                   detail::JsonFile& file) {
    const auto& reference = read.reference.get();
    const auto report = [&file](const detail::Field& field, AssetProblemCode code,
                                std::string message) {
        detail::report_at(field, code, std::move(message), file);
    };
    switch (read.meaning) {
    case detail::TexelMeaning::color:
        if (read.opaque && image.alpha) {
            report(reference.field, AssetProblemCode::alpha_in_color_texture,
                   std::format("the image {:?} has an alpha channel, but a material is opaque, so "
                               "its colour can't have one",
                               reference.path));
            return false;
        }
        return true;
    case detail::TexelMeaning::normal:
        if (image.grey) {
            report(reference.field, AssetProblemCode::grey_normal_texture,
                   std::format("the image {:?} is grey, but a normal texture needs red, green and "
                               "blue to hold a direction",
                               reference.path));
            return false;
        }
        return true;
    case detail::TexelMeaning::value:
        break;
    }

    // Only a single-value property reads a value, and each reads a channel.
    if (!read.channel) {
        return true;
    }
    const auto& [written, channel_read] = *read.channel;
    const auto& channel = written.get();
    if (!channel) {
        if (!image.grey || image.alpha) {
            report(reference.field, AssetProblemCode::missing_channel,
                   std::format(R"(the image {:?} has several channels, but the {} names no )"
                               R"("channel" to read)",
                               reference.path, read.noun));
            return false;
        }
        channel_read.get() = Channel::r;
        return true;
    }
    if (channel->channel == Channel::a && !image.alpha) {
        report(channel->field, AssetProblemCode::no_such_channel,
               std::format("the image {:?} has no alpha channel to read", reference.path));
        return false;
    }
    channel_read.get() = channel->channel;
    return true;
}

/// Lists the textures a material reads, in the order its file names them, each pointing into the
/// material where its image's index goes.
///
/// A normal texture is always there, so its index goes to `normal_index` instead, for the caller to
/// move into the material.
std::vector<TextureRead> texture_reads(const detail::MaterialDescription& description,
                                       Material& material,
                                       std::optional<std::size_t>& normal_index) {
    std::vector<TextureRead> reads;
    const auto add_value = [&reads](const detail::ValueDescription& value, std::string_view noun,
                                    ValueProperty& property) {
        if (value.texture) {
            reads.push_back({
                .reference = *value.texture,
                .meaning = detail::TexelMeaning::value,
                .noun = noun,
                .index = property.texture,
                .channel = ChannelRead{.written = value.channel, .read = property.channel},
            });
        }
    };
    if (description.color.texture) {
        reads.push_back({
            .reference = *description.color.texture,
            .meaning = detail::TexelMeaning::color,
            .noun = "colour",
            .index = material.color.texture,
            .opaque = true,
        });
    }
    add_value(description.roughness, "roughness", material.roughness);
    add_value(description.metallic, "metalness", material.metallic);
    if (description.normal) {
        reads.push_back({
            .reference = description.normal->texture,
            .meaning = detail::TexelMeaning::normal,
            .noun = "normal",
            .index = normal_index,
        });
    }
    // The material has an occlusion and an emission where its description does.
    if (description.occlusion && material.occlusion) {
        add_value(description.occlusion->value, "occlusion", material.occlusion->value);
    }
    if (description.emission && description.emission->texture && material.emission) {
        reads.push_back({
            .reference = *description.emission->texture,
            .meaning = detail::TexelMeaning::color,
            .noun = "emission",
            .index = material.emission->texture,
        });
    }
    std::ranges::stable_sort(reads, {}, [](const TextureRead& read) {
        return read.reference.get().field.value.offset();
    });
    return reads;
}

/// Returns a material as the assets hold it, with no textures yet.
Material untextured_material(const detail::MaterialDescription& description) {
    const auto value_property = [](const detail::ValueDescription& value) {
        return ValueProperty{.factor = value.factor};
    };
    Material material{
        .color = {.factor = description.color.factor},
        .roughness = value_property(description.roughness),
        .metallic = value_property(description.metallic),
        .normal = description.normal.transform([](const detail::NormalDescription& normal) {
            return NormalProperty{.scale = normal.scale};
        }),
        .size = description.size,
        .properties = description.properties,
    };
    if (description.occlusion) {
        material.occlusion = OcclusionProperty{
            .value = value_property(description.occlusion->value),
            .strength = description.occlusion->strength,
        };
    }
    if (description.emission) {
        material.emission = Emission{
            .nits = description.emission->nits,
            .color = description.emission->color,
        };
    }
    return material;
}

/// Loads one material and the images it reads, adding every problem found to `problems`.
///
/// \param root The content root.
/// \param path The material's asset path.
/// \param reference The JSON pointer of the first surface in the map that carries it.
/// \param textures The images loaded so far.
/// \param problems Where its problems go.
/// \return The material, or nothing if it has any problem.
std::optional<Material> load_material(const ContentRoot& root, const std::string& path,
                                      const std::string& reference, Textures& textures,
                                      std::vector<AssetProblem>& problems) {
    auto name = std::format("{}{}", path, material_extension);
    const auto text = detail::read_file(root.directory() / name);
    if (!text && text.error().code == AssetProblemCode::missing_file) {
        // At the map's surface, whose file the map doesn't know, so without a position.
        problems.push_back({
            .code = AssetProblemCode::missing_file,
            .message = missing_message(name),
            .pointer = reference,
        });
        return std::nullopt;
    }
    if (!text) {
        problems.push_back(
            name_whole_file_problem(text.error(), name, std::format("the map at {}", reference)));
        return std::nullopt;
    }
    const auto document = detail::parse_json(*text);
    if (!document) {
        auto problem = document.error();
        problem.file = std::move(name);
        problems.push_back(std::move(problem));
        return std::nullopt;
    }

    detail::JsonFile file(std::move(name), *text, *document);
    const auto description = detail::decode_material(file);
    if (!description || file.has_problems()) {
        std::ranges::move(std::move(file).take_problems(), std::back_inserter(problems));
        return std::nullopt;
    }

    auto material = untextured_material(*description);
    auto textured = true;
    std::optional<std::size_t> normal_index;
    for (const auto& read : texture_reads(*description, material, normal_index)) {
        const auto& texture = read.reference.get();
        const auto* const image = textures.image(texture, file);
        if (image == nullptr || !check_texture(read, *image, file)) {
            textured = false;
            continue;
        }
        read.index.get() = textures.texture(texture.path, *image, read.meaning);
    }
    std::ranges::move(std::move(file).take_problems(), std::back_inserter(problems));
    std::ranges::move(textures.take_problems(), std::back_inserter(problems));
    if (!textured) {
        return std::nullopt;
    }
    if (material.normal) {
        material.normal->texture = normal_index.value_or(0);
    }
    return material;
}

} // namespace

std::expected<MapAssets, AssetError> load_map_assets(const ContentRoot& root, const Map& map) {
    const auto references = first_references(map);
    Textures textures(root);
    std::vector<AssetProblem> problems;
    std::vector<Material> materials;
    for (const auto& [path, reference] : std::views::zip(map.materials(), references)) {
        auto material = load_material(root, path, reference, textures, problems);
        if (material) {
            materials.push_back(std::move(*material));
        }
    }

    if (!problems.empty()) {
        return std::unexpected(AssetError{
            .code = AssetErrorCode::asset_problems,
            .message = std::format("the map's assets have {} problem{}", problems.size(),
                                   problems.size() == 1 ? "" : "s"),
            .problems = std::move(problems),
        });
    }
    return MapAssets{
        .materials = std::move(materials),
        .images = std::move(textures).take_images(),
    };
}

} // namespace wulfhilde
