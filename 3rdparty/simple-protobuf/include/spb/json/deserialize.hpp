/***************************************************************************\
* Name        : deserialize library for json                                *
* Description : all json deserialization functions                          *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "../bits.h"
#include "../concepts.h"
#include "../to_from_chars.h"
#include "../utf8.h"
#include "base64.h"
#include "field.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <spb/io/buffer-io.hpp>
#include <spb/io/io.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace spb::json::detail
{
using namespace std::literals;

static const auto escape = '\\';

/**
 * @brief helper for std::variant visit
 *        https://en.cppreference.com/w/cpp/utility/variant/visit
 *
 */
template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};
// explicit deduction guide (not needed as of C++20)
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

/**
 * @brief djb2_hash for strings
 *        http://www.cse.yorku.ca/~oz/hash.html
 *
 * @param str
 * @return uint32_t
 */
static constexpr inline auto djb2_hash(std::string_view str) noexcept -> uint32_t
{
    uint32_t hash = 5381U;

    for (auto c : str)
    {
        hash = ((hash << 5U) + hash) + uint8_t(c); /* hash * 33 + c */
    }

    return hash;
}

static constexpr inline auto fnv1a_hash(std::string_view str) noexcept -> uint64_t
{
    uint64_t hash        = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;

    for (auto c : str)
    {
        hash *= prime;
        hash ^= c;
    }

    return hash;
}

template <spb::detail::proto_field_bytes T> void clear(T &container)
{
    if constexpr (spb::detail::proto_field_bytes_resizable<T>)
    {
        container.clear();
    }
    else
    {
        std::fill(container.begin(), container.end(), typename T::value_type());
    }
}

struct istream_buffer
{
    const uint8_t *p_start;
    const uint8_t *p_end;

    istream_buffer(const void *start, const void *end) noexcept
        : p_start((uint8_t *)start), p_end((uint8_t *)end)
    {
        assert(start <= end);
    }
    istream_buffer(const void *start, size_t size) noexcept : p_start((uint8_t *)start), p_end(p_start + size)
    {
    }
    [[nodiscard]] size_t size() const noexcept
    {
        return p_end - p_start;
    }
    [[nodiscard]] bool empty() const noexcept
    {
        return p_start >= p_end;
    }
    void skip(size_t chars)
    {
        assert(size() >= chars);
        p_start += chars;
    }
    [[nodiscard]] bool consume(char c)
    {
        if (current_char() != c)
            return false;

        ++p_start;
        return true;
    }

    [[nodiscard]] bool consume_and_skip_white_space(char c)
    {
        if (!consume(c))
            return false;

        skip_white_spaces();
        return true;
    }

    [[nodiscard]] bool consume_and_skip_white_space(std::string_view token)
    {
        assert(!token.empty());

        if (size() < token.size()) [[unlikely]]
            return false;

        if (memcmp(token.data(), p_start, token.size()) != 0)
            return false;

        if (size() == token.size()) [[unlikely]]
            return true;

        const auto char_behind_token = p_start[token.size()];
        if (!isalnum(char_behind_token) && char_behind_token != '_')
        {
            skip(token.size());
            return true;
        }

        if (isspace(char_behind_token))
        {
            skip(token.size());
            skip_white_spaces();
            return true;
        }
        return false;
    }
    [[nodiscard]] int current_char() const
    {
        return (p_start < p_end) ? *p_start : -1;
    }
    void skip_white_spaces() noexcept
    {
        while (!empty() && isspace(*p_start))
        {
            ++p_start;
        }
    }
    void consume_current_char(bool skip_white_space)
    {
        if (empty()) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        ++p_start;
        if (skip_white_space)
            skip_white_spaces();
    }

    [[nodiscard]] auto view(size_t min_size, size_t max_size) -> std::string_view
    {
        if (size() < min_size) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        return {(char *)p_start, std::min(size(), max_size)};
    }
};

struct istream_reader
{
  private:
    spb::io::buffered_reader reader;
    size_t m_consumed_size = 0;

    void consume_bytes(size_t size)
    {
        m_consumed_size += size;
        reader.skip(size);
    }

  public:
    istream_reader(spb::io::reader reader) : reader(reader)
    {
    }

    size_t consumed_size() const
    {
        return m_consumed_size;
    }

    [[nodiscard]] auto current_char() -> char
    {
        auto view = reader.view(1);
        if (view.empty())
            throw std::runtime_error("unexpected end of stream");

        return view[0];
    }

