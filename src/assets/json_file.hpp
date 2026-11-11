#pragma once

#include <wulfhilde/assets/asset_problem.hpp>

#include <pedantjson/pedantjson.hpp>

#include <cstddef>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wulfhilde::detail {

/// A JSON file being decoded: its text, the document parsed from it, and the problems found in it
/// so far, each placed at the position of a value or a key in the text.
class JsonFile {
  public:
    /// Starts the decoding of a file.
    ///
    /// \param name The file's asset path with its extension, which each problem names.
    /// \param text The file's text, which must outlive this.
    /// \param document The document parsed from the text, which must outlive this.
    JsonFile(std::string name, std::string_view text, const pedantjson::Document& document) noexcept
        : name_(std::move(name)), text_(text), document_(document) {}

    /// Returns the file's asset path with its extension.
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// Returns the file's text.
    [[nodiscard]] std::string_view text() const noexcept { return text_; }

    /// Returns the document parsed from the file's text.
    [[nodiscard]] const pedantjson::Document& document() const noexcept { return document_; }

    /// Reports a problem.
    ///
    /// \param code What kind of problem it is.
    /// \param pointer The JSON pointer of the value it lies in.
    /// \param offset Where in the text it lies, in bytes: at the value, or at a member's key.
    /// \param message What is wrong.
    void report(AssetProblemCode code, const pedantjson::Pointer& pointer, std::size_t offset,
                std::string message);

    /// Tells whether any problem was reported.
    [[nodiscard]] bool has_problems() const noexcept { return !problems_.empty(); }

    /// Takes the problems, in the order of their positions in the file.
    [[nodiscard]] std::vector<AssetProblem> take_problems() &&;

  private:
    /// A problem, and where in the text it lies.
    struct PlacedProblem {
        std::size_t offset = 0;
        AssetProblem problem;
    };

    std::string name_;
    std::string_view text_;
    std::reference_wrapper<const pedantjson::Document> document_;
    std::vector<PlacedProblem> problems_;
};

/// Parses a JSON file's text, finding the first place where it stops being JSON if it isn't.
///
/// \param text The file's text.
/// \return The document, or the syntax error as a problem, which names no file.
[[nodiscard]] std::expected<pedantjson::Document, AssetProblem> parse_json(const std::string& text);

} // namespace wulfhilde::detail
