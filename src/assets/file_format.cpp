#include "file_format.hpp"

#include "json_file.hpp"
#include "object_reader.hpp"
#include <wulfhilde/assets/asset_problem.hpp>

#include <pedantjson/pedantjson.hpp>

#include <format>
#include <string>

namespace wulfhilde::detail {

bool check_format_and_version(const pedantjson::Value& root, const FileFormat& format,
                              JsonFile& file) {
    const pedantjson::Pointer pointer;
    const auto object = root.as_object();
    if (!object) {
        file.report(AssetProblemCode::wrong_type, pointer, root.offset(),
                    std::format("expected an object, not {}", kind_name(root.kind())));
        return false;
    }

    const auto written_format = object->find("format");
    if (!written_format) {
        file.report(AssetProblemCode::unknown_format, pointer, root.offset(),
                    std::format(R"(the file isn't a wulfhilde {}: it has no "format", which a )"
                                "{} gives as {:?}",
                                format.noun, format.noun, format.name));
        return false;
    }
    const auto name = written_format->value.as_string();
    if (name != format.name) {
        file.report(AssetProblemCode::unknown_format, pointer / "format",
                    written_format->value.offset(),
                    std::format(R"(the file isn't a wulfhilde {}: its "format" is {}, not {:?})",
                                format.noun,
                                name ? std::format("{:?}", *name)
                                     : std::string{kind_name(written_format->value.kind())},
                                format.name));
        return false;
    }

    const auto version = object->find("version");
    if (!version) {
        file.report(AssetProblemCode::unsupported_version, pointer, root.offset(),
                    std::format(R"(the {} has no "version", but this version of the engine )"
                                "reads {}s of version {} only",
                                format.noun, format.noun, format.version));
        return false;
    }
    // A number written with a zero fraction, such as 1.0, is an integer.
    const auto number = version->value.as_number();
    if (!number || number->to<int>() != format.version) {
        file.report(AssetProblemCode::unsupported_version, pointer / "version",
                    version->value.offset(),
                    std::format("this version of the engine reads {}s of version {} only, not {}",
                                format.noun, format.version,
                                number ? number->text() : kind_name(version->value.kind())));
        return false;
    }
    return true;
}

} // namespace wulfhilde::detail
