/***************************************************************************\
* Name        : C++ header dumper                                           *
* Description : generate C++ header with all structs and enums              *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "header.h"
#include "../parser/char_stream.h"
#include "ast/proto-common.h"
#include "ast/proto-field.h"
#include "ast/proto-file.h"
#include "ast/proto-message.h"
#include "indent_ostream.h"
#include "parser/parser.h"
#include <set>
#include <spb/json/deserialize.hpp>
#include <string>
#include <string_view>

using namespace std::literals;

namespace
{

using cpp_includes = std::set<std::string>;

struct std_includes
{
    bool vector;
    bool array;
    bool map;
    bool string;
    bool variant;
    bool optional;
    bool memory;
};

void dump_comment(std::ostream &stream, const proto_comment &comment)
{
    for (const auto &comm : comment.comments)
    {
        if (comm.starts_with("//[["))
        {
            //- ignore options in comments
            continue;
        }

        stream << comm;
        if (comm.back() != '\n')
            stream << '\n';
    }
}

auto trim_include(std::string_view str) -> std::string
{
    auto p_begin = str.data();
    auto p_end   = str.data() + str.size();
    while (p_begin < p_end && isspace(*p_begin))
    {
        p_begin++;
    }

    while (p_begin < p_end && isspace(p_end[-1]))
    {
        p_end--;
    }

    if (p_begin == p_end)
        return {};

    auto add_prefix  = *p_begin != '"' && *p_begin != '<';
    auto add_postfix = p_end[-1] != '"' && p_end[-1] != '>';

    if (add_prefix || add_postfix)
        return '"' + std::string(str) + '"';

    return std::string(str);
}

void dump_includes(std::ostream &stream, const cpp_includes &includes)
{
    for (auto &include : includes)
    {
        auto file = trim_include(include);
        if (!file.empty())
            stream << "#include " << file << "\n";
    }
    stream << "\n";
}

void get_user_includes(cpp_includes &includes, const proto_file &file)
{
    includes.insert(file.attributes.include.begin(), file.attributes.include.end());
}

void get_imports(cpp_includes &includes, const proto_file &file)
{
    for (const auto &import : file.imports)
    {
        if (file.attributes.exclude.contains(import.path.filename().string()))
            continue;

        includes.insert("\"" + cpp_file_name_from_proto(import.path, ".pb.h").string() + "\"");
    }
}

void dump_pragma(std::ostream &stream, const proto_file &)
{
    stream << "#pragma once\n\n";
}

void dump_syntax(std::ostream &stream, const proto_file &file)
{
    dump_comment(stream, file.syntax.comments);
}

auto type_literal_suffix(proto_field::Type type) -> std::string_view
{
    static constexpr auto type_map = std::array<std::pair<proto_field::Type, std::string_view>, 12>{{
        {proto_field::Type::INT64, "LL"},
        {proto_field::Type::UINT32, "U"},
        {proto_field::Type::UINT64, "ULL"},
        {proto_field::Type::SINT64, "LL"},
        {proto_field::Type::FIXED32, "U"},
        {proto_field::Type::FIXED64, "ULL"},
        {proto_field::Type::SFIXED64, "LL"},
        {proto_field::Type::FLOAT, "F"},
    }};

    for (auto [proto_type, suffix] : type_map)
    {
        if (type == proto_type)
            return suffix;
    }

    return {};
}

auto get_field_bits(const proto_field &field) -> std::string_view
{
    if (auto name = field.attributes.type; !name.empty())
    {
        auto bitfield = name;
        if (auto index = bitfield.find(':'); index != std::string_view::npos)
        {
            const_cast<proto_field &>(field).bit_field = bitfield.substr(index + 1);
            return bitfield.substr(index);
        }
    }
    return {};
}

auto get_container_type(std::string_view options, std::string_view message_options,
                        std::string_view file_options, std::string_view ctype,
                        std::string_view default_type = {}) -> std::string
{
    if (!options.empty())
        return replace(options, "$", ctype);

    if (!message_options.empty())
        return replace(message_options, "$", ctype);

    if (!file_options.empty())
        return replace(file_options, "$", ctype);

    return replace(default_type, "$", ctype);
}

auto get_map_type(std::string_view options, std::string_view message_options, std::string_view file_options,
                  std::string_view key_type, std::string_view value_type, std::string_view default_type = {})
    -> std::string
{
    if (!options.empty())
        return replace(replace(options, "@", value_type), "$", key_type);

    if (!message_options.empty())
        return replace(replace(message_options, "@", value_type), "$", key_type);

    if (!file_options.empty())
        return replace(replace(file_options, "@", value_type), "$", key_type);

    return replace(replace(default_type, "@", value_type), "$", key_type);
}

auto get_enum_type(const proto_file &file, const proto_attributes &attributes,
                   const proto_attributes &message_attributes, const proto_attributes &file_attributes,
                   std::string_view default_type) -> std::string_view
{
    static constexpr auto type_map = std::array<std::pair<std::string_view, std::string_view>, 6>{{
        {"int8"sv, "int8_t"sv},
        {"uint8"sv, "uint8_t"sv},
        {"int16"sv, "int16_t"sv},
        {"uint16"sv, "uint16_t"sv},
        {"int32"sv, "int32_t"sv},
    }};

    auto ctype_from_pb = [&](std::string_view type)
    {
        for (auto [proto_type, c_type] : type_map)
        {
            if (type == proto_type)
                return c_type;
        }
        throw_parse_error(file, type, "invalid enum type: " + std::string(type));
    };

    if (auto name = attributes.type; !name.empty())
        return ctype_from_pb(name);

    if (auto name = message_attributes.enum_; !name.empty())
        return ctype_from_pb(name);

    if (auto name = file_attributes.enum_; !name.empty())
        return ctype_from_pb(name);

    return default_type;
}

auto convert_to_ctype(const proto_file &file, const proto_field &field, const proto_message &message = {})
    -> std::string
{
    if (field.bit_type != proto_field::BitType::NONE)
    {
        switch (field.bit_type)
        {
        case proto_field::BitType::NONE:
            throw_parse_error(file, field.type_name.proto_name, "invalid type");
        case proto_field::BitType::INT8:
            return "int8_t";
        case proto_field::BitType::INT16:
            return "int16_t";
        case proto_field::BitType::INT32:
            return "int32_t";
        case proto_field::BitType::INT64:
            return "int64_t";
        case proto_field::BitType::UINT8:
            return "uint8_t";
        case proto_field::BitType::UINT16:
            return "uint16_t";
        case proto_field::BitType::UINT32:
            return "uint32_t";
        case proto_field::BitType::UINT64:
            return "uint64_t";
        }
    }

    switch (field.type)
    {
    case proto_field::Type::NONE:
        throw_parse_error(file, field.type_name.proto_name, "invalid type");

    case proto_field::Type::STRING:
        return get_container_type(field.attributes.string, message.attributes.string, file.attributes.string,
                                  "char", "std::string");
    case proto_field::Type::BYTES:
        return get_container_type(field.attributes.bytes, message.attributes.bytes, file.attributes.bytes,
                                  "std::byte", "std::vector<$>");
    case proto_field::Type::ENUM:
    case proto_field::Type::MESSAGE:
        return std::string(field.type_name.get_name());

    case proto_field::Type::FLOAT:
        return "float";
    case proto_field::Type::DOUBLE:
        return "double";
    case proto_field::Type::BOOL:
        return "bool";
    case proto_field::Type::SFIXED32:
    case proto_field::Type::INT32:
    case proto_field::Type::SINT32:
        return "int32_t";
    case proto_field::Type::FIXED32:
    case proto_field::Type::UINT32:
        return "uint32_t";
    case proto_field::Type::SFIXED64:
    case proto_field::Type::INT64:
    case proto_field::Type::SINT64:
        return "int64_t";
    case proto_field::Type::UINT64:
    case proto_field::Type::FIXED64:
        return "uint64_t";
    }

    throw_parse_error(file, field.type_name.proto_name, "invalid type");
}
auto add_label_type(std::string_view ctype, const proto_field &field, const proto_message &message,
                    const proto_file &file) -> std::string
{
    switch (field.label)
    {
    case proto_field::Label::OPTIONAL:
        return get_container_type(field.attributes.optional, message.attributes.optional,
                                  file.attributes.optional, ctype, "std::optional<$>");

    case proto_field::Label::REPEATED:
        return get_container_type(field.attributes.repeated, message.attributes.repeated,
                                  file.attributes.repeated, ctype, "std::vector<$>");
    case proto_field::Label::PTR:
        return get_container_type(field.attributes.pointer, message.attributes.pointer,
                                  file.attributes.pointer, ctype, "std::unique_ptr<$>");
    default:
        return std::string(ctype);
    }
}

void dump_field_type_and_name(std::ostream &stream, const proto_field &field, const proto_message &message,
                              const proto_file &file)
{
    const auto ctype = convert_to_ctype(file, field, message);

    if (field.label == proto_field::Label::NONE)
    {
        stream << ctype << ' ' << field.name.get_name() << get_field_bits(field);
        return;
    }

    if (auto bitfield = get_field_bits(field); !bitfield.empty())
        throw_parse_error(file, bitfield, "bitfield can be used only with `required` label");

    stream << add_label_type(ctype, field, message, file) << ' ' << field.name.get_name();
}

void dump_enum_field(std::ostream &stream, const proto_base &field)
{
    dump_comment(stream, field.comment);

    stream << field.name.get_name() << " = " << field.number << ",\n";
}

void dump_enum(std::ostream &stream, const proto_enum &my_enum, const proto_message &message,
               const proto_file &file)
{
    dump_comment(stream, my_enum.comment);

    stream << "enum class " << my_enum.name.get_name() << " : "
           << get_enum_type(file, my_enum.attributes, message.attributes, file.attributes, "int32_t")
           << "\n{\n";
    for (const auto &field : my_enum.fields)
    {
        dump_enum_field(stream, field);
    }
    stream << "};\n";
}

void dump_message_oneof(std::ostream &stream, const proto_oneof &oneof, const proto_file &file)
{
    dump_comment(stream, oneof.comment);

    stream << "std::variant<std::monostate";
    for (const auto &field : oneof.fields)
    {
        stream << ", " << convert_to_ctype(file, field);
    }
    stream << "> " << oneof.name.get_name() << ";\n";
}

void dump_message_map(std::ostream &stream, const proto_map &map, const proto_message &message,
                      const proto_file &file)
{
    const auto key_type   = convert_to_ctype(file, map.key);
    const auto value_type = convert_to_ctype(file, map.value);

    dump_comment(stream, map.comment);
    stream << get_map_type(map.attributes.map, message.attributes.map, file.attributes.map, key_type,
                           value_type, "std::map<$, @>")
           << " " << map.name.get_name() << ";\n";
}

void dump_default_value(std::ostream &stream, const proto_field &field)
{
    // Use option-key presence rather than attributes.default_.empty(): a declared
    // [default = ""] parses to an empty string_view, which must still emit ` = ""`.
    if (field.options.find("default") == field.options.end())
        return;

    auto default_value = field.attributes.default_;

    if (field.type == proto_field::Type::ENUM)
    {
        stream << " = " << field.type_name.get_name() << "::" << default_value;
    }
    else if (field.type == proto_field::Type::STRING &&
             (default_value.size() < 2 || default_value.front() != '"' || default_value.back() != '"'))
    {
        stream << " = \"" << default_value << "\"";
    }
    else
    {
        stream << " = " << default_value << type_literal_suffix(field.type);
    }
}

void dump_deprecated_attribute(std::ostream &stream, const proto_field &field)
{
    if (field.attributes.deprecated)
        stream << "[[deprecated]] ";
}

void dump_message_field(std::ostream &stream, const proto_field &field, const proto_message &message,
                        const proto_file &file)
{
    dump_comment(stream, field.comment);
    dump_deprecated_attribute(stream, field);
    dump_field_type_and_name(stream, field, message, file);
    dump_default_value(stream, field);
    stream << ";\n";
}

void dump_forwards(std::ostream &stream, const forwarded_declarations &forwards)
{
    if (forwards.empty())
        return;

    for (const auto &forward : forwards)
    {
        stream << "struct " << forward << ";\n";
    }
    stream << '\n';
}

void get_std_includes(std::string_view ctype, std::string_view type, std_includes &result)
{
    result.array |= ctype.starts_with("std::array<") || type.starts_with("std::array<");
    result.vector |= ctype.starts_with("std::vector<") || type.starts_with("std::vector<");
    result.string |= ctype.starts_with("std::string") || type.starts_with("std::string");
    result.optional |= ctype.starts_with("std::optional<") || type.starts_with("std::optional<");
    result.memory |= ctype.starts_with("std::unique_ptr<") || type.starts_with("std::unique_ptr<");
}

void get_std_includes(const proto_map &map, const proto_message &message, const proto_file &file,
                      std_includes &result)
{
    const auto key_type   = convert_to_ctype(file, map.key);
    const auto value_type = convert_to_ctype(file, map.value);

    result.map |= get_map_type(map.attributes.map, message.attributes.map, file.attributes.map, key_type,
                               value_type, "std::map<$, @>")
                      .starts_with("std::map<");

    get_std_includes(key_type, "", result);
    get_std_includes(value_type, "", result);
}

void get_std_includes(const proto_field &field, const proto_message &message, const proto_file &file,
                      std_includes &result)
{
    const auto ctype = convert_to_ctype(file, field, message);
    const auto type  = add_label_type(ctype, field, message, file);

    get_std_includes(ctype, type, result);
}

void get_std_includes(const proto_oneof &oneof, const proto_file &file, std_includes &result)
{
    for (const auto &field : oneof.fields)
    {
        get_std_includes(convert_to_ctype(file, field), "", result);
    }
}

void get_std_includes(const proto_message &message, const proto_file &file, std_includes &result)
{
    for (const auto &map : message.maps)
    {
        get_std_includes(map, message, file, result);
    }

    for (const auto &oneof : message.oneofs)
    {
        result.variant = true;
        get_std_includes(oneof, file, result);
    }

    for (const auto &field : message.fields)
    {
        get_std_includes(field, message, file, result);
    }

    for (const auto &sub_message : message.messages)
    {
        get_std_includes(sub_message, file, result);
    }
}

auto get_std_includes(const proto_file &file) -> std_includes
{
    auto result = std_includes{};
    for (const auto &message : file.package.messages)
    {
        get_std_includes(message, file, result);
    }
    return result;
}

void dump_message(std::ostream &stream, const proto_message &message, const proto_file &file)
{
    dump_comment(stream, message.comment);

    stream << "struct " << message.name.get_name() << "\n{\n";

    dump_forwards(stream, message.forwards);
    for (const auto &sub_enum : message.enums)
    {
        dump_enum(stream, sub_enum, message, file);
    }

    for (const auto &sub_message : message.messages)
    {
        dump_message(stream, sub_message, file);
    }

    for (const auto &field : message.fields)
    {
        dump_message_field(stream, field, message, file);
    }

    for (const auto &map : message.maps)
    {
        dump_message_map(stream, map, message, file);
    }

    for (const auto &oneof : message.oneofs)
    {
        dump_message_oneof(stream, oneof, file);
    }

    //- TODO: is this used in any way?
    // stream << "auto operator == ( const " << message.name << " & ) const noexcept ->
    // bool = default;\n"; stream << "auto operator != ( const " << message.name << " &
    // ) const noexcept -> bool = default;\n";
    stream << "};\n";
}

void dump_messages(std::ostream &stream, const proto_file &file)
{
    dump_forwards(stream, file.package.forwards);
    for (const auto &message : file.package.messages)
    {
        dump_message(stream, message, file);
    }
}

void dump_enums(std::ostream &stream, const proto_file &file)
{
    for (const auto &my_enum : file.package.enums)
    {
        dump_enum(stream, my_enum, file.package, file);
    }
}

void dump_package_begin(std::ostream &stream, const proto_file &file)
{
    dump_comment(stream, file.package.comment);
    if (!file.package.name.get_name().empty())
        stream << "namespace " << file.package.name.get_name() << "\n{\n";
}

void dump_package_end(std::ostream &stream, const proto_file &file)
{
    if (!file.package.name.get_name().empty())
        stream << "}// namespace " << file.package.name.get_name() << "\n\n";
}
} // namespace

void throw_parse_error(const proto_file &file, std::string_view at, std::string_view message)
{
    auto stream = spb::char_stream(file.content);
    stream.skip_to(at.data());
    stream.throw_parse_error(message);
}

void get_std_includes(cpp_includes &includes, const proto_file &file)
{
    includes.insert("<spb/json.hpp>");
    includes.insert("<spb/pb.hpp>");
    includes.insert("<cstddef>");
    includes.insert("<cstdint>");

    const auto std_includes = get_std_includes(file);

    if (std_includes.optional)
        includes.insert("<optional>");
    if (std_includes.memory)
        includes.insert("<memory>");
    if (std_includes.string)
        includes.insert("<string>");
    if (std_includes.array)
        includes.insert("<array>");
    if (std_includes.map)
        includes.insert("<map>");
    if (std_includes.string)
        includes.insert("<string>");
    if (std_includes.vector)
        includes.insert("<vector>");
    if (std_includes.variant)
        includes.insert("<variant>");
}

void dump_cpp_definitions(const proto_file &file, std::ostream &stream)
{
    indented_ostream ostream(stream);

    auto includes = cpp_includes();
    dump_pragma(ostream, file);
    get_imports(includes, file);
    get_std_includes(includes, file);
    get_user_includes(includes, file);
    dump_includes(ostream, includes);
    dump_syntax(ostream, file);
    dump_package_begin(ostream, file);
    dump_enums(ostream, file);
    dump_messages(ostream, file);
    dump_package_end(ostream, file);
}

auto replace(std::string_view input, std::string_view what, std::string_view with) -> std::string
{
    auto result = std::string(input);
    auto pos    = size_t{};

    while ((pos = result.find(what, pos)) != std::string::npos)
    {
        result.replace(pos, what.size(), with);
        pos += with.size();
    }

    return result;
}
