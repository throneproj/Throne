/***************************************************************************\
* Name        : pb dumper                                                   *
* Description : generate C++ src files for pb de/serialization              *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper.h"
#include "../dumper.h"
#include "../header.h"
#include "ast/ast-types.h"
#include "ast/proto-field.h"
#include "ast/proto-file.h"
#include "template-h.h"
#include <spb/io/function_ref.hpp>
#include <string>
#include <string_view>

using namespace std::literals;

namespace
{
void dump_cpp_includes(std::ostream &stream, std::string_view header_file_path)
{
    stream << replace(pb_includes_template, "$", header_file_path);
}

void dump_cpp_close_namespace(std::ostream &stream, std::string_view name)
{
    stream << "} // namespace " << name << "\n";
}

void dump_cpp_open_namespace(std::ostream &stream, std::string_view name)
{
    stream << "namespace " << name << "\n{\n";
}

auto encoder_type_str(const proto_file &file, const proto_field &field) -> std::string
{
    switch (field.type)
    {
    case proto_field::Type::NONE:
    case proto_field::Type::BYTES:
    case proto_field::Type::MESSAGE:
    case proto_field::Type::STRING:
        return {};

    case proto_field::Type::BOOL:
    case proto_field::Type::ENUM:
    case proto_field::Type::INT32:
    case proto_field::Type::UINT32:
    case proto_field::Type::INT64:
    case proto_field::Type::UINT64:
        return is_packed_array(file, field) ? "make_packed(scalar_encoder::varint)"
                                            : "scalar_encoder::varint";

    case proto_field::Type::SINT32:
    case proto_field::Type::SINT64:
        return is_packed_array(file, field) ? "make_packed(scalar_encoder::svarint)"
                                            : "scalar_encoder::svarint";

    case proto_field::Type::FLOAT:
    case proto_field::Type::FIXED32:
    case proto_field::Type::SFIXED32:
        return is_packed_array(file, field) ? "make_packed(scalar_encoder::i32)" : "scalar_encoder::i32";

    case proto_field::Type::DOUBLE:
    case proto_field::Type::FIXED64:
    case proto_field::Type::SFIXED64:
        return is_packed_array(file, field) ? "make_packed(scalar_encoder::i64)" : "scalar_encoder::i64";
    }
    return {};
}

auto field_max_size(const proto_file &file, const proto_message &message, const proto_field &field) -> size_t
{
    return field.attributes.max_size.value_or(
        message.attributes.max_size.value_or(file.attributes.max_size.value_or(0)));
}

auto field_max_count(const proto_file &file, const proto_message &message, const proto_field &field) -> size_t
{
    return field.attributes.max_count.value_or(
        message.attributes.max_count.value_or(file.attributes.max_count.value_or(0)));
}

void dump_serialize_mode(std::ostream &stream, const proto_file &file, const proto_message &message,
                         const proto_field &field)
{
    const auto encoder   = encoder_type_str(file, field);
    const auto max_count = field_max_count(file, message, field);
    const auto max_size  = field_max_size(file, message, field);

    stream << "serialize_mode";

    if (!max_count && !max_size && encoder.empty())
    {
        stream << "{}";
        return;
    }

    auto next = "{";

    if (!encoder.empty())
    {
        stream << next << ".encoder = " << encoder;
        next = ", ";
    }
    if (max_count)
    {
        stream << next << ".max_count = " << max_count;
        next = ", ";
    }
    if (max_size)
        stream << next << ".max_size = " << max_size;
    stream << "}";
}

void dump_serialize_mode(std::ostream &stream, const proto_file &file, const proto_message &,
                         const proto_map &map)
{
    const auto key_encoder   = encoder_type_str(file, map.key);
    const auto value_encoder = encoder_type_str(file, map.value);

    stream << "serialize_mode";

    if (key_encoder.empty() && value_encoder.empty())
    {
        stream << "{}";
        return;
    }

    auto next = "{";
    if (!key_encoder.empty())
    {
        stream << next << ".encoder = " << key_encoder;
        next = ", ";
    }
    if (!value_encoder.empty())
    {
        stream << next << ".encoder2 = " << value_encoder;
    }
    stream << "}";
}

void dump_cpp_serialize_field(std::ostream &stream, const proto_file &file, const proto_message &message,
                              const proto_field &field)
{
    stream << "\tserialize<";
    dump_serialize_mode(stream, file, message, field);
    stream << ">(stream, " << field.number << ", value." << field.name.get_name() << ");\n";
}

void dump_cpp_serialize_field(std::ostream &stream, const proto_file &file, const proto_message &message,
                              const proto_map &field)
{
    stream << "\tserialize<";
    dump_serialize_mode(stream, file, message, field);
    stream << ">(stream, " << field.number << ", value." << field.name.get_name() << ");\n";
}

void dump_cpp_serialize_field(std::ostream &stream, const proto_file &file, const proto_message &message,
                              const proto_oneof &oneof)
{
    stream << "\t{\n\t\tconst auto index = value." << oneof.name.get_name() << ".index( );\n";
    stream << "\t\tswitch (index)\n\t\t{\n";
    for (size_t i = 0; i < oneof.fields.size(); ++i)
    {
        stream << "\t\t\tcase " << i + 1 << ":\n\t\t\t\treturn serialize<";
        dump_serialize_mode(stream, file, message, oneof.fields[i]);
        stream << ">(stream, " << oneof.fields[i].number << ", std::get<" << i + 1 << ">(value."
               << oneof.name.get_name() << "));\n";
    }
    stream << "\t\t}\n\t}\n\n";
}

void dump_cpp_serialize_message(std::ostream &stream, const proto_file &, const proto_message &,
                                std::string_view full_name)
{
    stream << replace(pb_serialize_value_template, "$", full_name);
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
    stream << "}\n\n";
}

void dump_cpp_check_enum_value_or_throw(std::ostream &stream, const proto_enum &my_enum,
                                        std::string_view full_name)
{
    stream << "template<>\n";

    if (my_enum.fields.empty())
    {
        stream << "void check_enum_value_or_throw<" << full_name << ">(enum_type)\n";
        stream << "{\n\treturn ;\n}\n\n";
        return;
    }

    stream << "void check_enum_value_or_throw<" << full_name << ">(enum_type value)\n{\n";
    stream << "\tstatic_assert(sizeof(enum_type) >= sizeof(" << full_name << "));\n\n";
    stream << "\tswitch(value)\n\t{\n";

    std::set<int32_t> numbers_taken;
    for (const auto &field : my_enum.fields)
    {
        if (!numbers_taken.insert(field.number).second)
            continue;

        stream << "\tcase (enum_type)" << full_name << "::" << field.name.get_name() << ":\n";
    }
    stream << "\t\treturn ;\n"
           << "\tdefault:\n"
           << "\t\tthrow std::system_error(std::make_error_code(std::errc::invalid_argument));\n";
    stream << "\t}\n}\n\n";
}

void dump_enum_prototypes(std::ostream &stream, const proto_enum &, std::string_view full_name)
{
    stream << replace(file_pb_header_enum_prototypes, "$", full_name);
}

void dump_message_prototypes(std::ostream &stream, const proto_file &, const proto_message &,
                             std::string_view full_name)
{
    stream << replace(file_pb_header_prototypes, "$", full_name);
}

void dump_cpp_deserialize_message_gen(std::ostream &stream, const proto_file &file,
                                      const proto_message &message, std::string_view full_name)
{
    if (message.fields.empty() && message.maps.empty() && message.oneofs.empty())
    {
        stream << "static void deserialize_value_gen(auto &stream, " << full_name << " &, tag_type tag)\n{\n";
        stream << "\tskip(stream, wire_type_from_tag(tag));\n}\n\n";
        return;
    }

    stream << "static void deserialize_value_gen(auto &stream, " << full_name << " &value, tag_type tag)\n{\n"
           << "\tconst auto type = wire_type_from_tag(tag);\n";

    stream << "\tswitch(field_from_tag(tag))\n\t{\n";

    for (const auto &field : message.fields)
    {
        stream << "\t\tcase " << field.number << ":\n\t\t\t";
        if (!field.bit_field.empty())
        {
            stream << "value." << field.name.get_name() << " = deserialize_bitfield<";
            dump_serialize_mode(stream, file, message, field);
            stream << ", decltype( value." + std::string(field.name.get_name()) << ")>(stream, "
                   << field.bit_field << ", type);\n";
            stream << "\t\t\treturn;\n";
        }
        else
        {
            stream << "return deserialize<";
            dump_serialize_mode(stream, file, message, field);
            stream << ">(stream, value." << field.name.get_name() << ", type);\n";
        }
    }
    for (const auto &map : message.maps)
    {
        stream << "\t\tcase " << map.number << ":\n\t\t\treturn ";
        stream << "\tdeserialize<";
        dump_serialize_mode(stream, file, message, map);
        stream << ">(stream, value." << map.name.get_name() << ", type);\n";
    }
    for (const auto &oneof : message.oneofs)
    {
        for (size_t i = 0; i < oneof.fields.size(); ++i)
        {
            stream << "\t\tcase " << oneof.fields[i].number << ":\n\t\t\treturn ";
            stream << "\tdeserialize_variant<";
            dump_serialize_mode(stream, file, message, oneof.fields[i]);
            stream << ", " << i + 1 << ">(stream, value." << oneof.name.get_name() << ", type);\n";
        }
    }

    stream << "\t\tdefault:\n\t\t\treturn skip(stream, type);\t\n\t}\n}\n\n";
}
} // namespace

void dump_pb_header(const proto_file &file, std::ostream &stream)
{
    dump_cpp_open_namespace(stream, "spb::pb");
    stream << file_pb_header_template;
    dump_cpp_open_namespace(stream, "detail");
    dump_cpp(stream, file, dump_message_prototypes, dump_enum_prototypes);
    dump_cpp_close_namespace(stream, "detail");
    dump_cpp_close_namespace(stream, "spb::pb");
}

void dump_pb_cpp(const proto_file &file, const std::filesystem::path &header_file, std::ostream &stream)
{
    dump_cpp_includes(stream, header_file.string());
    dump_cpp_open_namespace(stream, "spb::pb::detail");
    dump_cpp(stream, file, dump_cpp_serialize_message_gen, dump_cpp_check_enum_value_or_throw);
    dump_cpp(stream, file, dump_cpp_deserialize_message_gen, nullptr);
    dump_cpp(stream, file, dump_cpp_serialize_message, nullptr);
    dump_cpp_close_namespace(stream, "spb::pb::detail");
}
