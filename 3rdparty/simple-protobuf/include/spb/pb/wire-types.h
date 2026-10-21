/***************************************************************************\
* Name        : serialize library for protobuf                              *
* Description : encoding in protobuf                                        *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <cstdint>
#include <stdexcept>

namespace spb::pb::detail
{
using enum_type = int32_t;

enum class tag_type : uint32_t
{
    invalid = 0
};

//- https://protobuf.dev/programming-guides/encoding/
enum class wire_type : uint8_t
{
    //- int32, int64, uint32, uint64, sint32, sint64, bool, enum
    varint = 0,
    //- fixed64, sfixed64, double
    fixed64 = 1,
    //- string, bytes, embedded messages, packed repeated fields
    length_delimited = 2,
    //- not used
    StartGroup = 3,
    //- not used
    EndGroup = 4,
    //- fixed32, sfixed32, float
    fixed32 = 5
};

//- type1, type2 and packed flag
enum scalar_encoder : uint8_t
{
    none = 0,
    //- int32, int64, uint32, uint64, bool
    varint = 0x01,
    //- zigzag int32 or int64
    svarint = 0x02,
    //- 4 bytes
    i32 = 0x03,
    //- 8 bytes
    i64 = 0x04,
    //- packed array
    packed = 0x08
};

struct serialize_mode
{
    scalar_encoder encoder  = {};
    scalar_encoder encoder2 = {};
    size_t max_count        = 0;
    size_t max_size         = 0;
};

constexpr auto make_packed(scalar_encoder a) noexcept -> scalar_encoder
{
    return scalar_encoder(a | scalar_encoder::packed);
}

constexpr auto is_packed(scalar_encoder a) noexcept -> bool
{
    return (a & scalar_encoder::packed) == scalar_encoder::packed;
}

constexpr auto reset_packed(serialize_mode a) noexcept -> serialize_mode
{
    a.encoder = scalar_encoder(a.encoder & ~scalar_encoder::packed);
    return a;
}

constexpr auto encoder_type(scalar_encoder a) noexcept -> scalar_encoder
{
    return scalar_encoder(a & 0x07);
}

constexpr auto to_wire_type(scalar_encoder a) noexcept -> wire_type
{
    switch (encoder_type(a))
    {
    case scalar_encoder::i32:
        return wire_type::fixed32;
    case scalar_encoder::i64:
        return wire_type::fixed64;
    default:
        return wire_type::varint;
    }
}

inline void check_size(size_t size, size_t max_size)
{
    if (size > max_size) [[unlikely]]
        throw std::length_error("field is too large");
}

} // namespace spb::pb::detail
