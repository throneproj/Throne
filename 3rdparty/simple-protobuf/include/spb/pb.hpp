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
#include "spb/pb/wire-types.h"
#include <cstdint>
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
size_t serialize(const auto &message, spb::io::writer on_write, const serialize_options &options = {})
{
    auto stream = detail::ostream_writer{on_write};
    if (options.delimited)
        detail::serialize_varint(stream, detail::serialize_size(message));

    serialize_value(stream, message);
    return stream.size;
}

/**
 * @brief return protobuf serialized size in bytes
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized size in bytes
 */
[[nodiscard]] size_t serialize_size(const auto &message, const serialize_options &options = {})
{
    const auto size = detail::serialize_size(message);
    return (options.delimited) ? size + detail::serialize_varint_size(size) : size;
}

size_t serialize(const auto &message, void *buffer, const serialize_options &options = {})
{
    const auto start = (uint8_t *)buffer;
    auto stream      = detail::ostream_buffer((uint8_t *)buffer);
    if (options.delimited)
        detail::serialize_varint(stream, detail::serialize_size(message));

    serialize_value(stream, message);
    return stream.p_buffer - start;
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
template <spb::resizable_container Container>
size_t serialize(const auto &message, Container &result, const serialize_options &options = {})
{
    static_assert(sizeof(*result.data()) == sizeof(uint8_t));

    const auto size            = detail::serialize_size(message);
    const auto serialized_size = options.delimited ? size + detail::serialize_varint_size(size) : size;
    result.resize(serialized_size);
    auto stream = detail::ostream_buffer((uint8_t *)result.data());
    if (options.delimited)
        detail::serialize_varint(stream, size);

    serialize_value(stream, message);
    return result.size();
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
template <spb::resizable_container Container = std::string>
[[nodiscard]] Container serialize(const auto &message, const serialize_options &options = {})
{
    auto result = Container();
    serialize(message, result, options);
    return result;
}

size_t deserialize(auto &message, const void *buffer, size_t size, const deserialize_options &options = {})
{
    detail::istream_buffer stream((const uint8_t *)buffer, size);
    if (options.delimited)
    {
        const auto substream_length = read_varint<uint32_t>(stream);
        auto substream              = stream.sub_stream(substream_length);
        deserialize<detail::serialize_mode{}>(substream, message);
    }
    else
    {
        deserialize<detail::serialize_mode{}>(stream, message);
    }
    return size - stream.size();
}

/**
 * @brief deserialize message from protobuf
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @param[out] message deserialized message
 * @throws std::runtime_error on error
 */
size_t deserialize(auto &message, spb::io::reader reader, const deserialize_options &options = {})
{
    detail::istream_reader stream{reader};
    if (options.delimited)
    {
        const auto substream_length = read_varint<uint32_t>(stream);
        auto substream              = stream.sub_stream(substream_length);
        deserialize<detail::serialize_mode{}>(substream, message);
    }
    else
    {
        deserialize<detail::serialize_mode{}>(stream, message);
    }
    return stream.consumed_size();
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
template <typename Message, spb::size_container Container>
size_t deserialize(Message &message, const Container &protobuf, const deserialize_options &options = {})
{
    return deserialize(message, protobuf.data(), protobuf.size(), options);
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
template <typename Message, spb::size_container Container>
[[nodiscard]] Message deserialize(const Container &protobuf, const deserialize_options &options = {})
{
    auto message = Message{};
    deserialize(message, protobuf.data(), protobuf.size(), options);
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
template <typename Message>
[[nodiscard]] Message deserialize(spb::io::reader reader, const deserialize_options &options = {})
{
    auto message = Message{};
    deserialize(message, reader, options);
    return message;
}
} // namespace spb::pb
