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
#include <map>
#include <memory>
#include <spb/io/io.hpp>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace spb::pb::detail
{

struct istream
{
private:
    spb::io::reader on_read;
    size_t m_size;

public:
    istream( spb::io::reader reader, size_t size = std::numeric_limits< size_t >::max( ) ) noexcept
        : on_read( reader )
        , m_size( size )
    {
    }

    void skip( uint32_t tag );
    void read_skip( size_t size );

    void deserialize( auto & value, uint32_t tag );

    template < scalar_encoder encoder >
    void deserialize_as( auto & value, uint32_t tag );

    template < size_t ordinal, typename T >
    void deserialize_variant( T & variant, uint32_t tag );

    template < size_t ordinal, scalar_encoder encoder, typename T >
    void deserialize_variant_as( T & variant, uint32_t tag );

    template < scalar_encoder encoder, typename T >
    auto deserialize_bitfield_as( uint32_t bits, uint32_t tag ) -> T;

    [[nodiscard]] auto read_byte( ) -> uint8_t
    {
        uint8_t result = { };
        read_exact( &result, sizeof( result ) );
        return result;
    }

    [[nodiscard]] auto read_byte_or_eof( ) -> int
    {
        uint8_t result = { };
        if( on_read( &result, sizeof( result ) ) == 0 )
        {
            return -1;
        }
        return result;
    }

    [[nodiscard]] auto size( ) const -> size_t
    {
        return m_size;
    }

    void read_exact( void * data, size_t size )
    {
        if( this->size( ) < size ) [[unlikely]]
        {
            throw std::runtime_error( "unexpected end of stream" );
        }

        while( size > 0 )
        {
            auto chunk_size = on_read( data, size );
            if( chunk_size == 0 )
            {
                throw std::runtime_error( "unexpected end of stream" );
            }

            size -= chunk_size;
            m_size -= chunk_size;
        }
    }

    [[nodiscard]] auto empty( ) const -> bool
    {
        return size( ) == 0;
    }

    [[nodiscard]] auto sub_stream( size_t sub_size ) -> istream
    {
        if( size( ) < sub_size )
        {
            throw std::runtime_error( "unexpected end of stream" );
        }
        m_size -= sub_size;
        return istream( on_read, sub_size );
    }
};

[[nodiscard]] static inline auto wire_type_from_tag( uint32_t tag ) -> wire_type
{
    return wire_type( tag & 0x07 );
}

[[nodiscard]] static inline auto field_from_tag( uint32_t tag ) -> uint32_t
{
    return tag >> 3;
}

static inline void check_tag( uint32_t tag )
{
    if( field_from_tag( tag ) == 0 )
    {
        throw std::runtime_error( "invalid field id" );
    }
}

static inline void check_wire_type( wire_type type1, wire_type type2 )
{
    if( type1 != type2 )
    {
        throw std::runtime_error( "invalid wire type" );
    }
}

static inline void check_if_empty( istream & stream )
{
    if( !stream.empty( ) )
    {
        throw std::runtime_error( "unexpected data in stream" );
    }
}

[[nodiscard]] static inline auto read_tag_or_eof( istream & stream ) -> uint32_t
{
    auto byte_or_eof = stream.read_byte_or_eof( );
    if( byte_or_eof < 0 )
    {
        return 0;
    }
    auto byte = uint8_t( byte_or_eof );
    auto tag  = uint32_t( byte & 0x7F );

    for( size_t shift = CHAR_BIT - 1; ( byte & 0x80 ) != 0; shift += CHAR_BIT - 1 )
    {
        if( shift >= sizeof( tag ) * CHAR_BIT )
        {
            throw std::runtime_error( "invalid tag" );
        }

        byte = stream.read_byte( );
        tag |= uint64_t( byte & 0x7F ) << shift;
    }

    check_tag( tag );

    return tag;
}

template < typename T >
[[nodiscard]] static inline auto read_varint( istream & stream ) -> T
{
    if constexpr( std::is_same_v< T, bool > )
    {
        switch( stream.read_byte( ) )
        {
        case 0:
            return false;
        case 1:
            return true;
        default:
            throw std::runtime_error( "invalid varint for bool" );
        }
    }
    else
    {
        auto value = uint64_t( 0 );

        for( auto shift = 0U; shift < sizeof( value ) * CHAR_BIT; shift += CHAR_BIT - 1 )
        {
            uint8_t byte = stream.read_byte( );
            value |= uint64_t( byte & 0x7F ) << shift;
            if( ( byte & 0x80 ) == 0 )
            {
                if constexpr( std::is_signed_v< T > && sizeof( T ) < sizeof( value ) )
                {
                    //- GPB encodes signed varints always as 64-bits
                    //- so int32_t(-2) is encoded as "\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01",
                    // same as int64_t(-2)
                    //- but it should be encoded as  "\xfe\xff\xff\xff\x0f"
                    value = T( value );
                }
                auto result = T( value );
                if constexpr( std::is_signed_v< T > )
                {
                    if( result == std::make_signed_t< T >( value ) )
                    {
                        return result;
                    }
                }
                else
                {
                    if( result == value )
                    {
                        return result;
                    }
                }

                break;
            }
        }
        throw std::runtime_error( "invalid varint" );
    }
}

static inline void deserialize( istream & stream, spb::detail::proto_message auto & value,
                                wire_type type );

template < scalar_encoder encoder >
static inline void deserialize_as( istream & stream,
                                   spb::detail::proto_field_int_or_float auto & value,
                                   wire_type type );
static inline void deserialize( istream & stream, spb::detail::proto_field_bytes auto & value,
                                wire_type type );
static inline void deserialize( istream & stream, spb::detail::proto_label_repeated auto & value,
                                wire_type type );
static inline void deserialize( istream & stream, spb::detail::proto_field_string auto & value,
                                wire_type type );

template < scalar_encoder encoder, spb::detail::proto_label_repeated C >
static inline void deserialize_as( istream & stream, C & value, wire_type type );

template < scalar_encoder encoder, typename keyT, typename valueT >
static inline void deserialize_as( istream & stream, std::map< keyT, valueT > & value,
                                   wire_type type );

static inline void deserialize( istream & stream, spb::detail::proto_label_optional auto & p_value,
                                wire_type type );

template < scalar_encoder encoder, spb::detail::proto_label_optional C >
static inline void deserialize_as( istream & stream, C & p_value, wire_type type );

template < typename T >
static inline void deserialize( istream & stream, std::unique_ptr< T > & value, wire_type type );

template < typename T, typename signedT, typename unsignedT >
static auto create_tmp_var( )
{
    if constexpr( std::is_signed< T >::value )
    {
        return signedT( );
    }
    else
    {
        return unsignedT( );
    }
}

template < scalar_encoder encoder, typename T >
static inline auto deserialize_bitfield_as( istream & stream, uint32_t bits, wire_type type ) -> T
{
    auto value = T( );
    if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::svarint )
    {
        check_wire_type( type, wire_type::varint );

        auto tmp = read_varint< std::make_unsigned_t< T > >( stream );
        value    = T( ( tmp >> 1 ) ^ ( ~( tmp & 1 ) + 1 ) );
    }
    else if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::varint )
    {
        check_wire_type( type, wire_type::varint );
        value = read_varint< T >( stream );
    }
    else if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::i32 )
    {
        static_assert( sizeof( T ) <= sizeof( uint32_t ) );

        check_wire_type( type, wire_type::fixed32 );

        if constexpr( sizeof( value ) == sizeof( uint32_t ) )
        {
            stream.read_exact( &value, sizeof( value ) );
        }
        else
        {
            auto tmp = create_tmp_var< T, int32_t, uint32_t >( );
            stream.read_exact( &tmp, sizeof( tmp ) );
            spb::detail::check_if_value_fit_in_bits( tmp, bits );
            value = T( tmp );
        }
    }
    else if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::i64 )
    {
        static_assert( sizeof( T ) <= sizeof( uint64_t ) );
        check_wire_type( type, wire_type::fixed64 );

        if constexpr( sizeof( value ) == sizeof( uint64_t ) )
        {
            stream.read_exact( &value, sizeof( value ) );
        }
        else
        {
            auto tmp = create_tmp_var< T, int64_t, uint64_t >( );
            stream.read_exact( &tmp, sizeof( tmp ) );
            spb::detail::check_if_value_fit_in_bits( tmp, bits );
            value = T( tmp );
        }
    }
    spb::detail::check_if_value_fit_in_bits( value, bits );
    return value;
}

