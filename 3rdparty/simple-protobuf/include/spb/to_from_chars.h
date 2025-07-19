/***************************************************************************\
* Name        : std::to/from_chars emulation, #if __cpp_lib_to_chars < 201611L *
* Description : string to number conversion, (std::from_chars or strtoX)    *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once
#include <charconv>
#include <string>

#if __cpp_lib_to_chars >= 201611L
namespace spb_std_emu = std;
#else
#include <algorithm>
#include <cassert>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <system_error>
#include <type_traits>

namespace spb::std_emu
{
struct from_chars_result
{
    const char * ptr;
    std::errc ec;
};

struct to_chars_result
{
    const char * ptr;
    std::errc ec;
};

static inline auto from_chars( const char * first, const char * last, std::integral auto & number,
                               int base = 10 ) -> from_chars_result
{
    assert( first <= last );
    char buffer[ 32 ];
    auto copy_size = std::min< size_t >( last - first, sizeof( buffer ) - 1 );
    memcpy( buffer, first, copy_size );
    buffer[ copy_size ] = '\0';
    const char * end    = nullptr;

    using T = std::decay_t< decltype( number ) >;

    static_assert( sizeof( number ) >= 1 && sizeof( number ) <= 8, "unsupported size" );
    errno       = 0;
    auto result = T( 0 );

    if constexpr( std::is_signed_v< T > )
    {
        if constexpr( sizeof( number ) < 4 )
        {
            auto tmp = strtol( buffer, ( char ** ) &end, base );
            if( tmp <= std::numeric_limits< T >::max( ) && tmp >= std::numeric_limits< T >::min( ) )
                [[likely]]
            {
                result = T( tmp );
            }
            else
            {
                errno = ERANGE;
            }
        }
        else if constexpr( sizeof( number ) == 4 )
        {
            result = strtol( buffer, ( char ** ) &end, base );
        }
        else if constexpr( sizeof( number ) == 8 )
        {
            result = strtoll( buffer, ( char ** ) &end, base );
        }
    }
    else
    {
        if constexpr( sizeof( number ) < 4 )
        {
            auto tmp = strtoul( buffer, ( char ** ) &end, base );
            if( tmp <= std::numeric_limits< T >::max( ) ) [[likely]]
            {
                result = T( tmp );
            }
            else
            {
                errno = ERANGE;
            }
        }
        else if constexpr( sizeof( number ) == 4 )
        {
            result = strtoul( buffer, ( char ** ) &end, base );
        }
        else if constexpr( sizeof( number ) == 8 )
        {
            result = strtoull( buffer, ( char ** ) &end, base );
        }
    }
    end = first + ( end - buffer );
    if( end == first )
    {
        return { first, std::errc::invalid_argument };
    }
    if( errno == ERANGE )
    {
        return { end, std::errc::result_out_of_range };
    }
    number = result;
    return { end, std::errc( ) };
}

static inline auto from_chars( const char * first, const char * last,
                               std::floating_point auto & number ) -> from_chars_result
{
    assert( first <= last );
    char buffer[ 256 ];
    auto copy_size = std::min< size_t >( last - first, sizeof( buffer ) - 1 );
    memcpy( buffer, first, copy_size );
    buffer[ copy_size ] = '\0';
    const char * end    = nullptr;

    static_assert( sizeof( number ) == 4 || sizeof( number ) == 8, "unsupported size" );
    errno       = 0;
    auto result = std::decay_t< decltype( number ) >{ 0 };
    if constexpr( sizeof( number ) == 4 )
    {
        result = strtof( buffer, ( char ** ) &end );
    }
    else if constexpr( sizeof( number ) == 8 )
    {
        result = strtod( buffer, ( char ** ) &end );
    }
    end = first + ( end - buffer );
    if( end == first )
    {
        return { end, std::errc::invalid_argument };
    }
    if( errno == ERANGE )
    {
        return { end, std::errc::result_out_of_range };
    }
    number = result;
    return { end, std::errc( ) };
}

static inline auto to_chars( char * first, char * last, const std::integral auto & number,
                             int base = 10 ) -> to_chars_result
{
    if( base != 10 )
    {
        return { first, std::errc::invalid_argument };
    }

    if( last <= first )
    {
        return { first, std::errc::value_too_large };
    }

    const auto result      = std::to_string( number );
    const auto buffer_size = static_cast< size_t >( last - first );

    if( result.size( ) > buffer_size )
    {
        return { first, std::errc::value_too_large };
    }

    memcpy( first, result.data( ), result.size( ) );
    return { first + result.size( ), std::errc{} };
}

static inline auto to_chars( char * first, char * last, const std::floating_point auto & number )
    -> to_chars_result
{
    if( last <= first )
    {
        return { first, std::errc::value_too_large };
    }

    const auto buffer_size = last - first;

    const char * format;

    if constexpr( std::is_same_v< std::remove_cvref_t< decltype( number ) >, double > )
    {
        format = "%lg";
    }
    else if constexpr( std::is_same_v< std::remove_cvref_t< decltype( number ) >, long double > )
    {
        format = "%Lg";
    }
    else
    {
        format = "%g";
    }

    const int written = snprintf( first, buffer_size, format, number );
    if( written < 0 )
    {
        return { first, std::errc::invalid_argument };
    }
    else if( written > buffer_size )
    {
        return { first, std::errc::value_too_large };
    }

    return { first + written, std::errc{} };
}

}// namespace spb::std_emu
namespace spb_std_emu = spb::std_emu;
#endif
