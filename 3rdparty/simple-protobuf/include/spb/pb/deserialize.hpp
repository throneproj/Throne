/***************************************************************************\
* Name        : deserialize library for protobuf                            *
* Description : all protobuf deserialization functions                      *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "../bits.h"
#include "../concepts.h"
#include "../utf8.h"
#include "wire-types.h"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <spb/io/io.hpp>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace spb::pb::detail
{

struct istream_reader
{
    size_t bytes_left;
    size_t consumed_bytes = 0;
    spb::io::reader on_read;

    istream_reader(spb::io::reader reader, size_t size = std::numeric_limits<size_t>::max()) noexcept
        : bytes_left(size), on_read(reader)
    {
    }
    size_t consumed_size() const noexcept
    {
        return consumed_bytes;
    }
    size_t size() const noexcept
    {
        return bytes_left;
    }
    bool empty() const noexcept
    {
        return bytes_left == 0;
    }

    [[nodiscard]] size_t read(void *data, size_t data_size)
    {
        data_size = std::min(data_size, size());
        if (data_size == 0) [[unlikely]]
            return 0;

        const auto size = on_read(data, data_size);
        bytes_left -= size;
        consumed_bytes += size;
        return size;
    }

    [[nodiscard]] uint8_t read_byte_or_throw()
    {
        uint8_t result;

        if (read(&result, sizeof(result)) != sizeof(result)) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        return result;
    }

    [[nodiscard]] int read_byte_or_eof()
    {
        uint8_t result;
        return read(&result, sizeof(result)) == sizeof(result) ? result : -1;
    }

    void read_exact_or_throw(void *data, size_t data_size)
    {
        while (data_size > 0)
        {
            auto chunk_size = read(data, data_size);
            if (chunk_size == 0) [[unlikely]]
                throw std::runtime_error("unexpected end of stream");

            data_size -= chunk_size;
        }
    }

    [[nodiscard]] istream_reader sub_stream(size_t sub_size)
    {
        if (size() < sub_size) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        bytes_left -= sub_size;
        consumed_bytes += sub_size;
        return istream_reader(on_read, sub_size);
    }

    void skip_or_throw(size_t size)
    {
        uint8_t buffer[128];
        while (size > 0)
        {
            const auto chunk_size = std::min(size, sizeof(buffer));
            read_exact_or_throw(buffer, chunk_size);
            size -= chunk_size;
        }
    }
};

struct istream_buffer
{
    const uint8_t *p_start;
    const uint8_t *p_end;

    istream_buffer(const uint8_t *start, const uint8_t *end) noexcept : p_start(start), p_end(end)
    {
        assert(start <= end);
    }
    istream_buffer(const uint8_t *start, size_t size) noexcept : p_start(start), p_end(start + size)
    {
    }
    size_t size() const noexcept
    {
        return p_end - p_start;
    }
    bool empty() const noexcept
    {
        return p_start >= p_end;
    }

    [[nodiscard]] size_t read(void *data, size_t data_size) noexcept
    {
        data_size = std::min(data_size, size());
        memcpy(data, p_start, data_size);
        p_start += data_size;
        return data_size;
    }

    [[nodiscard]] uint8_t read_byte_or_throw()
    {
        if (p_start >= p_end) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        return *p_start++;
    }

    [[nodiscard]] int read_byte_or_eof() noexcept
    {
        if (p_start < p_end) [[likely]]
            return *p_start++;

        return -1;
    }

    void read_exact_or_throw(void *data, size_t data_size)
    {
        if (read(data, data_size) != data_size) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");
    }

    [[nodiscard]] istream_buffer sub_stream(size_t sub_size)
    {
        if (size() < sub_size) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        const auto sub_start = p_start;
        p_start += sub_size;
        return istream_buffer(sub_start, sub_size);
    }

    void skip_or_throw(size_t size)
    {
        if (this->size() < size) [[unlikely]]
            throw std::runtime_error("unexpected end of stream");

        p_start += size;
    }
};

void skip(auto &stream, wire_type);

template <typename T> void check_enum_value_or_throw(enum_type);

template <serialize_mode, size_t ordinal, typename T>
void deserialize_variant(auto &stream, T &variant, wire_type type);

template <serialize_mode, typename T>
auto deserialize_bitfield(auto &stream, uint32_t bits, wire_type type) -> T;

[[nodiscard]] inline auto wire_type_from_tag(tag_type tag) -> wire_type
{
    return wire_type(uint32_t(tag) & 0x07);
}

[[nodiscard]] inline auto field_from_tag(tag_type tag) -> uint32_t
{
    return uint32_t(tag) >> 3;
}

inline void check_tag_or_throw(tag_type tag)
{
    if (field_from_tag(tag) == 0) [[unlikely]]
        throw std::runtime_error("invalid field id");
}

inline void check_wire_type_or_throw(wire_type type1, wire_type type2)
{
    if (type1 != type2) [[unlikely]]
        throw std::runtime_error("invalid wire type");
}

void check_if_empty_or_throw(auto &stream)
{
    if (!stream.empty()) [[unlikely]]
        throw std::runtime_error("unexpected data in stream");
}

[[nodiscard]] auto read_tag_or_eof(auto &stream) -> tag_type
{
    auto byte_or_eof = stream.read_byte_or_eof();
    if (byte_or_eof < 0) [[unlikely]]
        return tag_type::invalid;

    auto byte = (uint8_t)(byte_or_eof);
    auto tag  = (uint32_t)(byte & 0x7F);

    for (size_t shift = CHAR_BIT - 1; (byte & 0x80) != 0; shift += CHAR_BIT - 1)
    {
        if (shift >= sizeof(tag) * CHAR_BIT) [[unlikely]]
            throw std::runtime_error("invalid tag");

        byte = stream.read_byte_or_throw();
        tag |= uint64_t(byte & 0x7F) << shift;
    }

    const auto result = tag_type(tag);
    check_tag_or_throw(result);
    return result;
}

template <typename T> [[nodiscard]] auto read_varint(auto &stream) -> T
{
    if constexpr (std::is_same_v<T, bool>)
    {
        switch (stream.read_byte_or_throw())
        {
        case 0:
            return false;
        case 1:
            return true;
        default:
            throw std::runtime_error("invalid varint for bool");
        }
    }
    else
    {
        auto value = uint64_t(0);

        for (auto shift = 0U; shift < sizeof(value) * CHAR_BIT; shift += CHAR_BIT - 1)
        {
            uint8_t byte = stream.read_byte_or_throw();
            value |= uint64_t(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0)
            {
                if constexpr (std::is_signed_v<T> && sizeof(T) < sizeof(value))
                {
                    //- GPB encodes signed varints always as 64-bits
                    //- so int32_t(-2) is encoded as "\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01",
                    // same as int64_t(-2)
                    //- but it should be encoded as  "\xfe\xff\xff\xff\x0f"
                    value = T(value);
                }
                auto result = T(value);
                if constexpr (std::is_signed_v<T>)
                {
                    if (result == std::make_signed_t<T>(value)) [[likely]]
                        return result;
                }
                else
                {
                    if (result == value) [[likely]]
                        return result;
                }

                break;
            }
        }
        throw std::runtime_error("invalid varint");
    }
}

template <serialize_mode>
void deserialize(auto &stream, spb::detail::proto_message auto &value, wire_type type);
template <serialize_mode>
void deserialize(auto &stream, spb::detail::proto_field_int_or_float auto &value, wire_type type);
template <serialize_mode>
void deserialize(auto &stream, spb::detail::proto_field_bytes auto &value, wire_type type);
template <serialize_mode>
void deserialize(auto &stream, spb::detail::proto_field_string auto &value, wire_type type);
template <serialize_mode, spb::detail::proto_label_repeated Container>
void deserialize(auto &stream, Container &value, wire_type type);
template <serialize_mode, spb::detail::proto_label_repeated_fixed_size Container>
void deserialize(auto &stream, Container &value, wire_type type);
template <serialize_mode, spb::detail::proto_label_optional Container>
void deserialize(auto &stream, Container &p_value, wire_type type);

template <serialize_mode> void deserialize(auto &stream, spb::detail::proto_map auto &value, wire_type type);

template <serialize_mode, typename T>
void deserialize(auto &stream, std::unique_ptr<T> &value, wire_type type);

template <typename T, typename signedT, typename unsignedT> auto create_tmp_var()
{
    if constexpr (std::is_signed<T>::value)
    {
        return signedT();
    }
    else
    {
        return unsignedT();
    }
}

template <serialize_mode mode, typename T>
auto deserialize_bitfield(auto &stream, uint32_t bits, wire_type type) -> T
{
    auto value = T();
    if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::svarint)
    {
        check_wire_type_or_throw(type, wire_type::varint);

        auto tmp = read_varint<std::make_unsigned_t<T>>(stream);
        value    = T((tmp >> 1) ^ (~(tmp & 1) + 1));
    }
    else if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::varint)
    {
        check_wire_type_or_throw(type, wire_type::varint);
        value = read_varint<T>(stream);
    }
    else if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::i32)
    {
        static_assert(sizeof(T) <= sizeof(uint32_t));

        check_wire_type_or_throw(type, wire_type::fixed32);

        if constexpr (sizeof(value) == sizeof(uint32_t))
        {
            stream.read_exact_or_throw(&value, sizeof(value));
        }
        else
        {
            auto tmp = create_tmp_var<T, int32_t, uint32_t>();
            stream.read_exact_or_throw(&tmp, sizeof(tmp));
            spb::detail::check_if_value_fit_in_bits(tmp, bits);
            value = T(tmp);
        }
    }
    else if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::i64)
    {
        static_assert(sizeof(T) <= sizeof(uint64_t));
        check_wire_type_or_throw(type, wire_type::fixed64);

        if constexpr (sizeof(value) == sizeof(uint64_t))
        {
            stream.read_exact_or_throw(&value, sizeof(value));
        }
        else
        {
            auto tmp = create_tmp_var<T, int64_t, uint64_t>();
            stream.read_exact_or_throw(&tmp, sizeof(tmp));
            spb::detail::check_if_value_fit_in_bits(tmp, bits);
            value = T(tmp);
        }
    }
    spb::detail::check_if_value_fit_in_bits(value, bits);
    return value;
}

template <serialize_mode mode>
void deserialize(auto &stream, spb::detail::proto_enum auto &value, wire_type type)
{
    using T = std::remove_cvref_t<decltype(value)>;

    if constexpr (!is_packed(mode.encoder))
    {
        check_wire_type_or_throw(type, wire_type::varint);
    }
    const auto int_enum = read_varint<enum_type>(stream);
    check_enum_value_or_throw<T>(int_enum);
    value = (T)int_enum;
}

template <serialize_mode mode>
void deserialize(auto &stream, spb::detail::proto_field_int_or_float auto &value, wire_type type)
{
    using T = std::remove_cvref_t<decltype(value)>;

    if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::svarint)
    {
        if constexpr (!is_packed(mode.encoder))
        {
            check_wire_type_or_throw(type, wire_type::varint);
        }
        auto tmp = read_varint<std::make_unsigned_t<T>>(stream);
        value    = T((tmp >> 1) ^ (~(tmp & 1) + 1));
    }
    else if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::varint)
    {
        if constexpr (!is_packed(mode.encoder))
        {
            check_wire_type_or_throw(type, wire_type::varint);
        }
        value = read_varint<T>(stream);
    }
    else if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::i32)
    {
        static_assert(sizeof(T) <= sizeof(uint32_t));

        if constexpr (!is_packed(mode.encoder))
        {
            check_wire_type_or_throw(type, wire_type::fixed32);
        }
        if constexpr (sizeof(value) == sizeof(uint32_t))
        {
            stream.read_exact_or_throw(&value, sizeof(value));
        }
        else
        {
            if constexpr (std::is_signed_v<T>)
            {
                auto tmp = int32_t(0);
                stream.read_exact_or_throw(&tmp, sizeof(tmp));
                if (tmp > std::numeric_limits<T>::max() || tmp < std::numeric_limits<T>::min()) [[unlikely]]
                    throw std::runtime_error("int overflow");

                value = T(tmp);
            }
            else
            {
                auto tmp = uint32_t(0);
                stream.read_exact_or_throw(&tmp, sizeof(tmp));
                if (tmp > std::numeric_limits<T>::max()) [[unlikely]]
                    throw std::runtime_error("int overflow");

                value = T(tmp);
            }
        }
    }
    else if constexpr (scalar_encoder(mode.encoder) == scalar_encoder::i64)
    {
        static_assert(sizeof(T) <= sizeof(uint64_t));
        if constexpr (!is_packed(mode.encoder))
        {
            check_wire_type_or_throw(type, wire_type::fixed64);
        }
        if constexpr (sizeof(value) == sizeof(uint64_t))
        {
            stream.read_exact_or_throw(&value, sizeof(value));
        }
        else
        {
            if constexpr (std::is_signed_v<T>)
            {
                auto tmp = int64_t(0);
                stream.read_exact_or_throw(&tmp, sizeof(tmp));
                if (tmp > std::numeric_limits<T>::max() || tmp < std::numeric_limits<T>::min()) [[unlikely]]
                    throw std::runtime_error("int overflow");

                value = T(tmp);
            }
            else
            {
                auto tmp = uint64_t(0);
                stream.read_exact_or_throw(&tmp, sizeof(tmp));
                if (tmp > std::numeric_limits<T>::max()) [[unlikely]]
                    throw std::runtime_error("int overflow");

                value = T(tmp);
            }
        }
    }
}

template <serialize_mode mode, spb::detail::proto_label_optional Container>
void deserialize(auto &stream, Container &p_value, wire_type type)
{
    auto &value = p_value.emplace(typename Container::value_type());
    deserialize<mode>(stream, value, type);
}

template <serialize_mode mode>
void deserialize(auto &stream, spb::detail::proto_field_string auto &value, wire_type type)
{
    check_wire_type_or_throw(type, wire_type::length_delimited);
    if constexpr (mode.max_size)
        check_size(stream.size(), mode.max_size);

    if constexpr (spb::detail::proto_field_string_resizable<decltype(value)>)
    {
        value.resize(stream.size());
    }
    else
    {
        if (value.size() != stream.size()) [[unlikely]]
            throw std::runtime_error("invalid string size");
    }
    stream.read_exact_or_throw(value.data(), stream.size());
    spb::detail::utf8::validate(std::string_view(value.data(), value.size()));
}

template <serialize_mode mode, typename T>
void deserialize(auto &stream, std::unique_ptr<T> &value, wire_type type)
{
    value = std::make_unique<T>();
    deserialize<mode>(stream, *value, type);
}

template <serialize_mode mode>
void deserialize(auto &stream, spb::detail::proto_field_bytes auto &value, wire_type type)
{
    check_wire_type_or_throw(type, wire_type::length_delimited);

    if constexpr (mode.max_size)
        check_size(stream.size(), mode.max_size);

    if constexpr (spb::detail::proto_field_bytes_resizable<decltype(value)>)
    {
        value.resize(stream.size());
    }
    else
    {
        if (stream.size() != value.size()) [[unlikely]]
            throw std::runtime_error("invalid bytes size");
    }
    stream.read_exact_or_throw(value.data(), stream.size());
}

template <serialize_mode mode, spb::detail::proto_label_repeated Container>
void deserialize_packed(auto &stream, Container &value)
{
    static_assert(is_packed(mode.encoder));

    while (!stream.empty())
    {
        if constexpr (mode.max_count)
            check_size(value.size() + 1, mode.max_count);

        if constexpr (std::is_same_v<typename Container::value_type, bool>)
        {
            value.emplace_back(read_varint<bool>(stream));
        }
        else
        {
            deserialize<reset_packed(mode)>(stream, value.emplace_back(), to_wire_type(mode.encoder));
        }
    }
}

template <serialize_mode mode, spb::detail::proto_label_repeated_fixed_size Container>
void deserialize_packed(auto &stream, Container &value)
{
    static_assert(is_packed(mode.encoder));

    using value_type = typename Container::value_type;

    for (size_t i = 0; i < value.size(); i++)
    {
        if constexpr (std::is_same_v<value_type, bool>)
        {
            value[i] = read_varint<bool>(stream);
        }
        else
        {
            value_type tmp;
            deserialize<reset_packed(mode)>(stream, tmp, to_wire_type(mode.encoder));
            value[i] = tmp;
        }
    }
    check_if_empty_or_throw(stream);
}

template <serialize_mode mode, spb::detail::proto_label_repeated_fixed_size Container>
void deserialize(auto &stream, Container &value, wire_type type)
{
    static_assert(is_packed(mode.encoder), "repeated field with fixed size has to have attribute 'packed'");

    check_wire_type_or_throw(type, wire_type::length_delimited);
    deserialize_packed<mode>(stream, value);
}

template <serialize_mode mode, spb::detail::proto_label_repeated Container>
void deserialize(auto &stream, Container &value, wire_type type)
{
    if constexpr (is_packed(mode.encoder))
    {
        deserialize_packed<mode>(stream, value);
    }
    else
    {
        if constexpr (mode.max_count)
            check_size(value.size() + 1, mode.max_count);

        if constexpr (std::is_same_v<typename Container::value_type, bool>)
        {
            value.emplace_back(read_varint<bool>(stream));
        }
        else
        {
            deserialize<mode>(stream, value.emplace_back(), type);
        }
    }
}

template <serialize_mode mode>
void deserialize(auto &stream, spb::detail::proto_map auto &value, wire_type type)
{
    using map_type    = std::remove_cvref_t<decltype(value)>;
    using key_type    = typename map_type::key_type;
    using mapped_type = typename map_type::mapped_type;

    constexpr auto key_encoder   = serialize_mode{.encoder = mode.encoder};
    constexpr auto value_encoder = serialize_mode{.encoder = mode.encoder2};

    check_wire_type_or_throw(type, wire_type::length_delimited);

    auto pair          = std::pair<key_type, mapped_type>();
    auto key_or_value_defined   = false;
    while (!stream.empty())
    {
        const auto tag          = tag_type(read_varint<uint32_t>(stream));
        const auto field_number = field_from_tag(tag);
        const auto field_type   = wire_type_from_tag(tag);

        check_tag_or_throw(tag);

        switch (field_number)
        {
        case 1:
            if constexpr (std::is_integral_v<key_type>)
            {
                deserialize<key_encoder>(stream, pair.first, field_type);
            }
            else
            {
                if (field_type == wire_type::length_delimited)
                {
                    const auto size = read_varint<uint32_t>(stream);
                    auto substream  = stream.sub_stream(size);
                    deserialize<key_encoder>(substream, pair.first, field_type);
                    check_if_empty_or_throw(substream);
                }
                else
                {
                    deserialize<key_encoder>(stream, pair.first, field_type);
                }
            }
            key_or_value_defined = true;
            break;
        case 2:
            if constexpr (spb::detail::proto_field_number<mapped_type>)
            {
                deserialize<value_encoder>(stream, pair.second, field_type);
            }
            else
            {
                if (field_type == wire_type::length_delimited)
                {
                    const auto size = read_varint<uint32_t>(stream);
                    auto substream  = stream.sub_stream(size);
                    deserialize<value_encoder>(substream, pair.second, field_type);
                    check_if_empty_or_throw(substream);
                }
                else [[unlikely]]
                {
                    throw std::runtime_error("invalid field");
                }
            }
            key_or_value_defined = true;
            break;
        default:
            throw std::runtime_error("invalid field");
        }
    }
    if (key_or_value_defined) [[likely]]
    {
        value.insert(std::move(pair));
    }
}

template <serialize_mode mode, size_t ordinal, typename T>
void deserialize_variant(auto &stream, T &variant, wire_type type)
{
    deserialize<mode>(stream, variant.template emplace<ordinal>(), type);
}

template <serialize_mode>
void deserialize(auto &stream, spb::detail::proto_message auto &value, wire_type type)
{
    check_wire_type_or_throw(type, wire_type::length_delimited);

    while (!stream.empty())
    {
        const auto tag = read_tag_or_eof(stream);
        if (tag == tag_type::invalid)
            return;

        const auto field_type = wire_type_from_tag(tag);
        if (field_type == wire_type::length_delimited)
        {
            const auto size = read_varint<uint32_t>(stream);
            auto substream  = stream.sub_stream(size);
            deserialize_value(substream, value, tag);
            check_if_empty_or_throw(substream);
        }
        else
        {
            deserialize_value(stream, value, tag);
        }
    }
}
template <serialize_mode mode> void deserialize(auto &stream, spb::detail::proto_message auto &value)
{
    return deserialize<mode>(stream, value, wire_type::length_delimited);
}

void skip(auto &stream, wire_type type)
{
    switch (type)
    {
    case wire_type::varint:
        return (void)read_varint<uint64_t>(stream);
    case wire_type::length_delimited:
        return stream.skip_or_throw(stream.size());
    case wire_type::fixed32:
        return stream.skip_or_throw(sizeof(uint32_t));
    case wire_type::fixed64:
        return stream.skip_or_throw(sizeof(uint64_t));
    default:
        throw std::runtime_error("invalid wire type");
    }
}
} // namespace spb::pb::detail