template < scalar_encoder encoder >
static inline void deserialize_as( istream & stream, spb::detail::proto_enum auto & value,
                                   wire_type type )
{
    using T        = std::remove_cvref_t< decltype( value ) >;
    using int_type = std::underlying_type_t< T >;

    if constexpr( !scalar_encoder_is_packed( encoder ) )
    {
        check_wire_type( type, wire_type::varint );
    }

    value = T( read_varint< int_type >( stream ) );
}

template < scalar_encoder encoder >
static inline void deserialize_as( istream & stream,
                                   spb::detail::proto_field_int_or_float auto & value,
                                   wire_type type )
{
    using T = std::remove_cvref_t< decltype( value ) >;

    if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::svarint )
    {
        if constexpr( !scalar_encoder_is_packed( encoder ) )
        {
            check_wire_type( type, wire_type::varint );
        }
        auto tmp = read_varint< std::make_unsigned_t< T > >( stream );
        value    = T( ( tmp >> 1 ) ^ ( ~( tmp & 1 ) + 1 ) );
    }
    else if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::varint )
    {
        if constexpr( !scalar_encoder_is_packed( encoder ) )
        {
            check_wire_type( type, wire_type::varint );
        }
        value = read_varint< T >( stream );
    }
    else if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::i32 )
    {
        static_assert( sizeof( T ) <= sizeof( uint32_t ) );

        if constexpr( !scalar_encoder_is_packed( encoder ) )
        {
            check_wire_type( type, wire_type::fixed32 );
        }
        if constexpr( sizeof( value ) == sizeof( uint32_t ) )
        {
            stream.read_exact( &value, sizeof( value ) );
        }
        else
        {
            if constexpr( std::is_signed_v< T > )
            {
                auto tmp = int32_t( 0 );
                stream.read_exact( &tmp, sizeof( tmp ) );
                if( tmp > std::numeric_limits< T >::max( ) ||
                    tmp < std::numeric_limits< T >::min( ) )
                {
                    throw std::runtime_error( "int overflow" );
                }
                value = T( tmp );
            }
            else
            {
                auto tmp = uint32_t( 0 );
                stream.read_exact( &tmp, sizeof( tmp ) );
                if( tmp > std::numeric_limits< T >::max( ) )
                {
                    throw std::runtime_error( "int overflow" );
                }
                value = T( tmp );
            }
        }
    }
    else if constexpr( scalar_encoder_type1( encoder ) == scalar_encoder::i64 )
    {
        static_assert( sizeof( T ) <= sizeof( uint64_t ) );
        if constexpr( !scalar_encoder_is_packed( encoder ) )
        {
            check_wire_type( type, wire_type::fixed64 );
        }
        if constexpr( sizeof( value ) == sizeof( uint64_t ) )
        {
            stream.read_exact( &value, sizeof( value ) );
        }
        else
        {
            if constexpr( std::is_signed_v< T > )
            {
                auto tmp = int64_t( 0 );
                stream.read_exact( &tmp, sizeof( tmp ) );
                if( tmp > std::numeric_limits< T >::max( ) ||
                    tmp < std::numeric_limits< T >::min( ) )
                {
                    throw std::runtime_error( "int overflow" );
                }
                value = T( tmp );
            }
            else
            {
                auto tmp = uint64_t( 0 );
                stream.read_exact( &tmp, sizeof( tmp ) );
                if( tmp > std::numeric_limits< T >::max( ) )
                {
                    throw std::runtime_error( "int overflow" );
                }
                value = T( tmp );
            }
        }
    }
}

