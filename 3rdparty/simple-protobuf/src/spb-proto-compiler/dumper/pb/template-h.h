/***************************************************************************\
* Name        : pb dumper                                                   *
* Description : C++ template used for pb de/serialization                   *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <string_view>

constexpr std::string_view pb_includes_template =
    R"(#include "$"
#include <spb/pb/wire-types.h>
#include <spb/pb.hpp>
#include <spb/pb/deserialize.hpp>
#include <spb/pb/serialize.hpp>
#include <system_error>

)";

constexpr std::string_view pb_serialize_value_template =
    R"(void serialize_value(ostream_size &stream, const $ &message)
{
    return serialize_value_gen(stream, message);
}
void serialize_value(ostream_writer &stream, const $ &message)
{
    return serialize_value_gen(stream, message);
}
void serialize_value(ostream_buffer &stream, const $ &message)
{
    return serialize_value_gen(stream, message);
}
void deserialize_value(istream_reader &stream, $ &message, tag_type tag)
{
    return deserialize_value_gen(stream, message, tag);
}
void deserialize_value(istream_buffer &stream, $ &message, tag_type tag)
{
    return deserialize_value_gen(stream, message, tag);
}
)";

constexpr std::string_view file_pb_header_enum_prototypes =
    R"(template <$> void check_enum_value_or_throw(enum_type);
)";

constexpr std::string_view file_pb_header_prototypes =
    R"(void serialize_value(ostream_size &, const $ &);
void serialize_value(ostream_writer &, const $ &);
void serialize_value(ostream_buffer &, const $ &);
void deserialize_value(istream_reader &, $ &, tag_type);
void deserialize_value(istream_buffer &, $ &, tag_type);
)";

constexpr std::string_view file_pb_header_template = R"(
/**
 * @brief serialize message via writer
 *
 * @param[in] message to be serialized
 * @param[in] on_write function for handling the writes
 * @param[in] options
 * @return serialized size in bytes
 * @throws exceptions only from `on_write`
 */
auto serialize(const auto &message, spb::io::writer on_write, const serialize_options &options) -> size_t;

/**
 * @brief return protobuf serialized size in bytes
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized size in bytes
 */
auto serialize_size(const auto &message, const serialize_options &options) -> size_t;

/**
 * @brief serialize message into a buffer
 *        Warning: User is responsible for allocating buffers size (use `auto buffer_size = serialize_size(message)`)
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized size in bytes
 */
auto serialize(const auto &message, void *buffer, const serialize_options &options) -> size_t;

/**
 * @brief serialize message into protobuf
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @param[out] result serialized message
 * @return serialized size in bytes
 * @throws std::runtime_error on error
 * @example `auto serialized = std::vector<std::byte>();`
 *          `spb::pb::serialize(message, serialized);`
 */
template <spb::resizable_container Container>
auto serialize(const auto &message, Container &result, const serialize_options &options) -> size_t;

/**
 * @brief serialize message into protobuf
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized message
 * @throws std::runtime_error on error
 * @example `auto serialized_message = spb::pb::serialize(message);`
 */
template <spb::resizable_container Container>
auto serialize(const auto &message, const serialize_options &options) -> Container;

/**
 * @brief deserialize message from protobuf
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @param[out] message deserialized message
 * @throws std::runtime_error on error
 */
auto deserialize(auto &message, spb::io::reader reader, const deserialize_options &options) -> size_t;

/**
 * @brief deserialize message from protobuf
 *
 * @param[in] protobuf string with protobuf
 * @param[in] options
 * @param[out] message deserialized message
 * @throws std::runtime_error on error
 * @example `auto serialized = std::string( ... );`
 *          `auto message = Message();`
 *          `spb::pb::deserialize(message, serialized);`
 */
template <typename Message, spb::size_container Container>
auto deserialize(Message &message, const Container &protobuf, const deserialize_options &options) -> size_t;

/**
 * @brief deserialize message from protobuf
 *
 * @param[in] protobuf serialized protobuf
 * @param[in] options
 * @return deserialized message
 * @throws std::runtime_error on error
 * @example `auto serialized = std::string( ... );`
 *          `auto message = spb::pb::deserialize<Message>(serialized);`
 */
template <typename Message, spb::size_container Container>
auto deserialize(const Container &protobuf, const deserialize_options &options) -> Message;

/**
 * @brief deserialize message from reader
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @return deserialized message
 * @throws std::runtime_error on error
 */
template <typename Message>
auto deserialize(spb::io::reader reader, const deserialize_options &options) -> Message;

)";
