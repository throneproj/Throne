/***************************************************************************\
* Name        : json dumper                                                 *
* Description : generate C++ src files for json de/serialization            *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper.h"
#include "../dumper.h"
#include "../header.h"
#include "ast/proto-field.h"
#include "ast/proto-file.h"
#include "template-h.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <map>
#include <set>
#include <spb/json/deserialize.hpp>
#include <string>
#include <string_view>

using namespace std::literals;

namespace
{

auto json_name_from_options(const proto_attributes &attributes) -> std::string_view
{
    return attributes.json_name;
}

auto convert_to_camelCase(std::string_view input) -> std::string
{
    auto result = std::string(input);
    if (!result.empty())
        result[0] = char(std::tolower(result[0]));

    if (input.find('_') == std::string::npos)
        return result;

    auto index = 0U;
    auto up    = false;
    for (auto c : input)
    {
        if (c == '_')
        {
            up = true;
        }
        else
        {
            result[index] = char((up && index > 0) ? std::toupper(c) : std::tolower(c));
            index += 1;
            up = false;
        }
    }
    result.resize(index);
    return result;
}

auto json_field_name(const proto_base &field) -> std::string
{
    if (const auto result = json_name_from_options(field.attributes); !result.empty())
    {
        return std::string(result);
    }

    return std::string(field.name.proto_name);
}

auto json_field_name_or_camelCase(const proto_base &field) -> std::string
{
    if (const auto result = json_name_from_options(field.attributes); !result.empty())
        return std::string(result);

    return convert_to_camelCase(field.name.proto_name);
}

void dump_cpp_includes(std::ostream &stream, std::string_view header_file_path)
{
    stream << replace(json_includes_template, "$", header_file_path);
}

void dump_cpp_close_namespace(std::ostream &stream, std::string_view name)
{
    stream << "} // namespace " << name << "\n\n";
}

void dump_cpp_open_namespace(std::ostream &stream, std::string_view name)
{
    stream << "namespace " << name << "\n{\n";
}

auto field_max_size(const proto_file &file, const proto_message &message, const proto_attributes &attributes)
    -> size_t
{
    return attributes.max_size.value_or(
        message.attributes.max_size.value_or(file.attributes.max_size.value_or(0)));
}

auto field_max_count(const proto_file &file, const proto_message &message, const proto_attributes &attributes)
    -> size_t
{
    return attributes.max_count.value_or(
        message.attributes.max_count.value_or(file.attributes.max_count.value_or(0)));
}

void dump_field_attributes(std::ostream &stream, const proto_file &file, const proto_message &message,
                           const proto_attributes &attributes)
{
    const auto max_count = field_max_count(file, message, attributes);
    const auto max_size  = field_max_size(file, message, attributes);

    stream << "field_attributes";

    if (max_size == 0 && max_count == 0)
    {
        stream << "{}";
        return;
    }

    auto next = '{';
    if (max_count)
    {
        stream << next << " .max_count = " << max_count;
        next = ',';
    }
    if (max_size)
        stream << next << " .max_size = " << max_size;
    stream << "}";
}

void dump_field_attributes(std::ostream &stream, const proto_file &file, const proto_message &message,
                           const proto_field &field)
{
    dump_field_attributes(stream, file, message, field.attributes);
}

void dump_cpp_serialize_field(std::ostream &stream, const proto_file &file, const proto_message &message,
                              const proto_oneof &oneof)
{
    stream << "\tconst auto index = value." << oneof.name.get_name() << ".index();\n";
    stream << "\tswitch(index)\n\t{\n";
    for (size_t i = 0; i < oneof.fields.size(); ++i)
    {
        stream << "\t\tcase " << i + 1 << ":\n\t\t\treturn serialize<";
        dump_field_attributes(stream, file, message, oneof.fields[i]);
        stream << ">(stream, std::get<" << i + 1 << ">(value." << oneof.name.get_name() << "), \""
               << json_field_name(oneof.fields[i]) << "\"sv);\n";
    }
    stream << "\t}\n";
}

void dump_cpp_serialize_enum_gen(std::ostream &stream, const proto_enum &my_enum, std::string_view full_name)
{
    if (my_enum.fields.empty())
    {
        stream << "static void serialize_value_gen(auto &, const " << full_name << " &)\n{\n";
        stream << "\treturn ;\n}\n\n";
        return;
    }

    stream << "static void serialize_value_gen(auto &stream, const " << full_name << " &value)\n{\n";
    stream << "\tswitch(value)\n\t{\n";

    std::set<int32_t> numbers_taken;
    for (const auto &field : my_enum.fields)
    {
        if (!numbers_taken.insert(field.number).second)
            continue;

        stream << "\tcase " << full_name << "::" << field.name.get_name()
               << ":\n\t\treturn serialize_enum(stream, \"" << field.name.get_name() << "\"sv);\n";
    }
    stream << "\tdefault:\n\t\tthrow std::system_error(std::make_error_code("
              "std::errc::invalid_argument));\n";
    stream << "\t}\n}\n\n";
}

void dump_cpp_deserialize_enum_gen(std::ostream &stream, const proto_enum &my_enum,
                                   std::string_view full_name)
{
    if (my_enum.fields.empty())
    {
        stream << "void deserialize_value_gen(auto &, " << full_name << " &)\n{\n";
        stream << "\n}\n\n";
        return;
    }

    size_t key_size_min = UINT32_MAX;
    size_t key_size_max = 0;

    auto name_map = std::multimap<uint32_t, std::string_view>();
    for (const auto &field : my_enum.fields)
    {
        name_map.emplace(spb::json::detail::djb2_hash(field.name.get_name()), field.name.get_name());
        key_size_min = std::min(key_size_min, field.name.get_name().size());
        key_size_max = std::max(key_size_max, field.name.get_name().size());
    }

    stream << "void deserialize_value_gen(auto &stream, " << full_name << " & value)\n{\n";
    stream << "\tchar buffer[" << key_size_max << "];\n";
    stream << "\tauto enum_value = deserialize_string_or_int(stream, " << key_size_min << ", " << key_size_max
           << ", buffer);\n";
    stream << "\tstd::visit(detail::overloaded{\n\t\t[&](std::string_view enum_str)\n\t\t{\n";
    stream << "\t\t\tconst auto enum_hash = djb2_hash(enum_str);\n";
    stream << "\t\t\tswitch (enum_hash)\n\t\t\t{\n";
    auto last_hash = name_map.begin()->first + 1;
    auto put_break = false;
    for (const auto &[hash, name] : name_map)
    {
        if (hash != last_hash)
        {
            last_hash = hash;
            if (put_break)
                stream << "\t\t\t\tbreak;\n";

            put_break = true;
            stream << "\t\t\tcase detail::djb2_hash(\"" << name << "\"sv):\n";
        }
        stream << "\t\t\t\tif (enum_str == \"" << name << "\"sv)\n\t\t\t\t{\n\t\t\t\t\tvalue = " << full_name
               << "::" << name << ";\n\t\t\t\t\treturn ;\n\t\t\t\t}\n";
    }
    stream << "\t\t\t\tbreak ;\n";
    stream << "\t\t\t}\n\t\t\tthrow std::system_error(std::make_error_code("
              "std::errc::invalid_argument));\n";
    stream << "\t\t},\n\t\t[&](spb::pb::detail::enum_type enum_int)\n\t\t{\n\t";
    stream << "\t\t\tspb::pb::detail::check_enum_value_or_throw<" << full_name << ">(enum_int);\n";
    stream << "\t\t\t\tvalue = (" << full_name << ")enum_int;\n";
    stream << "\t\t}\n\t}, enum_value);\n}\n\n";
}

void dump_cpp_serialize_field(std::ostream &stream, const proto_file &file, const proto_message &message,
                              const proto_field &field)
{
    stream << "\tserialize<";
    dump_field_attributes(stream, file, message, field);
    stream << ">(stream, value." << field.name.get_name() << ", \"" << json_field_name(field) << "\"sv);\n";
}

void dump_cpp_serialize_field(std::ostream &stream, const proto_file &file, const proto_message &message,
                              const proto_map &map)
{
    stream << "\tserialize<";
    dump_field_attributes(stream, file, message, map.key);
    stream << ">(stream, value." << map.name.get_name() << ", \"" << json_field_name(map) << "\"sv);\n";
}

void dump_cpp_enum_prototype(std::ostream &stream, const proto_enum &, std::string_view full_name)
{
    stream << replace(file_json_header_prototypes, "$", full_name);
}

void dump_cpp_message_prototype(std::ostream &stream, const proto_file &, const proto_message &,
                                std::string_view full_name)
{
    stream << replace(file_json_header_prototypes, "$", full_name);
}

void dump_cpp_serialize_enum(std::ostream &stream, const proto_enum &, std::string_view full_name)
{
    stream << replace(json_serialize_value_template, "$", full_name);
}

void dump_cpp_serialize_message(std::ostream &stream, const proto_file &, const proto_message &,
                                std::string_view full_name)
{
    stream << replace(json_serialize_value_template, "$", full_name);
}

void dump_cpp_serialize_message_gen(std::ostream &stream, const proto_file &file,
                                    const proto_message &message, std::string_view full_name)
{
    if (message.fields.empty() && message.maps.empty() && message.oneofs.empty())
    {
        stream << "static void serialize_value_gen(auto &, const " << full_name << " &)\n{\n}\n\n";
        return;
    }

    stream << "static void serialize_value_gen(auto &stream, const " << full_name << " &value)\n{\n";
    for (const auto &field : message.fields)
    {
        dump_cpp_serialize_field(stream, file, message, field);
    }
    for (const auto &map : message.maps)
    {
        dump_cpp_serialize_field(stream, file, message, map);
    }
    for (const auto &oneof : message.oneofs)
    {
        dump_cpp_serialize_field(stream, file, message, oneof);
    }
    stream << "}\n";
}

void dump_cpp_deserialize_message_gen(std::ostream &stream, const proto_file &file,
                                      const proto_message &message, std::string_view full_name)
{
    if (message.fields.empty() && message.maps.empty() && message.oneofs.empty())
    {
        stream << "void deserialize_value_gen(auto &, " << full_name << " &)\n{\n";
        stream << "\n}\n\n";
        return;
    }

    //- json deserializer needs to accept both camelCase (parsed_name) and the original field name
    struct one_field
    {
        std::string parsed_name;
        cpp_ident name;
        proto_attributes attributes;
        size_t oneof_index = SIZE_MAX;
        std::string_view bitfield;
    };

    size_t key_size_min = UINT32_MAX;
    size_t key_size_max = 0;

    auto name_map = std::multimap<uint32_t, one_field>();
    for (const auto &field : message.fields)
    {
        key_size_min = std::min(key_size_min, field.name.proto_name.size());
        key_size_max = std::max(key_size_max, field.name.proto_name.size());

        const auto field_name = json_field_name_or_camelCase(field);
        name_map.emplace(spb::json::detail::djb2_hash(field_name), one_field{
                                                                       .parsed_name = field_name,
                                                                       .name        = field.name,
                                                                       .attributes  = field.attributes,
                                                                       .bitfield    = field.bit_field,
                                                                   });
        if (field_name != field.name.proto_name)
        {
            key_size_min = std::min(key_size_min, field_name.size());
            key_size_max = std::max(key_size_max, field_name.size());

            name_map.emplace(spb::json::detail::djb2_hash(field.name.proto_name),
                             one_field{
                                 .parsed_name = std::string(field.name.proto_name),
                                 .name        = field.name,
                                 .attributes  = field.attributes,
                                 .bitfield    = field.bit_field,
                             });
        }
    }
    for (const auto &field : message.maps)
    {
        key_size_min = std::min(key_size_min, field.name.proto_name.size());
        key_size_max = std::max(key_size_max, field.name.proto_name.size());

        const auto field_name = json_field_name_or_camelCase(field);
        name_map.emplace(spb::json::detail::djb2_hash(field_name), one_field{
                                                                       .parsed_name = field_name,
                                                                       .name        = field.name,
                                                                   });
        if (field_name != field.name.proto_name)
        {
            key_size_min = std::min(key_size_min, field_name.size());
            key_size_max = std::max(key_size_max, field_name.size());

            name_map.emplace(
                spb::json::detail::djb2_hash(field.name.proto_name),
                one_field{.parsed_name = std::string(field.name.proto_name), .name = field.name});
        }
    }
    for (const auto &oneof : message.oneofs)
    {
        for (size_t i = 0; i < oneof.fields.size(); ++i)
        {
            key_size_min = std::min(key_size_min, oneof.fields[i].name.proto_name.size());
            key_size_max = std::max(key_size_max, oneof.fields[i].name.proto_name.size());

            const auto field_name = json_field_name_or_camelCase(oneof.fields[i]);
            name_map.emplace(spb::json::detail::djb2_hash(field_name), one_field{
                                                                           .parsed_name = field_name,
                                                                           .name        = oneof.name,
                                                                           .oneof_index = i,
                                                                       });
            if (field_name != oneof.fields[i].name.proto_name)
            {
                key_size_min = std::min(key_size_min, field_name.size());
                key_size_max = std::max(key_size_max, field_name.size());

                name_map.emplace(spb::json::detail::djb2_hash(oneof.fields[i].name.proto_name),
                                 one_field{
                                     .parsed_name = std::string(oneof.fields[i].name.proto_name),
                                     .name        = oneof.name,
                                     .oneof_index = i,
                                 });
            }
        }
    }

    stream << "void deserialize_value_gen(auto & stream, " << full_name << " & value)\n{\n";
    stream << "\tchar buffer[" << key_size_max << "];\n";
    stream << "\tauto key = deserialize_key(stream, " << key_size_min << ", " << key_size_max
           << ", buffer);\n";
    stream << "\tswitch (djb2_hash(key))\n\t{\n";

    auto last_hash = name_map.begin()->first + 1;
    auto put_break = false;
    for (const auto &[hash, field] : name_map)
    {
        if (hash != last_hash)
        {
            if (put_break)
                stream << "\t\t\tbreak;\n";

            put_break = true;
            last_hash = hash;
            stream << "\t\tcase detail::djb2_hash(\"" << field.parsed_name << "\"sv):\n";
        }
        stream << "\t\t\tif (key == \"" << field.parsed_name << "\"sv)\n\t\t\t{\n";
        if (field.oneof_index == SIZE_MAX)
        {
            if (!field.bitfield.empty())
            {
                stream << "\t\t\t\tvalue." << field.name.get_name() << " = deserialize_bitfield<";
                dump_field_attributes(stream, file, message, field.attributes);
                stream << ", decltype(value." << field.name.get_name() << ")>(stream, " << field.bitfield
                       << ");\n\t\t\t\treturn ;\n";
            }
            else
            {
                stream << "\t\t\t\treturn deserialize<";
                dump_field_attributes(stream, file, message, field.attributes);
                stream << ">(stream, value." << field.name.get_name() << ");\n";
            }
        }
        else
        {
            stream << "\t\t\t\treturn deserialize_variant<";
            dump_field_attributes(stream, file, message, field.attributes);
            stream << ", " << field.oneof_index + 1 << ">(stream, value." << field.name.get_name() << ");\n";
        }
        stream << "\t\t\t}\n";
    }
    stream << "\t\t\tbreak;\n\t}\n\treturn skip_value(stream);\n}\n";
}

} // namespace

void dump_json_header(const proto_file &file, std::ostream &stream)
{
    dump_cpp_open_namespace(stream, "spb::json");
    stream << file_json_header_template;
    dump_cpp_open_namespace(stream, "detail");
    dump_cpp(stream, file, dump_cpp_message_prototype, dump_cpp_enum_prototype);
    dump_cpp_close_namespace(stream, "detail");
    dump_cpp_close_namespace(stream, "spb::json");
}

void dump_json_cpp(const proto_file &file, const std::filesystem::path &header_file, std::ostream &stream)
{
    dump_cpp_includes(stream, header_file.string());
    dump_cpp_open_namespace(stream, "spb::json::detail");
    dump_cpp(stream, file, dump_cpp_serialize_message_gen, dump_cpp_serialize_enum_gen);
    dump_cpp(stream, file, dump_cpp_deserialize_message_gen, dump_cpp_deserialize_enum_gen);
    dump_cpp(stream, file, dump_cpp_serialize_message, dump_cpp_serialize_enum);
    dump_cpp_close_namespace(stream, "spb::json::detail");
}