static inline void deserialize( istream & stream, spb::detail::proto_label_optional auto & p_value,
                                wire_type type )
{
    auto & value = p_value.emplace( typename std::decay_t< decltype( p_value ) >::value_type( ) );
    deserialize( stream, value, type );
}

template < scalar_encoder encoder, spb::detail::proto_label_optional C >
static inline void deserialize_as( istream & stream, C & p_value, wire_type type )
{
    auto & value = p_value.emplace( typename C::value_type( ) );
    deserialize_as< encoder >( stream, value, type );
}

static inline void deserialize( istream & stream, spb::detail::proto_field_string auto & value,
                                wire_type type )
{
    check_wire_type( type, wire_type::length_delimited );
    if constexpr( spb::detail::proto_field_string_resizable< decltype( value ) > )
    {
        value.resize( stream.size( ) );
    }
    else
    {
        if( value.size( ) != stream.size( ) )
        {
            throw std::runtime_error( "invalid string size" );
        }
    }
    stream.read_exact( value.data( ), stream.size( ) );
    spb::detail::utf8::validate( std::string_view( value.data( ), value.size( ) ) );
}

template < typename T >
static inline void deserialize( istream & stream, std::unique_ptr< T > & value, wire_type type )
{
    value = std::make_unique< T >( );
    deserialize( stream, *value, type );
}