    [[nodiscard]] auto consume(char c) -> bool
    {
        if (current_char() != c)
            return false;

        skip(1);
        return true;
    }

    [[nodiscard]] auto consume_and_skip_white_space(char c) -> bool
    {
        if (!consume(c))
            return false;

        skip_white_spaces();
        return true;
    }

    [[nodiscard]] auto consume_and_skip_white_space(std::string_view token) -> bool
    {
        assert(!token.empty());

        const auto token_view = reader.view(token.size() + 1);
        if (!token_view.starts_with(token))
            return false;

        if (token_view.size() == token.size()) [[unlikely]]
            return true;

        const auto char_behind_token = token_view.back();
        if (!isalnum(char_behind_token) && char_behind_token != '_')
        {
            skip(token.size());
            return true;
        }

        if (isspace(char_behind_token))
        {
            skip(token.size());
            skip_white_spaces();
            return true;
        }
        return false;
    }

    void skip_white_spaces()
    {
        for (;;)
        {
            auto view = reader.view(1);
            if (view.empty())
                return;

            size_t spaces = 0;
            for (auto c : view)
            {
                if (!isspace(c))
                {
                    consume_bytes(spaces);
                    return;
                }
                spaces += 1;
            }
            consume_bytes(spaces);
        }
    }

    [[nodiscard]] auto view(size_t min_size, size_t max_size) -> std::string_view
    {
        auto result = reader.view(max_size);
        if (result.size() < min_size) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        if (result.size() > max_size)
            result = result.substr(0, max_size);

        return result;
    }

    void consume_current_char(bool skip_white_space) noexcept
    {
        consume_bytes(1);
        if (skip_white_space)
            skip_white_spaces();
    }

    void skip(size_t size)
    {
        consume_bytes(size);
    }
};

[[nodiscard]] bool eof(const auto &stream)
{
    return stream.current_char() == -1;
}

template <field_attributes> void deserialize(auto &stream, spb::detail::proto_enum auto &value)
{
    deserialize_value(stream, value);
}

void ignore_string_until_double_quota(auto &stream)
{
    for (;;)
    {
        auto view = stream.view(1, UINT32_MAX);
        auto pos  = view.find_first_of(R"(\")");
        if (pos == view.npos)
        {
            stream.skip(view.size());
            continue;
        }

        if (view[pos] == '"') [[likely]]
        {
            // +1 for '""
            stream.skip(pos + 1);
            stream.skip_white_spaces();
            return;
        }

        stream.skip(pos + 1);
        // +1 for char behind '\'
        (void)stream.view(1, UINT32_MAX);
        stream.skip(1);
    }
}

