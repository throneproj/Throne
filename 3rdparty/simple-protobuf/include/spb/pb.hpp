/***************************************************************************\
* Name        : Public API for protobuf                                     *
* Description : all protobuf serialize and deserialize functions            *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/
#pragma once

#include "concepts.h"
#include "pb/deserialize.hpp"
#include "pb/serialize.hpp"
#include "spb/io/io.hpp"
#include <cstdlib>

namespace spb::pb
{

struct serialize_options
{
    /**
     * @brief Writes the size of the message (as a varint) before the message itself.
     *        Compatible with Google's `writeDelimitedTo` and NanoPb's PB_ENCODE_DELIMITED.
     */
    bool delimited = false;
};

struct deserialize_options
{
    /**
     * @brief Expect the size of the message (encoded as a varint) to come before the message
     * itself. Compatible with Google's `parseDelimitedFrom` and NanoPb's PB_DECODE_DELIMITED. Will
     * return after having read the specified length; the spb::io::reader object can then be read
     * from again to get the next message (if any).
     */
    bool delimited = false;
};

/**
 * @brief serialize message via writer
 *
 * @param[in] message to be serialized
 * @param[in] on_write function for handling the writes
 * @param[in] options
 * @return serialized size in bytes
 * @throws exceptions only from `on_write`
 */
static inline auto serialize( const auto & message, spb::io::writer on_write,
                              const serialize_options & options = { } ) -> size_t
{
    auto stream = detail::ostream{ on_write };
    if( options.delimited )
    {
        detail::serialize_varint( stream, detail::serialize_size( message ) );
    }
    serialize( stream, message );
    return stream.size( );
}

/**
 * @brief return protobuf serialized size in bytes
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized size in bytes
 */
[[nodiscard]] static inline auto serialize_size( const auto & message,
                                                 const serialize_options & options = { } ) -> size_t
{
    return serialize( message, spb::io::writer( nullptr ), options );
}

/**
 * @brief serialize message into protobuf
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @param[out] result serialized protobuf
 * @return serialized size in bytes
 * @throws std::runtime_error on error
 * @example `auto serialized = std::vector< std::byte >();`
 *          `spb::pb::serialize( message, serialized );`
 */
template < typename Message, spb::resizable_container Container >
static inline auto serialize( const Message & message, Container & result,
                              const serialize_options & options = { } ) -> size_t
{
    const auto size = serialize_size( message, options );
    result.resize( size );
    auto writer = [ ptr = result.data( ) ]( const void * data, size_t size ) mutable
    {
        memcpy( ptr, data, size );
        ptr += size;
    };

    serialize( message, writer, options );
    return size;
}

/**
 * @brief serialize message into protobuf
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized protobuf
 * @throws std::runtime_error on error
 * @example `auto serialized_message = spb::pb::serialize< std::vector< std::byte > >( message );`
 */
template < spb::resizable_container Container = std::string, typename Message >
[[nodiscard]] static inline auto serialize( const Message & message,
                                            const serialize_options & options = { } ) -> Container
{
    auto result = Container( );
    serialize< Message, Container >( message, result, options );
    return result;
}

/**
 * @brief deserialize message from protobuf
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @param[out] message deserialized message
 * @throws std::runtime_error on error
 */
static inline void deserialize( auto & message, spb::io::reader reader,
                                const deserialize_options & options = { } )
{
    detail::istream stream{ reader };
    if( options.delimited )
    {
        const auto substream_length = read_varint< uint32_t >( stream );
        auto substream              = stream.sub_stream( substream_length );
        return deserialize_main( substream, message );
    }
    else
    {
        return deserialize_main( stream, message );
    }
}

/**
 * @brief deserialize message from protobuf
 *
 * @param[in] protobuf string with protobuf
 * @param[in] options
 * @param[out] message deserialized message
 * @throws std::runtime_error on error
 * @example `auto serialized = std::vector< std::byte >( ... );`
 *          `auto message = Message();`
 *          `spb::pb::deserialize( message, serialized );`
 */
template < typename Message, spb::size_container Container >
static inline void deserialize( Message & message, const Container & protobuf,
                                const deserialize_options & options = { } )
{
    auto reader = [ ptr = protobuf.data( ), end = protobuf.data( ) + protobuf.size( ) ](
                      void * data, size_t size ) mutable -> size_t
    {
        size_t bytes_left = end - ptr;

        size = std::min( size, bytes_left );
        memcpy( data, ptr, size );
        ptr += size;
        return size;
    };
    return deserialize( message, reader, options );
}

/**
 * @brief deserialize message from protobuf
 *
 * @param[in] protobuf serialized protobuf
 * @param[in] options
 * @return deserialized message
 * @throws std::runtime_error on error
 * @example `auto serialized = std::vector< std::byte >( ... );`
 *          `auto message = spb::pb::deserialize< Message >( serialized );`
 */
template < typename Message, spb::size_container Container >
[[nodiscard]] static inline auto deserialize( const Container & protobuf,
                                              const deserialize_options & options = { } ) -> Message
{
    auto message = Message{ };
    deserialize( message, protobuf, options );
    return message;
}

/**
 * @brief deserialize message from reader
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @return deserialized message
 * @throws std::runtime_error on error
 */
template < typename Message >
[[nodiscard]] static inline auto deserialize( spb::io::reader reader,
                                              const deserialize_options & options = { } ) -> Message
{
    auto message = Message{ };
    deserialize( message, reader, options );
    return message;
}

}// namespace spb::pb
