/***************************************************************************\
* Name        : ast options resolver                                        *
* Description : map proto_options to proto_attributes                       *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "ast-options.h"
#include "dumper/header.h"
#include <initializer_list>
#include <span>
#include <spb/to_from_chars.h>

namespace
{
enum class option_type
{
    option_file,
    option_message,
    option_field,
    option_enum,
};

using namespace std::literals;

auto option_value(std::span<const std::string_view> path, const proto_option &option) -> const proto_option *;

auto option_value(std::span<const std::string_view> path, const proto_options &options)
    -> const proto_option *
{
    if (path.empty())
        return nullptr;

    auto p_option = options.find(path.front());
    if (p_option == options.end())
        return nullptr;

    if (auto result = option_value(path.subspan(1), p_option->second); result)
        return result;

    return nullptr;
}

auto option_value(std::span<const std::string_view> path, const proto_option &option) -> const proto_option *
{
    if (path.empty())
        return &option;

    for (const auto &options : option.options)
    {
        if (auto result = option_value(path, options); result)
            return result;
    }

    return nullptr;
}

auto option_value(std::initializer_list<const std::string_view> path, const proto_options &options)
    -> std::string_view
{
    auto option = option_value(std::span(path.begin(), path.size()), options);
    return option && !option->value.empty() ? *option->value.begin() : std::string_view{};
}

auto option_values(std::initializer_list<const std::string_view> path, const proto_options &options)
    -> std::span<const proto_option_value>
{
    auto option = option_value(std::span(path.begin(), path.size()), options);
    return option ? option->value : std::span<const proto_option_value>{};
}

auto option_value_bool(const proto_file &file, std::initializer_list<const std::string_view> full_name,
                       const proto_options &options) -> std::optional<bool>
{
    auto option = option_value(full_name, options);
    if (option.empty())
        return {};

    if (option == "true")
        return true;

    if (option == "false")
        return false;

    throw_parse_error(file, option, "Expecting bool (\"true\", \"false\")");
}

template <typename T>
auto option_value_int(const proto_file &file, std::initializer_list<const std::string_view> full_name,
                      const proto_options &options) -> std::optional<T>
{
    auto option = option_value(full_name, options);
    if (option.empty())
        return {};

    auto value  = T{};
    auto result = spb_std_emu::from_chars(option.data(), option.data() + option.size(), value);
    if (result.ec == std::errc{}) [[likely]]
        return value;

    throw_parse_error(file, option, "Expecting integer (1, 3, ...)");
}

void convert_nanopb_options(const proto_file &file, proto_attributes &attributes,
                            const proto_options &options, std::string_view opt_value)
{
    if (auto values = option_values({opt_value, "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto value = option_value_int<uint32_t>(file, {opt_value, "max_size"}, options); value.has_value())
        attributes.max_size = *value;

    if (auto value = option_value_int<uint32_t>(file, {opt_value, "max_length"}, options); value.has_value())
        attributes.max_size = *value + 1;
}

void convert_nanopb_options(const proto_file &file, proto_attributes &attributes,
                            const proto_options &options, option_type type)
{
    switch (type)
    {
    case option_type::option_field:
        return convert_nanopb_options(file, attributes, options, "nanopb");
    case option_type::option_message:
        return convert_nanopb_options(file, attributes, options, "nanopb_msgopt");
    case option_type::option_file:
        return convert_nanopb_options(file, attributes, options, "nanopb_fileopt");
    default:;
    }
}

void convert_gpb_options(const proto_file &file, proto_attributes &attributes, const proto_options &options)
{
    if (auto value = option_value({"default"}, options); !value.empty())
        attributes.default_ = value;

    if (auto value = option_value_bool(file, {"deprecated"}, options); value.has_value())
        attributes.deprecated = *value;

    if (auto value = option_value_bool(file, {"packed"}, options); value.has_value())
        attributes.packed = *value;

    if (auto value = option_value({"json_name"}, options); !value.empty())
        attributes.json_name = value;
}

void convert_spb_options_legacy(proto_attributes &attributes, const proto_options &options)
{
    if (auto value = option_value({"field", "type"}, options); !value.empty())
        attributes.type = value;

    if (auto value = option_value({"enum", "type"}, options); !value.empty())
        attributes.enum_ = value;

    if (auto value = option_value({"optional", "type"}, options); !value.empty())
        attributes.optional = value;

    if (auto value = option_value({"repeated", "type"}, options); !value.empty())
        attributes.repeated = value;

    if (auto value = option_value({"pointer", "type"}, options); !value.empty())
        attributes.pointer = value;

    if (auto value = option_value({"bytes", "type"}, options); !value.empty())
        attributes.bytes = value;

    if (auto value = option_value({"string", "type"}, options); !value.empty())
        attributes.string = value;

    if (auto value = option_value({"map", "type"}, options); !value.empty())
        attributes.map = value;
}

void convert_spb_options(const proto_file &file, proto_attributes &attributes, const proto_options &options,
                         std::string_view opt_name)
{
    if (auto value = option_value({opt_name, "type"}, options); !value.empty())
        attributes.type = value;

    if (auto value = option_value({opt_name, "enum"}, options); !value.empty())
        attributes.enum_ = value;

    if (auto value = option_value({opt_name, "optional"}, options); !value.empty())
        attributes.optional = value;

    if (auto value = option_value({opt_name, "repeated"}, options); !value.empty())
        attributes.repeated = value;

    if (auto value = option_value({opt_name, "pointer"}, options); !value.empty())
        attributes.pointer = value;

    if (auto value = option_value({opt_name, "bytes"}, options); !value.empty())
        attributes.bytes = value;

    if (auto value = option_value({opt_name, "string"}, options); !value.empty())
        attributes.string = value;

    if (auto value = option_value({opt_name, "map"}, options); !value.empty())
        attributes.map = value;

    if (auto value = option_value_int<uint32_t>(file, {opt_name, "max_size"}, options); value.has_value())
        attributes.max_size = value;

    if (auto value = option_value_int<uint32_t>(file, {opt_name, "max_count"}, options); value.has_value())
        attributes.max_count = value;
}
void convert_spb_options(const proto_file &file, proto_attributes &attributes, const proto_options &options,
                         option_type type, bool legacy)
{
    if (legacy)
        convert_spb_options_legacy(attributes, options);

    switch (type)
    {
    case option_type::option_field:
        return convert_spb_options(file, attributes, options, "spb_opt");
    case option_type::option_message:
        return convert_spb_options(file, attributes, options, "spb_msgopt");
    case option_type::option_file:
        return convert_spb_options(file, attributes, options, "spb_fileopt");
    case option_type::option_enum:
        return convert_spb_options(file, attributes, options, "spb_enumopt");
    default:;
    }
}

void convert_include_options(proto_attributes &attributes, const proto_options &options)
{
    if (auto values = option_values({"spb", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"spb_msgopt", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"spb_fileopt", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"spb_enumopt", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"repeated", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"optional", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"pointer", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"bytes", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"string", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());

    if (auto values = option_values({"map", "include"}, options); !values.empty())
        attributes.include.insert(values.begin(), values.end());
}

void convert_options(proto_file &file, proto_attributes &attributes, const proto_options &options,
                     option_type type, bool legacy)
{
    convert_include_options(file.attributes, options);
    if (legacy)
        convert_gpb_options(file, attributes, options);
    convert_spb_options(file, attributes, options, type, legacy);
    convert_nanopb_options(file, attributes, options, type);
}

void resolve_options(proto_file &file, proto_enum &enum_)
{
    convert_options(file, enum_.attributes, enum_.options, option_type::option_enum, true);
}

void resolve_options(proto_file &file, proto_message &message)
{
    convert_options(file, message.attributes, message.options, option_type::option_message, true);
    convert_options(file, file.attributes, message.options, option_type::option_file, false);
    for (auto &field : message.fields)
    {
        convert_options(file, field.attributes, field.options, option_type::option_field, true);
        convert_options(file, message.attributes, field.options, option_type::option_message, false);
    }
    for (auto &map : message.maps)
    {
        convert_options(file, map.attributes, map.options, option_type::option_field, true);
        convert_options(file, message.attributes, map.options, option_type::option_message, false);
    }
    for (auto &sub_message : message.messages)
    {
        resolve_options(file, sub_message);
    }
    for (auto &enum_ : message.enums)
    {
        resolve_options(file, enum_);
    }
}
} // namespace

void resolve_options(proto_file &file)
{
    convert_options(file, file.attributes, file.options, option_type::option_file, true);
    resolve_options(file, file.package);
}
