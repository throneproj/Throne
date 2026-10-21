/***************************************************************************\
* Name        : json dumper                                                 *
* Description : C++ template used for json de/serialization                 *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <string_view>

constexpr std::string_view json_includes_template =
    R"(#include "$"
#include <spb/json/serialize.hpp>
#include <spb/json/deserialize.hpp>
#include <system_error>

)";

constexpr std::string_view json_serialize_value_template =
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
void deserialize_value(istream_reader &stream, $ &message)
{
    return deserialize_value_gen(stream, message);
}
void deserialize_value(istream_buffer &stream, $ &message)
{
    return deserialize_value_gen(stream, message);
}
)";

constexpr std::string_view file_json_header_prototypes =
    R"(void serialize_value(ostream_size &, const $ &);
void serialize_value(ostream_writer &, const $ &);
void serialize_value(ostream_buffer &, const $ &);
void deserialize_value(istream_reader &, $ &);
void deserialize_value(istream_buffer &, $ &);
)";

constexpr std::string_view file_json_header_template =
    R"(/**
 * @brief serialize message via writer
 *
 * @param[in] message to be serialized
 * @param[in] on_write function for handling the writes
 * @param[in] options
 * @return serialized size in bytes
 * @throws exceptions only from `on_write`
 */
size_t serialize(const auto &message, spb::io::writer on_write);

/**
 * @brief return JSON serialized size in bytes
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized size in bytes
 */
[[nodiscard]] size_t serialize_size(const auto &message);

size_t serialize(const auto &message, void *buffer);

/**
 * @brief serialize message into JSON
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @param[out] result serialized JSON
 * @return serialized size in bytes
 * @throws std::runtime_error on error
 * @example `std::string json;`
 *          `spb::json::serialize(message, json);`
 */
template <spb::resizable_container Container> size_t serialize(const auto &message, Container &result);

/**
 * @brief serialize message into JSON
 *
 * @param[in] message to be serialized
 * @param[in] options
 * @return serialized JSON
 * @throws std::runtime_error on error
 * @example `auto serialized_message = spb::json::serialize< std::vector< std::byte > >( message );`
 */
template <spb::resizable_container Container>
[[nodiscard]] Container serialize(const auto &message);

size_t deserialize(auto &message, const void *buffer, size_t size);

/**
 * @brief deserialize message from JSON
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @param[out] message deserialized message
 * @throws std::runtime_error on error
 */
size_t deserialize(auto &message, spb::io::reader reader);

/**
 * @brief deserialize message from JSON
 *
 * @param[in] JSON string with JSON
 * @param[in] options
 * @param[out] message deserialized message
 * @throws std::runtime_error on error
 * @example `auto serialized = std::vector<std::byte>( ... );`
 *          `auto message = Message();`
 *          `spb::json::deserialize(message, serialized);`
 */
size_t deserialize(auto &message, const spb::size_container auto &json);

/**
 * @brief deserialize message from JSON
 *
 * @param[in] JSON serialized JSON
 * @param[in] options
 * @return deserialized message
 * @throws std::runtime_error on error
 * @example `auto serialized = std::vector<std::byte>( ... );`
 *          `auto message = spb::json::deserialize<Message>(serialized);`
 */
template <typename Message>
[[nodiscard]] Message deserialize(const spb::size_container auto &json);

/**
 * @brief deserialize message from reader
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @return deserialized message
 * @throws std::runtime_error on error
 */
template <typename Message>
[[nodiscard]] Message deserialize(spb::io::reader reader);

)";
