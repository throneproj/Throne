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
#include <type_traits>

namespace spb::pb::detail
{

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
enum class scalar_encoder : uint8_t
{
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

static constexpr scalar_encoder operator&( scalar_encoder lhs, scalar_encoder rhs ) noexcept
{
    return scalar_encoder( std::underlying_type_t< scalar_encoder >( lhs ) &
                           std::underlying_type_t< scalar_encoder >( rhs ) );
}

static constexpr scalar_encoder operator|( scalar_encoder lhs, scalar_encoder rhs ) noexcept
{
    return scalar_encoder( std::underlying_type_t< scalar_encoder >( lhs ) |
                           std::underlying_type_t< scalar_encoder >( rhs ) );
}

static constexpr auto scalar_encoder_combine( scalar_encoder type1, scalar_encoder type2 ) noexcept
    -> scalar_encoder
{
    return scalar_encoder( ( std::underlying_type_t< scalar_encoder >( type1 ) & 0x0f ) |
                           ( ( std::underlying_type_t< scalar_encoder >( type2 ) & 0x0f ) << 4 ) );
}

static constexpr auto scalar_encoder_is_packed( scalar_encoder a ) noexcept -> bool
{
    return ( a & scalar_encoder::packed ) == scalar_encoder::packed;
}

static constexpr auto scalar_encoder_type1( scalar_encoder a ) noexcept -> scalar_encoder
{
    return scalar_encoder( static_cast< std::underlying_type_t< scalar_encoder > >( a ) & 0x07 );
}

static constexpr auto scalar_encoder_type2( scalar_encoder a ) noexcept -> scalar_encoder
{
    return scalar_encoder( ( static_cast< std::underlying_type_t< scalar_encoder > >( a ) >> 4 ) &
                           0x07 );
}

static constexpr auto wire_type_from_scalar_encoder( scalar_encoder a ) noexcept -> wire_type
{
    switch( scalar_encoder_type1( a ) )
    {
    case scalar_encoder::i32:
        return wire_type::fixed32;
    case scalar_encoder::i64:
        return wire_type::fixed64;
    default:
        return wire_type::varint;
    }
}

}// namespace spb::pb::detail
