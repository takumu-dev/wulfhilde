#pragma once

#include <wulfhilde/world/map_problem.hpp>

#include <pedantjson/pedantjson.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace wulfhilde::detail {

/// A JSON file being decoded: its text, the document parsed from it, and the problems found in it
/// so far, each placed at the position of a value or a key in the text.
class JsonFile {
  public:
    /// Starts the decoding of a file.
    ///
    /// \param text The file's text, which must outlive this.
    /// \param document The document parsed from the text, which must outlive this.
    JsonFile(std::string_view text, const pedantjson::Document& document) noexcept
        : text_(text), document_(document) {}

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
    void report(MapProblemCode code, const pedantjson::Pointer& pointer, std::size_t offset,
                std::string message);

    /// Reports a problem found in what the file describes, at the value its pointer names or, if
    /// the file never wrote that value, at the nearest value on the way to it that the file did.
    ///
    /// \param problem The problem, whose pointer is an RFC 6901 pointer.
    void report_at_pointer(MapProblem problem);

    /// Tells whether any problem was reported.
    [[nodiscard]] bool has_problems() const noexcept { return !problems_.empty(); }

    /// Takes the problems, in the order of their positions in the file.
    [[nodiscard]] std::vector<MapProblem> take_problems() &&;

  private:
    /// A problem, and where in the text it lies.
    struct PlacedProblem {
        std::size_t offset = 0;
        MapProblem problem;
    };

    /// Reports a problem at an offset, giving it the offset's position.
    void place(MapProblem problem, std::size_t offset);

    std::string_view text_;
    std::reference_wrapper<const pedantjson::Document> document_;
    std::vector<PlacedProblem> problems_;
};

} // namespace wulfhilde::detail
