/***************************************************************************\
* Name        : CPP dumper                                                  *
* Description : generate C++ src files for de/serialization                 *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper.h"
#include "header.h"
#include "pb/dumper.h"
#include "json/dumper.h"
#include <string_view>

void dump_cpp_header(const proto_file &file, std::ostream &stream)
{
    try
    {
        dump_cpp_definitions(file, stream);
        dump_pb_header(file, stream);
        dump_json_header(file, stream);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(file.path.string() + ":" + e.what());
    }
}

void dump_cpp(const proto_file &file, const std::filesystem::path &header_file, std::ostream &file_stream)
{
    try
    {
        dump_pb_cpp(file, header_file, file_stream);
        dump_json_cpp(file, header_file, file_stream);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(file.path.string() + ":" + e.what());
    }
}

void dump_cpp_enum(std::ostream &stream, const proto_enum &my_enum, std::string_view parent,
                   enum_dumper dump_enum)
{
    if (!dump_enum)
        return;

    const auto full_name = std::string(parent) + "::" + std::string(my_enum.name.get_name());
    dump_enum(stream, my_enum, full_name);
}

void dump_cpp_enums(std::ostream &stream, const proto_enums &enums, std::string_view parent,
                    enum_dumper dump_enum)
{
    for (const auto &my_enum : enums)
    {
        dump_cpp_enum(stream, my_enum, parent, dump_enum);
    }
}

void dump_cpp_messages(std::ostream &stream, const proto_file &file, const proto_messages &messages,
                       std::string_view parent, message_dumper dump_message, enum_dumper dump_enum);

void dump_cpp_message(std::ostream &stream, const proto_file &file, const proto_message &message,
                      std::string_view parent, message_dumper dump_message, enum_dumper dump_enum)
{
    const auto full_name = std::string(parent) + "::" + std::string(message.name.get_name());

    if (dump_message)
        dump_message(stream, file, message, full_name);

    dump_cpp_enums(stream, message.enums, full_name, dump_enum);
    dump_cpp_messages(stream, file, message.messages, full_name, dump_message, dump_enum);
}

void dump_cpp_messages(std::ostream &stream, const proto_file &file, const proto_messages &messages,
                       std::string_view parent, message_dumper dump_message, enum_dumper dump_enum)
{
    for (const auto &message : messages)
    {
        dump_cpp_message(stream, file, message, parent, dump_message, dump_enum);
    }
}

void dump_cpp(std::ostream &stream, const proto_file &file, message_dumper dump_message,
              enum_dumper dump_enum)
{
    const auto str_namespace = file.package.name.get_name().empty()
                                   ? std::string()
                                   : "::" + std::string(file.package.name.get_name());
    dump_cpp_enums(stream, file.package.enums, str_namespace, dump_enum);
    dump_cpp_messages(stream, file, file.package.messages, str_namespace, dump_message, dump_enum);
}
