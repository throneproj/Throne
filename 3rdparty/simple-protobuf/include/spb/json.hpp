/***************************************************************************\
* Name        : Public API for JSON                                         *
* Description : all json serialize and deserialize functions                *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/
#pragma once

#include "spb/io/io.hpp"
#include "json/deserialize.hpp"
#include "json/serialize.hpp"
#include <cstdlib>

namespace spb::json
{

/**
 * @brief serialize message via writer
 *
 * @param message to be serialized
 * @param on_write function for handling the writes
 * @return serialized size in bytes
 * @throws exceptions only from `on_write`
 */
static inline auto serialize( const auto & message, spb::io::writer on_write ) -> size_t
{
    return detail::serialize( message, on_write );
}

/**
 * @brief return json-string serialized size in bytes
 *
 * @param message to be serialized
 * @return serialized size in bytes
 */
[[nodiscard]] static inline auto serialize_size( const auto & message ) -> size_t
{
    return serialize( message, spb::io::writer( nullptr ) );
}

/**
 * @brief serialize message into json-string
 *
 * @param message to be serialized
 * @return serialized json
 * @throws std::runtime_error on error
 * @example `auto serialized = std::vector< std::byte >();`
 *          `spb::json::serialize( message, serialized );`
 */
template < typename Message, spb::resizable_container Container >
static inline auto serialize( const Message & message, Container & result ) -> size_t
{
    const auto size = serialize_size( message );
    result.resize( size );
    auto writer = [ ptr = result.data( ) ]( const void * data, size_t size ) mutable
    {
        memcpy( ptr, data, size );
        ptr += size;
    };

    serialize( message, writer );
    return size;
}

/**
 * @brief serialize message into json
 *
 * @param[in] message to be serialized
 * @return serialized json
 * @throws std::runtime_error on error
 * @example `auto serialized_message = spb::json::serialize( message );`
 */
template < spb::resizable_container Container = std::string, typename Message >
[[nodiscard]] static inline auto serialize( const Message & message ) -> Container
{
    auto result = Container( );
    serialize< Message, Container >( message, result );
    return result;
}

/**
 * @brief deserialize json-string into variable
 *
 * @param on_read function for handling reads
 * @param result deserialized json
 * @throws std::runtime_error on error
 */
static inline void deserialize( auto & result, spb::io::reader on_read )
{
    return detail::deserialize( result, on_read );
}

/**
 * @brief deserialize json-string into variable
 *
 * @param json string with json
 * @param message deserialized json
 * @throws std::runtime_error on error
 * @example `auto serialized = std::string( ... );`
 *          `auto message = Message();`
 *          `spb::json::deserialize( message, serialized );`
 */
template < typename Message, spb::size_container Container >
static inline void deserialize( Message & message, const Container & json )
{
    auto reader = [ ptr = json.data( ), end = json.data( ) + json.size( ) ](
                      void * data, size_t size ) mutable -> size_t
    {
        size_t bytes_left = end - ptr;

        size = std::min( size, bytes_left );
        memcpy( data, ptr, size );
        ptr += size;
        return size;
    };
    return deserialize( message, reader );
}

/**
 * @brief deserialize json-string into variable
 *
 * @param json string with json
 * @return deserialized json or throw an exception
 * @example `auto serialized = std::string( ... );`
 *          `auto message = spb::json::deserialize< Message >( serialized );`
 */
template < typename Message, spb::size_container Container >
[[nodiscard]] static inline auto deserialize( const Container & json ) -> Message
{
    auto message = Message{ };
    deserialize( message, json );
    return message;
}

/**
 * @brief deserialize json-string into variable
 *
 * @param on_read function for handling reads
 * @return deserialized json
 * @throws std::runtime_error on error
 * @example `auto message = spb::json::deserialize< Message >( reader )`
 */
template < typename Message >
[[nodiscard]] static inline auto deserialize( spb::io::reader on_read ) -> Message
{
    auto message = Message{ };
    return deserialize( message, on_read );
}

}// namespace spb::json
