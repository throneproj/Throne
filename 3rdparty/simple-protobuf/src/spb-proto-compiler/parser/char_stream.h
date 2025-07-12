/***************************************************************************\
* Name        : input stream                                                *
* Description : char stream used for parsing proto files                    *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once
#include <algorithm>
#include <cassert>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace spb
{

struct char_stream
{
private:
    //- start of the stream
    const char * p_start = nullptr;
    //- current position in the stream
    const char * p_begin = nullptr;
    //- end of the stream
    const char * p_end = nullptr;
    //- current char
    char m_current = { };

    /**
     * @brief gets the next char from the stream
     *
     * @param skip_white_space if true, skip white spaces
     */
    void update_current( bool skip_white_space ) noexcept
    {
        while( p_begin < p_end )
        {
            m_current = *p_begin;
            if( !skip_white_space || isspace( m_current ) == 0 )
            {
                return;
            }
            p_begin += 1;
        }
        m_current = { };
    }

public:
    explicit char_stream( std::string_view content ) noexcept
        : p_start( content.data( ) )
        , p_begin( p_start )
        , p_end( p_begin + content.size( ) )
    {
        update_current( true );
    }

    [[nodiscard]] auto begin( ) const noexcept -> const char *
    {
        return p_begin;
    }

    [[nodiscard]] auto end( ) const noexcept -> const char *
    {
        return p_end;
    }

    [[nodiscard]] auto empty( ) const noexcept -> bool
    {
        return p_end <= p_begin;
    }

    [[nodiscard]] auto current_char( ) const noexcept -> char
    {
        return m_current;
    }

    /**
     * @brief consumes `current char` if its equal to c
     *
     * @param c consumed char
     * @return true if char was consumed
     */
    [[nodiscard]] auto consume( char c ) noexcept -> bool
    {
        if( auto current = current_char( ); current == c )
        {
            consume_current_char( true );
            return true;
        }
        return false;
    }

    /**
     * @brief consumes an `token`
     *
     * @param token consumed `token` (whole word)
     * @return true if `token` was consumed
     */
    auto consume( std::string_view token ) noexcept -> bool
    {
        const auto state = *this;

        if( content( ).starts_with( token ) )
        {
            p_begin += token.size( );
            update_current( false );
            auto next = current_char( );
            if( isspace( next ) || !isalnum( next ) )
            {
                update_current( true );
                return true;
            }
            *this = state;
        }
        return false;
    }

    void consume_current_char( bool skip_white_space ) noexcept
    {
        if( begin( ) < end( ) )
        {
            p_begin += 1;
            update_current( skip_white_space );
        }
    }

    /**
     * @brief trim current spaces
     *
     */
    void consume_space( ) noexcept
    {
        update_current( true );
    }

    void skip_to( const char * ptr ) noexcept
    {
        assert( ptr >= p_start && ptr <= end( ) );
        p_begin = ptr;
        update_current( true );
    }

    [[nodiscard]] auto content( ) const noexcept -> std::string_view
    {
        return { begin( ), size_t( end( ) - begin( ) ) };
    }

    [[nodiscard]] auto current_line( ) const noexcept -> size_t
    {
        return std::count( p_start, p_begin, '\n' ) + 1;
    }
    [[nodiscard]] auto current_column( ) const noexcept -> size_t
    {
        auto parsed = std::string_view( p_start, p_begin - p_start );

        if( auto p = parsed.rfind( '\n' ); p != std::string_view::npos )
        {
            parsed.remove_prefix( p );
        }
        return std::max< size_t >( parsed.size( ), 1 );
    }
    [[noreturn]] void throw_parse_error( std::string_view message )
    {
        throw std::runtime_error( std::to_string( current_line( ) ) + ":" +
                                  std::to_string( current_column( ) ) + ": " +
                                  std::string( message ) );
    }
};
}// namespace spb