void ignore_string(auto &stream)
{
    if (!stream.consume('"')) [[unlikely]]
        throw std::runtime_error(R"(expecting '"')");

    if (stream.consume_and_skip_white_space('"')) [[unlikely]]
        return;

    ignore_string_until_double_quota(stream);
}

auto deserialize_string_to_buffer(auto &stream, size_t min_size, size_t max_size, char *buffer)
    -> std::string_view
{
    assert(max_size < io::buffered_reader::BUFFER_SIZE);

    if (!stream.consume('"')) [[unlikely]]
        throw std::runtime_error(R"(expecting '"')");

    // +1 for "
    auto view    = stream.view(1, max_size + 1);
    size_t start = 0;
    for (;;)
    {
        const auto pos = view.find_first_of(R"(\")", start);
        if (pos == view.npos) [[unlikely]]
        {
            stream.skip(view.size());
            break;
        }

        if (view[pos] == '"') [[likely]]
        {
            memcpy(buffer, view.data(), pos);
            // +1 for '""
            stream.skip(pos + 1);
            stream.skip_white_spaces();
            return (pos >= min_size) ? std::string_view{buffer, pos} : std::string_view{};
        }

        // +2 for \ and char behind
        start = pos + 2;
    }
    ignore_string_until_double_quota(stream);
    return {};
}

auto unicode_from_hex(auto &stream) -> uint16_t
{
    const auto esc_size = 4U;
    auto unicode_view   = stream.view(esc_size, esc_size);
    if (unicode_view.size() < esc_size) [[unlikely]]
        throw std::runtime_error("invalid escape sequence");

    auto value  = uint16_t(0);
    auto result = spb_std_emu::from_chars(unicode_view.data(), unicode_view.data() + esc_size, value, 16);
    if (result.ec != std::errc{} || result.ptr != unicode_view.data() + esc_size) [[unlikely]]
        throw std::runtime_error("invalid escape sequence");

    stream.skip(esc_size);
    return value;
}

auto unescape_unicode(auto &stream, char utf8[4]) -> uint32_t
{
    auto value = uint32_t(unicode_from_hex(stream));
    if (value >= 0xD800 && value <= 0xDBFF && stream.view(2, 2).starts_with("\\u"sv))
    {
        stream.skip(2);
        auto low = unicode_from_hex(stream);

        if (low < 0xDC00 || low > 0xDFFF) [[unlikely]]
            throw std::invalid_argument("invalid escape sequence");

        value = ((value - 0xD800) << 10) + (low - 0xDC00) + 0x10000;
    }
    if (auto result = spb::detail::utf8::encode_point(value, utf8); result != 0)
        return result;

    throw std::runtime_error("invalid escape sequence");
}

auto unescape(auto &stream, char utf8[4]) -> uint32_t
{
    auto c = stream.current_char();
    stream.consume_current_char(false);
    switch (c)
    {
    case '"':
        utf8[0] = '"';
        return 1;
    case '\\':
        utf8[0] = '\\';
        return 1;
    case '/':
        utf8[0] = '/';
        return 1;
    case 'b':
        utf8[0] = '\b';
        return 1;
    case 'f':
        utf8[0] = '\f';
        return 1;
    case 'n':
        utf8[0] = '\n';
        return 1;
    case 'r':
        utf8[0] = '\r';
        return 1;
    case 't':
        utf8[0] = '\t';
        return 1;
    case 'u':
        return unescape_unicode(stream, utf8);
    default:
        throw std::runtime_error("invalid escape sequence");
    }
}

template <field_attributes attributes>
void deserialize(auto &stream, spb::detail::proto_field_string auto &value)
{
    if (!stream.consume('"')) [[unlikely]]
        throw std::runtime_error(R"(expecting '"')");

    if constexpr (spb::detail::proto_field_string_resizable<decltype(value)>)
    {
        value.clear();
    }
    auto index           = size_t(0);
    auto append_to_value = [&](const char *str, size_t size)
    {
        if constexpr (attributes.max_size)
            check_size(value.size() + size, attributes.max_size);

        if constexpr (spb::detail::proto_field_string_resizable<decltype(value)>)
        {
            value.append(str, size);
        }
        else
        {
            if (auto space_left = value.size() - index; size <= space_left) [[likely]]
            {
                memcpy(value.data() + index, str, size);
                index += size;
            }
            else
            {
                throw std::runtime_error("invalid string size");
            }
        }
    };

    for (;;)
    {
        auto view  = stream.view(1, UINT32_MAX);
        auto found = view.find_first_of(R"("\)");
        if (found == view.npos) [[unlikely]]
        {
            append_to_value(view.data(), view.size());
            stream.skip(view.size());
            continue;
        }

        append_to_value(view.data(), found);
        // +1 for '"' or '\'
        stream.skip(found + 1);
        if (view[found] == '"') [[likely]]
        {
            if constexpr (!spb::detail::proto_field_string_resizable<decltype(value)>)
            {
                if (index != value.size()) [[unlikely]]
                    throw std::runtime_error("invalid string size");
            }
            return;
        }
        char utf8_buffer[4];
        auto utf8_size = unescape(stream, utf8_buffer);
        append_to_value(utf8_buffer, utf8_size);
    }
    spb::detail::utf8::validate(std::string_view(value.data(), value.size()));
}

template <field_attributes> void deserialize(auto &stream, spb::detail::proto_field_int_or_float auto &value)
{
    if (stream.current_char() == '"') [[unlikely]]
    {
        //- https://protobuf.dev/programming-guides/proto2/#json
        //- number can be a string
        char buffer[32];
        auto view   = deserialize_string_to_buffer(stream, 1, 32, buffer);
        auto result = spb_std_emu::from_chars(view.data(), view.data() + view.size(), value);
        if (result.ec != std::errc{} || result.ptr != (view.data() + view.size())) [[unlikely]]
            throw std::runtime_error("invalid number");

        return;
    }
    auto view   = stream.view(1, 32);
    auto result = spb_std_emu::from_chars(view.data(), view.data() + view.size(), value);
    if (result.ec != std::errc{}) [[unlikely]]
        throw std::runtime_error("invalid number");

    stream.skip(result.ptr - view.data());
}

template <field_attributes> void deserialize(auto &stream, bool &value)
{
    if (stream.consume_and_skip_white_space("true"sv))
    {
        value = true;
    }
    else if (stream.consume_and_skip_white_space("false"sv))
    {
        value = false;
    }
    else [[unlikely]]
    {
        throw std::runtime_error("expecting 'true' or 'false'");
    }
}

template <field_attributes> void deserialize(auto &stream, spb::detail::proto_message auto &value);
template <field_attributes> void deserialize(auto &stream, spb::detail::proto_map auto &value);

template <field_attributes attributes, spb::detail::proto_label_optional Container>
void deserialize(auto &stream, Container &p_value);

template <field_attributes attributes, spb::detail::proto_label_repeated Container>
void deserialize(auto &stream, Container &value)
{
    if (stream.consume_and_skip_white_space("null"sv))
    {
        value.clear();
        return;
    }

    if (!stream.consume_and_skip_white_space('[')) [[unlikely]]
        throw std::runtime_error("expecting '['");

    if (stream.consume_and_skip_white_space(']'))
        return;

    do
    {
        if constexpr (attributes.max_count)
            check_size(value.size() + 1, attributes.max_count);

        if constexpr (std::is_same_v<typename Container::value_type, bool>)
        {
            auto b = false;
            deserialize<attributes>(stream, b);
            value.push_back(b);
        }
        else
        {
            deserialize<attributes>(stream, value.emplace_back());
        }
    } while (stream.consume_and_skip_white_space(','));

    if (!stream.consume_and_skip_white_space(']')) [[unlikely]]
        throw std::runtime_error("expecting ']'");
}

template <field_attributes attributes, spb::detail::proto_label_repeated_fixed_size Container>
void deserialize(auto &stream, Container &value)
{
    if (stream.consume_and_skip_white_space("null"sv))
    {
        typename Container::value_type tmp = {};
        for (size_t i = 0; i < value.size(); i++)
        {
            value[i] = tmp;
        }
        return;
    }

    if (!stream.consume_and_skip_white_space('[')) [[unlikely]]
        throw std::runtime_error("expecting '['");

    for (size_t i = 0; i < value.size(); i++)
    {
        typename Container::value_type tmp;
        deserialize<attributes>(stream, tmp);
        value[i] = tmp;
        if (i + 1 >= value.size())
            break;

        while (stream.consume_and_skip_white_space(','))
            ;
    }

    if (!stream.consume_and_skip_white_space(']')) [[unlikely]]
        throw std::runtime_error("expecting ']'");
}

template <field_attributes attributes>
void deserialize(auto &stream, spb::detail::proto_field_bytes auto &value)
{
    if (stream.consume_and_skip_white_space("null"sv))
    {
        clear(value);
        return;
    }

    base64_decode_string(value, stream, attributes.max_size);
}

template <field_attributes attributes, typename T> void deserialize_map_key(auto &stream, T &map_key)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        deserialize<attributes>(stream, map_key);
    }
    else
    {
        char buffer[128];
        auto str_key_map = deserialize_string_to_buffer(stream, 1, 128, buffer);
        auto key_stream  = istream_buffer(str_key_map.data(), str_key_map.size());
        deserialize<attributes>(key_stream, map_key);
    }
}

template <field_attributes attributes> void deserialize(auto &stream, spb::detail::proto_map auto &value)
{
    using map_type    = std::remove_cvref_t<decltype(value)>;
    using key_type    = typename map_type::key_type;
    using mapped_type = typename map_type::mapped_type;

    if (stream.consume_and_skip_white_space("null"sv))
    {
        value.clear();
        return;
    }

    if (!stream.consume_and_skip_white_space('{')) [[unlikely]]
        throw std::runtime_error("expecting '{'");

    if (stream.consume_and_skip_white_space('}'))
        return;

    do
    {
        auto map_key = key_type();
        deserialize_map_key<attributes>(stream, map_key);
        if (!stream.consume_and_skip_white_space(':')) [[unlikely]]
            throw std::runtime_error("expecting ':'");

        auto map_value = mapped_type();
        deserialize<attributes>(stream, map_value);
        value.emplace(std::move(map_key), std::move(map_value));
    } while (stream.consume_and_skip_white_space(','));

    if (!stream.consume_and_skip_white_space('}')) [[unlikely]]
        throw std::runtime_error("expecting '}'");
}

template <field_attributes attributes, spb::detail::proto_label_optional Container>
void deserialize(auto &stream, Container &p_value)
{
    if (stream.consume_and_skip_white_space("null"sv))
    {
        p_value.reset();
        return;
    }

    if (p_value.has_value())
    {
        deserialize<attributes>(stream, *p_value);
    }
    else
    {
        deserialize<attributes>(stream, p_value.emplace(typename Container::value_type()));
    }
}

template <field_attributes attributes, typename T> void deserialize(auto &stream, std::unique_ptr<T> &value)
{
    if (stream.consume_and_skip_white_space("null"sv))
    {
        value.reset();
        return;
    }

    if (value)
    {
        deserialize<attributes>(stream, *value);
    }
    else
    {
        value = std::make_unique<T>();
        deserialize<attributes>(stream, *value);
    }
}

void ignore_value(auto &stream);
void ignore_key_and_value(auto &stream)
{
    ignore_string(stream);
    if (!stream.consume_and_skip_white_space(':')) [[unlikely]]
        throw std::runtime_error("expecting ':'");

    ignore_value(stream);
}

void ignore_object(auto &stream)
{
    //- '{' was already checked by caller
    stream.consume_current_char(true);

    if (stream.consume_and_skip_white_space('}'))
        return;

    do
    {
        ignore_key_and_value(stream);
    } while (stream.consume_and_skip_white_space(','));

    if (!stream.consume_and_skip_white_space('}'))
    {
        throw std::runtime_error("expecting '}'");
    }
}

void ignore_array(auto &stream)
{
    //- '[' was already checked by caller
    stream.consume_current_char(true);

    if (stream.consume_and_skip_white_space(']'))
        return;

    do
    {
        ignore_value(stream);
    } while (stream.consume_and_skip_white_space(','));

    if (!stream.consume_and_skip_white_space(']')) [[unlikely]]
        throw std::runtime_error("expecting ']");
}

void ignore_number(auto &stream)
{
    double value;
    deserialize<field_attributes{}>(stream, value);
}

void ignore_bool(auto &stream)
{
    bool value;
    deserialize<field_attributes{}>(stream, value);
}

void ignore_null(auto &stream)
{
    if (!stream.consume_and_skip_white_space("null"sv)) [[unlikely]]
        throw std::runtime_error("expecting 'null'");
}

void ignore_value(auto &stream)
{
    switch (stream.current_char())
    {
    case '{':
        return ignore_object(stream);
    case '[':
        return ignore_array(stream);
    case '"':
        return ignore_string(stream);
    case 'n':
        return ignore_null(stream);
    case 't':
    case 'f':
        return ignore_bool(stream);
    default:
        return ignore_number(stream);
    }
}

template <field_attributes attributes, typename T> T deserialize_bitfield(auto &stream, uint32_t bits)
{
    auto value = T();
    deserialize<attributes>(stream, value);
    spb::detail::check_if_value_fit_in_bits(value, bits);
    return value;
}

template <field_attributes attributes, size_t ordinal, typename T>
void deserialize_variant(auto &stream, T &variant)
{
    deserialize<attributes>(stream, variant.template emplace<ordinal>());
}

template <field_attributes> void deserialize(auto &stream, spb::detail::proto_message auto &value)
{
    if (!stream.consume_and_skip_white_space('{')) [[unlikely]]
        throw std::runtime_error("expecting '{'");

    if (stream.consume_and_skip_white_space('}'))
        return;

    for (;;)
    {
        //- generated by the sprotoc
        deserialize_value(stream, value);

        if (stream.consume_and_skip_white_space(','))
            continue;

        if (stream.consume_and_skip_white_space('}'))
            return;

        throw std::runtime_error("expecting '}' or ','");
    }
}

auto deserialize_key(auto &stream, size_t min_size, size_t max_size, char *buffer) -> std::string_view
{
    auto key = deserialize_string_to_buffer(stream, min_size, max_size, buffer);
    if (!stream.consume_and_skip_white_space(':')) [[unlikely]]
        throw std::runtime_error("expecting ':'");

    return key;
}

auto deserialize_string_or_int(auto &stream, size_t min_size, size_t max_size, char *buffer)
    -> std::variant<std::string_view, int32_t>
{
    if (stream.current_char() == '"')
        return deserialize_string_to_buffer(stream, min_size, max_size, buffer);

    return deserialize_int(stream);
}

auto deserialize_int(auto &stream) -> int32_t
{
    int32_t result;
    deserialize<field_attributes{}>(stream, result);
    return result;
}

void skip_value(auto &stream)
{
    ignore_value(stream);
}

} // namespace spb::json::detail
