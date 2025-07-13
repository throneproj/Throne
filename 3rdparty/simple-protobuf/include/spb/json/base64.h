/***************************************************************************\
* Name        : base64 library for json                                     *
* Description : RFC 4648 base64 decoder and encoder                         *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "../concepts.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace spb::json::detail
{
template < typename ostream >
static inline void base64_encode( ostream & output, std::span< const std::byte > input )
{
    static constexpr char encode_table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const auto * p_char = reinterpret_cast< const uint8_t * >( input.data( ) );
    //
    //- +3 means 3 bytes are being processed in one iteration (3 * 8 = 24 bits)
    //
    for( size_t idx = 3; idx <= input.size( ); idx += 3 )
    {
        auto temp = uint32_t( *p_char++ ) << 16U;
        temp += uint32_t( *p_char++ ) << 8U;
        temp += ( *p_char++ );
        output.write( encode_table[ ( temp & 0x00FC0000U ) >> 18U ] );
        output.write( encode_table[ ( temp & 0x0003F000U ) >> 12U ] );
        output.write( encode_table[ ( temp & 0x00000FC0U ) >> 6U ] );
        output.write( encode_table[ ( temp & 0x0000003FU ) ] );
    }
    switch( input.size( ) % 3 )
    {
    case 1:
    {
        auto temp = uint32_t( *p_char++ ) << 16U;
        output.write( encode_table[ ( temp & 0x00FC0000U ) >> 18U ] );
        output.write( encode_table[ ( temp & 0x0003F000U ) >> 12U ] );
        output.write( '=' );
        output.write( '=' );
    }
    break;
    case 2:
    {
        auto temp = uint32_t( *p_char++ ) << 16U;
        temp += uint32_t( *p_char++ ) << 8U;
        output.write( encode_table[ ( temp & 0x00FC0000 ) >> 18 ] );
        output.write( encode_table[ ( temp & 0x0003F000 ) >> 12 ] );
        output.write( encode_table[ ( temp & 0x00000FC0 ) >> 6 ] );
        output.write( '=' );
    }
    break;
    }
}

template < typename istream >
static inline void base64_decode_string( spb::detail::proto_field_bytes auto & output,
                                         istream & stream )
{
    static constexpr uint8_t decode_table[ 256 ] = {
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 62,  128, 128, 128, 63,  52,  53,  54,  55,  56,  57,
        58,  59,  60,  61,  128, 128, 128, 128, 128, 128, 128, 0,   1,   2,   3,   4,   5,   6,
        7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
        25,  128, 128, 128, 128, 128, 128, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,
        37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
        128, 128, 128, 128
    };

    /*static constexpr uint8_t decode_table2[] = {
        62, 128, 128, 128, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 128, 128, 128, 128, 128, 128,
        128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 128, 128, 128, 128, 128, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
    };*/

    if constexpr( spb::detail::proto_field_bytes_resizable< decltype( output ) > )
    {
        output.clear( );
    }
    if( stream.current_char( ) != '"' ) [[unlikely]]
    {
        throw std::runtime_error( "expecting '\"'" );
    }

    stream.consume_current_char( false );
    if( stream.consume( '"' ) )
    {
        return;
    }
    auto mask = uint8_t( 0 );

    for( auto out_index = size_t( 0 );; )
    {
        auto view      = stream.view( UINT32_MAX );
        auto length    = view.find( '"' );
        auto end_found = length < view.npos;
        if( ( end_found && length % 4 != 0 ) || view.size( ) <= 4 ) [[unlikely]]
        {
            throw std::runtime_error( "invalid base64" );
        }
        length = std::min( length, view.size( ) );

        //- align to 4 bytes
        auto aligned_length = length & ~3;
        if( aligned_length > 4 ) [[likely]]
        {
            auto out_length = ( ( aligned_length - 4 ) / 4 ) * 3;
            view            = view.substr( 0, aligned_length );

            if constexpr( spb::detail::proto_field_bytes_resizable< decltype( output ) > )
            {
                output.resize( output.size( ) + out_length );
            }
            else
            {
                if( out_length > ( output.size( ) - out_index ) )
                {
                    throw std::runtime_error( "too large base64" );
                }
            }

            auto * p_out      = output.data( ) + out_index;
            const auto * p_in = reinterpret_cast< const uint8_t * >( view.data( ) );
            const auto * p_end =
                p_in + aligned_length - 4;//- exclude the last 4 chars (possible padding)

            while( p_in < p_end ) [[likely]]
            {
                uint8_t v0 = decode_table[ *p_in++ ];
                uint8_t v1 = decode_table[ *p_in++ ];
                uint8_t v2 = decode_table[ *p_in++ ];
                uint8_t v3 = decode_table[ *p_in++ ];
                mask |= ( v0 | v1 | v2 | v3 );

                *p_out++ = std::byte( ( v0 << 2 ) | ( v1 >> 4 ) );
                *p_out++ = std::byte( ( v1 << 4 ) | ( v2 >> 2 ) );
                *p_out++ = std::byte( ( v2 << 6 ) | ( v3 ) );

                out_index += 3;
            }
            auto consumed_bytes = p_in - reinterpret_cast< const uint8_t * >( view.data( ) );
            view.remove_prefix( consumed_bytes );
            stream.skip( consumed_bytes );
        }

        if( end_found )
        {
            //- handle padding
            const auto * p_in = reinterpret_cast< const uint8_t * >( view.data( ) );

            uint8_t v0 = decode_table[ *p_in++ ];
            uint8_t v1 = decode_table[ *p_in++ ];
            auto i1    = *p_in++;
            uint8_t v2 = i1 == '=' ? 0 : decode_table[ i1 ];
            auto i2    = *p_in++;
            uint8_t v3 = i2 == '=' ? 0 : decode_table[ i2 ];
            mask |= ( v0 | v1 | v2 | v3 );
            mask |= ( ( i1 == '=' ) & ( i2 != '=' ) ) ? 128 : 0;
            if( mask & 128 ) [[unlikely]]
            {
                throw std::runtime_error( "invalid base64" );
            }

            auto padding_size   = ( i1 == '=' ? 1 : 0 ) + ( i2 == '=' ? 1 : 0 );
            auto consumed_bytes = 3 - padding_size;
            //- +1 is for "
            stream.skip( 5 );
            if constexpr( spb::detail::proto_field_bytes_resizable< decltype( output ) > )
            {
                output.resize( output.size( ) + consumed_bytes );
            }
            else
            {
                if( output.size( ) != out_index + consumed_bytes )
                {
                    throw std::runtime_error( "too large base64" );
                }
            }
            auto * p_out = output.data( ) + out_index;
            if( padding_size == 0 )
            {
                *p_out++ = std::byte( ( v0 << 2 ) | ( v1 >> 4 ) );
                *p_out++ = std::byte( ( v1 << 4 ) | ( v2 >> 2 ) );
                *p_out++ = std::byte( ( v2 << 6 ) | ( v3 ) );
            }
            else if( padding_size == 1 )
            {
                *p_out++ = std::byte( ( v0 << 2 ) | ( v1 >> 4 ) );
                *p_out++ = std::byte( ( v1 << 4 ) | ( v2 >> 2 ) );
            }
            else if( padding_size == 2 )
            {
                *p_out++ = std::byte( ( v0 << 2 ) | ( v1 >> 4 ) );
            }
            return;
        }
    }
}
}// namespace spb::json::detail
