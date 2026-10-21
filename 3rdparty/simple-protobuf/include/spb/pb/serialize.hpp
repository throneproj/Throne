/***************************************************************************\
* Name        : serialize library for protobuf                              *
* Description : all protobuf serialization functions                        *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "../concepts.h"
#include "../utf8.h"
#include "wire-types.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <spb/io/io.hpp>
#include <sys/types.h>
#include <type_traits>

namespace spb::pb::detail
{
struct ostream_size
{
    static constexpr bool size_only = true;
    size_t size;

    void write(const void *, size_t data_size)
    {
        size += data_size;
    }

    void write(uint8_t)
    {
        ++size;
    }
};
struct ostream_buffer
{
    static constexpr bool size_only = false;
    uint8_t *p_buffer;

    explicit ostream_buffer(void *buffer) : p_buffer((uint8_t *)buffer)
    {
    }

    void write(uint8_t byte)
    {
        *p_buffer++ = byte;
    }

    void write(const void *data, size_t size)
    {
        memcpy(p_buffer, data, size);
        p_buffer += size;
    }
};

struct ostream_writer
{
    static constexpr bool size_only = false;
    spb::io::writer on_write;
    size_t size = 0;

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

template <typename T> void check_enum_value_or_throw(enum_type);
template <serialize_mode = serialize_mode{}> size_t serialize_size(const auto &value);
template <serialize_mode = serialize_mode{}> size_t serialize_size(uint32_t field, const auto &value);
template <serialize_mode> void serialize(auto &stream, const spb::detail::proto_message auto &value);

inline size_t serialize_varint_size(uint64_t value)
{
    size_t size = 1;
    while (value >= 0x80)
    {
        ++size;
        value >>= 7;
    }
    return size;
}

void serialize_varint(auto &stream, uint64_t value)
{
    while (value >= 0x80)
    {
        stream.write((uint8_t)(value & 0x7F) | 0x80);
        value >>= 7;
    }
    stream.write((uint8_t)value);
}

void serialize_svarint(auto &stream, int64_t value)
{
    const auto tmp = uint64_t((value << 1) ^ (value >> 63));
    serialize_varint(stream, tmp);
}

void serialize_tag(auto &stream, uint32_t field_number, wire_type type)
{
    const auto tag = (field_number << 3) | uint32_t(type);
    serialize_varint(stream, tag);
}

template <serialize_mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_message auto &value);
template <serialize_mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_field_string auto &value);
template <serialize_mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_field_bytes auto &value);
template <serialize_mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_label_repeated auto &value);
template <serialize_mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_label_repeated_fixed_size auto &value);

template <serialize_mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_map auto &value);

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field, spb::detail::proto_field_number auto value)
{
    serialize_tag(stream, field, to_wire_type(mode.encoder));
    serialize<mode>(stream, value);
}

template <serialize_mode mode> void serialize(auto &stream, const spb::detail::proto_enum auto &value)
{
    using T = std::remove_cvref_t<decltype(value)>;

    const auto int_enum = (enum_type)value;
    check_enum_value_or_throw<T>(int_enum);
    serialize_varint(stream, int_enum);
}

template <serialize_mode mode> void serialize(auto &stream, spb::detail::proto_field_int_or_float auto value)
{
    using T = std::remove_cvref_t<decltype(value)>;

    constexpr auto type = encoder_type(mode.encoder);
    if constexpr (type == scalar_encoder::varint)
    {
        static_assert(std::is_integral_v<T>);

        if constexpr (std::is_same_v<bool, T>)
        {
            const uint8_t tmp = value ? 1 : 0;
            return stream.write(tmp);
        }
        else if constexpr (std::is_signed_v<T>)
        {
            //- GPB is serializing all negative ints always as int64_t
            const auto u_value = uint64_t(int64_t(value));
            return serialize_varint(stream, u_value);
        }
        else
        {
            return serialize_varint(stream, value);
        }
    }
    else if constexpr (type == scalar_encoder::svarint)
    {
        static_assert(std::is_signed_v<T> && std::is_integral_v<T>);

        return serialize_svarint(stream, value);
    }
    else if constexpr (type == scalar_encoder::i32)
    {
        if constexpr (sizeof(value) == sizeof(uint32_t))
        {
            return stream.write(&value, sizeof(value));
        }
        else
        {
            const auto tmp = uint32_t(value);
            return stream.write(&tmp, sizeof(tmp));
        }
    }
    else if constexpr (type == scalar_encoder::i64)
    {
        if constexpr (sizeof(value) == sizeof(uint64_t))
        {
            return stream.write(&value, sizeof(value));
        }
        else
        {
            const auto tmp = uint64_t(value);
            return stream.write(&tmp, sizeof(tmp));
        }
    }
}

template <serialize_mode mode>
void serialize_packed(auto &stream, const spb::detail::proto_label_repeated_fixed_size auto &container)
{
    static_assert(is_packed(mode.encoder), "repeated field with fixed size has to have attribute 'packed'");

    using ValueType = typename std::remove_cvref_t<decltype(container)>::value_type;

    for (size_t i = 0; i < container.size(); i++)
    {
        if constexpr (std::is_same_v<ValueType, bool>)
            serialize<mode>(stream, bool(container[i]));
        else
            serialize<mode>(stream, container[i]);
    }
}

template <serialize_mode mode>
void serialize(auto &stream, const spb::detail::proto_label_repeated_fixed_size auto &container)
{
    serialize_packed<mode>(stream, container);
}

template <serialize_mode mode>
void serialize_packed(auto &stream, const spb::detail::proto_label_repeated auto &container)
{
    static_assert(is_packed(mode.encoder), "repeated field has to have attribute 'packed'");

    using ValueType = typename std::remove_cvref_t<decltype(container)>::value_type;

    for (const auto &v : container)
    {
        if constexpr (std::is_same_v<ValueType, bool>)
            serialize<mode>(stream, bool(v));
        else
            serialize<mode>(stream, v);
    }
}

template <serialize_mode mode>
void serialize(auto &stream, const spb::detail::proto_label_repeated auto &container)
{
    serialize_packed<mode>(stream, container);
}

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_field_string auto &value)
{
    using stream_type = std::remove_cvref_t<decltype(stream)>;

    if constexpr (!stream_type::size_only && mode.max_size)
        check_size(value.size(), mode.max_size);

    if constexpr (!stream_type::size_only)
        spb::detail::utf8::validate(std::string_view(value.data(), value.size()));

    serialize_tag(stream, field, wire_type::length_delimited);
    serialize_varint(stream, value.size());
    stream.write(value.data(), value.size());
}

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_field_bytes auto &value)
{
    using stream_type = std::remove_cvref_t<decltype(stream)>;

    if constexpr (!stream_type::size_only && mode.max_size)
        check_size(value.size(), mode.max_size);

    serialize_tag(stream, field, wire_type::length_delimited);
    serialize_varint(stream, value.size());
    stream.write(value.data(), value.size());
}

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_map auto &value)
{
    constexpr auto key_encoder   = serialize_mode{.encoder = mode.encoder};
    constexpr auto value_encoder = serialize_mode{.encoder = mode.encoder2};

    for (const auto &[k, v] : value)
    {
        serialize_tag(stream, field, wire_type::length_delimited);
        serialize_varint(stream, serialize_size<key_encoder>(1, k) + serialize_size<value_encoder>(2, v));
        serialize<key_encoder>(stream, 1, k);
        serialize<value_encoder>(stream, 2, v);
    }
}

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_label_repeated auto &container)
{
    using value_type  = typename std::remove_cvref_t<decltype(container)>::value_type;
    using stream_type = std::remove_cvref_t<decltype(stream)>;

    if constexpr (!stream_type::size_only && mode.max_count)
        check_size(container.size(), mode.max_count);

    if constexpr (is_packed(mode.encoder))
    {
        if (container.empty())
            return;

        serialize_tag(stream, field, wire_type::length_delimited);
        serialize_varint(stream, serialize_size<mode>(container));
        serialize_packed<mode>(stream, container);
    }
    else
    {
        for (const auto &value : container)
        {
            if constexpr (std::is_same_v<value_type, bool>)
                serialize<mode>(stream, field, bool(value));
            else
                serialize<mode>(stream, field, value);
        }
    }
}

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field,
               const spb::detail::proto_label_repeated_fixed_size auto &container)
{
    static_assert(is_packed(mode.encoder), "repeated field with fixed size has to have attribute 'packed'");

    serialize_tag(stream, field, wire_type::length_delimited);
    serialize_varint(stream, serialize_size<mode>(container));
    serialize_packed<mode>(stream, container);
}

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_label_optional auto &p_value)
{
    if (p_value.has_value())
        serialize<mode>(stream, field, *p_value);
}

template <serialize_mode mode, typename T>
void serialize(auto &stream, uint32_t field, const std::unique_ptr<T> &p_value)
{
    if (p_value)
        serialize<mode>(stream, field, *p_value);
}

template <serialize_mode mode>
void serialize(auto &stream, uint32_t field, const spb::detail::proto_message auto &value)
{
    serialize_tag(stream, field, wire_type::length_delimited);
    serialize_varint(stream, serialize_size<mode>(value));
    serialize_value(stream, value);
}

template <serialize_mode mode> auto serialize_size(const auto &value) -> size_t
{
    auto stream = ostream_size();
    serialize<mode>(stream, value);
    return stream.size;
}

template <serialize_mode mode> auto serialize_size(uint32_t field, const auto &value) -> size_t
{
    auto stream = ostream_size();
    serialize<mode>(stream, field, value);
    return stream.size;
}

template <serialize_mode mode> void serialize(auto &stream, const spb::detail::proto_message auto &value)
{
    serialize_value(stream, value);
}

} // namespace spb::pb::detail
