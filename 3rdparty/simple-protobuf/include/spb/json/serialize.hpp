/***************************************************************************\
* Name        : serialize library for json                                  *
* Description : all json serialization functions                            *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "../concepts.h"

#include "../to_from_chars.h"
#include "base64.h"
#include "field.hpp"
#include "spb/utf8.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <spb/io/io.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <type_traits>

namespace spb::json::detail
{
struct ostream_size
{
    static constexpr bool size_only = true;
    size_t size;
    bool put_comma = false;

    void write(uint8_t) noexcept
    {
        size += sizeof(uint8_t);
    }

    void write(const void *, size_t data_size) noexcept
    {
        size += data_size;
    }
};

struct ostream_buffer
{
    static constexpr bool size_only = false;

    uint8_t *p_buffer;
    bool put_comma = false;

    explicit ostream_buffer(void *buffer) : p_buffer((std::uint8_t *)buffer)
    {
    }

    void write(uint8_t byte) noexcept
    {
        *p_buffer++ = byte;
    }

    void write(const void *data, size_t data_size) noexcept
    {
        memcpy(p_buffer, data, data_size);
        p_buffer += data_size;
    }
};

struct ostream_writer
{
    static constexpr bool size_only = false;
    spb::io::writer on_write;
    size_t size    = 0;
    bool put_comma = false;

    explicit ostream_writer(spb::io::writer writer) : on_write(writer)
    {
    }

    void write(uint8_t byte)
    {
        write(&byte, 1);
    }

    void write(const void *data, size_t data_size)
    {
        on_write(data, data_size);
        size += data_size;
    }
};

template <field_attributes = field_attributes{}> size_t serialize_size(const auto &value);

void write_unicode(auto &stream, uint32_t codepoint)
{
    if (codepoint <= 0xffff)
    {
        char buffer[8] = {};
        auto size      = snprintf(buffer, sizeof(buffer), "\\u%04" PRIx32, codepoint);
        return stream.write(buffer, size);
    }
    if (codepoint <= 0x10FFFF)
    {
        codepoint -= 0x10000;

        auto high       = static_cast<uint16_t>((codepoint >> 10) + 0xD800);
        auto low        = static_cast<uint16_t>((codepoint & 0x3FF) + 0xDC00);
        char buffer[16] = {};
        auto size       = snprintf(buffer, sizeof(buffer), "\\u%04" PRIx16 "\\u%04" PRIx16, high, low);
        return stream.write(buffer, size);
    }
    throw std::invalid_argument("invalid utf8");
}

template <size_t N> void write_string(auto &stream, const char (&string)[N])
{
    stream.write(string, N - 1);
}

static constexpr auto is_escape(uint8_t c) -> bool
{
    switch (c)
    {
    case '\\':
    case '\"':
    case '<':
    case '>':
        return true;
    default:
        return (c < ' ') | (c >= 0x7f);
    }
}

static constexpr auto has_escape_chars(std::string_view str) -> bool
{
    return std::any_of(str.begin(), str.end(), is_escape);
}

void write_escaped(auto &stream, std::string_view str)
{
    if (!has_escape_chars(str)) [[likely]]
    {
        stream.write(str.data(), str.size());
        return;
    }

    uint32_t codepoint = 0;
    uint32_t state     = spb::detail::utf8::ok;
    bool decoding_utf8 = false;
    for (uint8_t c : str)
    {
        if (decoding_utf8)
        {
            if (spb::detail::utf8::decode_point(&state, &codepoint, c) == spb::detail::utf8::ok)
            {
                write_unicode(stream, codepoint);
                decoding_utf8 = false;
            }
            continue;
        }
        if (is_escape(c))
        {
            switch (c)
            {
            case '"':
                write_string(stream, R"(\")");
                break;
            case '\\':
                write_string(stream, R"(\\)");
                break;
            case '\b':
                write_string(stream, R"(\b)");
                break;
            case '\f':
                write_string(stream, R"(\f)");
                break;
            case '\n':
                write_string(stream, R"(\n)");
                break;
            case '\r':
                write_string(stream, R"(\r)");
                break;
            case '\t':
                write_string(stream, R"(\t)");
                break;
            default:
                decoding_utf8 = true;
                if (spb::detail::utf8::decode_point(&state, &codepoint, c) == spb::detail::utf8::ok)
                {
                    write_unicode(stream, codepoint);
                    decoding_utf8 = false;
                }
            }
        }
        else
        {
            stream.write(c);
        }
    }
    if (state != spb::detail::utf8::ok) [[unlikely]]
    {
        throw std::runtime_error("invalid utf8");
    }
}

void put_comma_if_needed(auto &stream)
{
    if (stream.put_comma)
        stream.write(',');

    stream.put_comma = true;
}

void serialize_key(auto &stream, std::string_view key)
{
    put_comma_if_needed(stream);

    stream.write('"');
    stream.write(key.data(), key.size());
    write_string(stream, R"(":)");
}

template <field_attributes> void serialize(auto &stream, const bool &value);
template <field_attributes> void serialize(auto &stream, const bool &value, std::string_view field);

template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_field_int_or_float auto &value);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_field_int_or_float auto &value, std::string_view field);

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_message auto &value);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_message auto &value, std::string_view field);

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_enum auto &value);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_enum auto &value, std::string_view field);

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_field_string auto &value);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_field_string auto &value, std::string_view field);

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_field_bytes auto &value);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_field_bytes auto &value, std::string_view field);

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_optional auto &p_value);
template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_optional auto &p_value, std::string_view field);

template <field_attributes attributes, typename T>
void serialize(auto &stream, const std::unique_ptr<T> &p_value);
template <field_attributes attributes, typename T>
void serialize(auto &stream, const std::unique_ptr<T> &p_value, std::string_view field);

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_label_repeated auto &value);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_label_repeated auto &value, std::string_view field);

template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_label_repeated_fixed_size auto &value);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_label_repeated_fixed_size auto &value,
               std::string_view field);

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_map auto &map);
template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_map auto &map, std::string_view field);

template <field_attributes> void serialize(auto &stream, const bool &value)
{
    return value ? write_string(stream, "true") : write_string(stream, "false");
}

void serialize_enum(auto &stream, std::string_view value)
{
    stream.write('"');
    stream.write(value.data(), value.size());
    stream.write('"');
}

template <field_attributes attributes> void serialize(auto &stream, const std::string_view &value)
{
    using stream_type = std::remove_cvref_t<decltype(stream)>;

    if constexpr (!stream_type::size_only && attributes.max_size)
        check_size(value.size(), attributes.max_size);

    stream.write('"');
    write_escaped(stream, value);
    stream.write('"');
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_field_string auto &value)
{
    serialize<attributes>(stream, std::string_view(value.data(), value.size()));
}

template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_field_int_or_float auto &value)
{
    auto buffer = std::array<char, 32>();
    auto result = spb_std_emu::to_chars(buffer.data(), buffer.data() + sizeof(buffer), value);
    stream.write(buffer.data(), static_cast<size_t>(result.ptr - buffer.data()));
}

template <field_attributes attributes> void serialize(auto &stream, const bool &value, std::string_view field)
{
    serialize_key(stream, field);
    serialize<attributes>(stream, value);
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_field_int_or_float auto &value, std::string_view field)
{
    serialize_key(stream, field);
    serialize<attributes>(stream, value);
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_field_string auto &value, std::string_view field)
{
    if (value.empty())
        return;

    serialize_key(stream, field);
    serialize<attributes>(stream, value);
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_field_bytes auto &value)
{
    using stream_type = std::remove_cvref_t<decltype(stream)>;

    if constexpr (!stream_type::size_only && attributes.max_size)
        check_size(value.size(), attributes.max_size);

    stream.write('"');
    base64_encode(stream, value);
    stream.write('"');
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_field_bytes auto &value, std::string_view field)
{
    if (value.empty())
        return;

    serialize_key(stream, field);
    serialize<attributes>(stream, value);
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_repeated auto &value, std::string_view field)
{
    if (value.empty())
        return;

    serialize_key(stream, field);
    serialize<attributes>(stream, value);
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_repeated auto &value)
{
    using stream_type = std::remove_cvref_t<decltype(stream)>;

    if constexpr (!stream_type::size_only && attributes.max_count)
        check_size(value.size(), attributes.max_count);

    stream.write('[');
    stream.put_comma = false;
    for (const auto &v : value)
    {
        put_comma_if_needed(stream);

        if constexpr (std::is_same_v<typename std::decay_t<decltype(value)>::value_type, bool>)
        {
            serialize<attributes>(stream, bool(v));
        }
        else
        {
            serialize<attributes>(stream, v);
        }
    }
    stream.write(']');
    stream.put_comma = true;
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_repeated_fixed_size auto &value)
{
    stream.write('[');
    stream.put_comma = false;
    for (size_t i = 0; i < value.size(); i++)
    {
        put_comma_if_needed(stream);

        if constexpr (std::is_same_v<typename std::decay_t<decltype(value)>::value_type, bool>)
        {
            serialize<attributes>(stream, bool(value[i]));
        }
        else
        {
            serialize<attributes>(stream, value[i]);
        }
    }
    stream.write(']');
    stream.put_comma = true;
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_repeated_fixed_size auto &value,
               std::string_view field)
{
    serialize_key(stream, field);
    serialize<attributes>(stream, value);
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_map auto &map, std::string_view field)
{
    if (map.empty())
        return;

    serialize_key(stream, field);
    serialize<attributes>(stream, map);
}

template <field_attributes attributes> void serialize(auto &stream, const spb::detail::proto_map auto &map)
{
    using map_type = std::remove_cvref_t<decltype(map)>;
    using key_type = typename map_type::key_type;

    stream.write('{');
    stream.put_comma = false;
    for (const auto &[map_key, map_value] : map)
    {
        if constexpr (std::is_same_v<key_type, std::string_view> || std::is_same_v<key_type, std::string>)
        {
            serialize_key(stream, map_key);
        }
        else
        {
            if (stream.put_comma)
                stream.write(',');

            stream.write('"');
            serialize<attributes>(stream, map_key);
            write_string(stream, R"(":)");
        }
        stream.put_comma = false;
        serialize<attributes>(stream, map_value);
        stream.put_comma = true;
    }
    stream.write('}');
    stream.put_comma = true;
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_optional auto &p_value)
{
    if (p_value.has_value())
        return serialize<attributes>(stream, *p_value);
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_label_optional auto &p_value, std::string_view field)
{
    if (p_value.has_value())
        return serialize<attributes>(stream, *p_value, field);
}

template <field_attributes attributes, typename T>
void serialize(auto &stream, const std::unique_ptr<T> &p_value)
{
    if (p_value)
        return serialize<attributes>(stream, *p_value);
}

template <field_attributes attributes, typename T>
void serialize(auto &stream, const std::unique_ptr<T> &p_value, std::string_view field)
{
    if (p_value)
        return serialize<attributes>(stream, *p_value, field);
}

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_message auto &value)
{
    stream.write('{');
    stream.put_comma = false;
    serialize_value(stream, value);
    stream.write('}');
    stream.put_comma = true;
}

template <field_attributes attributes>
void serialize(auto &stream, const spb::detail::proto_message auto &value, std::string_view field)
{
    serialize_key(stream, field);
    serialize<attributes>(stream, value);
}

template <field_attributes> void serialize(auto &stream, const spb::detail::proto_enum auto &value)
{
    serialize_value(stream, value);
}

template <field_attributes>
void serialize(auto &stream, const spb::detail::proto_enum auto &value, std::string_view field)
{
    serialize_key(stream, field);
    serialize_value(stream, value);
}

template <field_attributes attributes> size_t serialize_size(const auto &value)
{
    auto stream = ostream_size();
    serialize<attributes>(stream, value);
    return stream.size;
}

} // namespace spb::json::detail