static inline void deserialize( istream & stream, spb::detail::proto_field_bytes auto & value,
                                wire_type type )
{
    check_wire_type( type, wire_type::length_delimited );
    if constexpr( spb::detail::proto_field_bytes_resizable< decltype( value ) > )
    {
        value.resize( stream.size( ) );
    }
    else
    {
        if( stream.size( ) != value.size( ) )
        {
            throw std::runtime_error( "invalid bytes size" );
        }
    }
    stream.read_exact( value.data( ), stream.size( ) );
}

static inline void deserialize( istream & stream, spb::detail::proto_label_repeated auto & value,
                                wire_type type )
{
    deserialize( stream, value.emplace_back( ), type );
}

template < scalar_encoder encoder, spb::detail::proto_label_repeated C >
static inline void deserialize_packed_as( istream & stream, C & value, wire_type type )
{
    while( !stream.empty( ) )
    {
        if constexpr( std::is_same_v< typename C::value_type, bool > )
        {
            value.emplace_back( read_varint< bool >( stream ) );
        }
        else
        {
            deserialize_as< encoder >( stream, value.emplace_back( ), type );
        }
    }
}

template < scalar_encoder encoder, spb::detail::proto_label_repeated C >
static inline void deserialize_as( istream & stream, C & value, wire_type type )
{
    if constexpr( scalar_encoder_is_packed( encoder ) )
    {
        deserialize_packed_as< encoder >( stream, value, wire_type_from_scalar_encoder( encoder ) );
    }
    else
    {
        if constexpr( std::is_same_v< typename C::value_type, bool > )
        {
            value.emplace_back( read_varint< bool >( stream ) );
        }
        else
        {
            deserialize_as< encoder >( stream, value.emplace_back( ), type );
        }
    }
}

template < scalar_encoder encoder, typename keyT, typename valueT >
static inline void deserialize_as( istream & stream, std::map< keyT, valueT > & value,
                                   wire_type type )
{
    const auto key_encoder   = scalar_encoder_type1( encoder );
    const auto value_encoder = scalar_encoder_type2( encoder );

    check_wire_type( type, wire_type::length_delimited );

    auto pair          = std::pair< keyT, valueT >( );
    auto key_defined   = false;
    auto value_defined = false;
    while( !stream.empty( ) )
    {
        const auto tag        = read_varint< uint32_t >( stream );
        const auto field      = field_from_tag( tag );
        const auto field_type = wire_type_from_tag( tag );

        switch( field )
        {
        case 1:
            if constexpr( std::is_integral_v< keyT > )
            {
                deserialize_as< key_encoder >( stream, pair.first, field_type );
            }
            else
            {
                if( field_type == wire_type::length_delimited )
                {
                    const auto size = read_varint< uint32_t >( stream );
                    auto substream  = stream.sub_stream( size );
                    deserialize( substream, pair.first, field_type );
                    check_if_empty( substream );
                }
                else
                {
                    deserialize( stream, pair.first, field_type );
                }
            }
            key_defined = true;
            break;
        case 2:
            if constexpr( spb::detail::proto_field_number< valueT > )
            {
                deserialize_as< value_encoder >( stream, pair.second, field_type );
            }
            else
            {
                if( field_type == wire_type::length_delimited )
                {
                    const auto size = read_varint< uint32_t >( stream );
                    auto substream  = stream.sub_stream( size );
                    deserialize( substream, pair.second, field_type );
                    check_if_empty( substream );
                }
                else
                {
                    throw std::runtime_error( "invalid field" );
                }
            }
            value_defined = true;
            break;
        default:
            throw std::runtime_error( "invalid field" );
        }
    }
    if( key_defined && value_defined )
    {
        value.insert( std::move( pair ) );
    }
    else
    {
        throw std::runtime_error( "invalid map item" );
    }
}

template < size_t ordinal, typename T >
static inline void deserialize_variant( istream & stream, T & variant, wire_type type )
{
    deserialize( stream, variant.template emplace< ordinal >( ), type );
}

template < size_t ordinal, scalar_encoder encoder, typename T >
static inline void deserialize_variant_as( istream & stream, T & variant, wire_type type )
{
    deserialize_as< encoder >( stream, variant.template emplace< ordinal >( ), type );
}

static inline void deserialize_main( istream & stream, spb::detail::proto_message auto & value )
{
    for( ;; )
    {
        const auto tag = read_tag_or_eof( stream );
        if( !tag )
        {
            break;
        }
        const auto field_type = wire_type_from_tag( tag );

        if( field_type == wire_type::length_delimited )
        {
            const auto size = read_varint< uint32_t >( stream );
            auto substream  = stream.sub_stream( size );
            deserialize_value( substream, value, tag );
            check_if_empty( substream );
        }
        else
        {
            deserialize_value( stream, value, tag );
        }
    }
}

static inline void deserialize( istream & stream, spb::detail::proto_message auto & value,
                                wire_type type )
{
    check_wire_type( type, wire_type::length_delimited );

    while( !stream.empty( ) )
    {
        const auto tag        = read_varint< uint32_t >( stream );
        const auto field_type = wire_type_from_tag( tag );

        if( field_type == wire_type::length_delimited )
        {
            const auto size = read_varint< uint32_t >( stream );
            auto substream  = stream.sub_stream( size );
            deserialize_value( substream, value, tag );
            check_if_empty( substream );
        }
        else
        {
            deserialize_value( stream, value, tag );
        }
    }
}

inline void istream::deserialize( auto & value, uint32_t tag )
{
    detail::deserialize( *this, value, wire_type_from_tag( tag ) );
}

inline void istream::read_skip( size_t size )
{
    uint8_t buffer[ 64 ];
    while( size > 0 )
    {
        auto chunk_size = std::min( size, sizeof( buffer ) );
        read_exact( buffer, chunk_size );
        size -= chunk_size;
    }
}

inline void istream::skip( uint32_t tag )
{
    switch( wire_type_from_tag( tag ) )
    {
    case wire_type::varint:
        return ( void ) read_varint< uint64_t >( *this );
    case wire_type::length_delimited:
        return read_skip( size( ) );
    case wire_type::fixed32:
        return read_skip( sizeof( uint32_t ) );
    case wire_type::fixed64:
        return read_skip( sizeof( uint64_t ) );
    default:
        throw std::runtime_error( "invalid wire type" );
    }
}

template < scalar_encoder encoder >
inline void istream::deserialize_as( auto & value, uint32_t tag )
{
    detail::deserialize_as< encoder >( *this, value, wire_type_from_tag( tag ) );
}

template < size_t ordinal, typename T >
inline void istream::deserialize_variant( T & variant, uint32_t tag )
{
    detail::deserialize_variant< ordinal >( *this, variant, wire_type_from_tag( tag ) );
}

template < size_t ordinal, scalar_encoder encoder, typename T >
inline void istream::deserialize_variant_as( T & variant, uint32_t tag )
{
    detail::deserialize_variant_as< ordinal, encoder >( *this, variant, wire_type_from_tag( tag ) );
}

template < scalar_encoder encoder, typename T >
inline auto istream::deserialize_bitfield_as( uint32_t bits, uint32_t tag ) -> T
{
    return detail::deserialize_bitfield_as< encoder, T >( *this, bits, wire_type_from_tag( tag ) );
}

static inline void deserialize( auto & value, spb::io::reader on_read )
{
    using T = std::remove_cvref_t< decltype( value ) >;
    static_assert( spb::detail::proto_message< T > );

    auto stream = istream( on_read );
    deserialize_main( stream, value );
}

}// namespace spb::pb::detail
