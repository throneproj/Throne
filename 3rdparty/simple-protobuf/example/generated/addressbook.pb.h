#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <spb/json.hpp>
#include <spb/pb.hpp>
#include <string>
#include <vector>

namespace tutorial
{
struct Person
{
    enum class PhoneType : int32_t
    {
        PHONE_TYPE_UNSPECIFIED = 0,
        PHONE_TYPE_MOBILE      = 1,
        PHONE_TYPE_HOME        = 2,
        PHONE_TYPE_WORK        = 3,
    };
    struct PhoneNumber
    {
        std::optional<std::string> number;
        std::optional<PhoneType> type = PhoneType::PHONE_TYPE_HOME;
    };
    std::optional<std::string> name;
    std::optional<int32_t> id;
    std::optional<std::string> email;
    std::vector<PhoneNumber> phones;
};
struct AddressBook
{
    std::vector<Person> people;
};
} // namespace tutorial

namespace spb::pb
{

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
 *        Warning: User is responsible for allocating buffers size (use `auto buffer_size =
 * serialize_size(message)`)
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

namespace detail
{
void serialize_value(ostream_size &, const ::tutorial::Person &message);
void serialize_value(ostream_writer &, const ::tutorial::Person &message);
void serialize_value(ostream_buffer &, const ::tutorial::Person &message);
void deserialize_value(istream_reader &, ::tutorial::Person &message, tag_type);
void deserialize_value(istream_buffer &, ::tutorial::Person &message, tag_type);
void serialize_value(ostream_size &, const ::tutorial::AddressBook &message);
void serialize_value(ostream_writer &, const ::tutorial::AddressBook &message);
void serialize_value(ostream_buffer &, const ::tutorial::AddressBook &message);
void deserialize_value(istream_reader &, ::tutorial::AddressBook &message, tag_type);
void deserialize_value(istream_buffer &, ::tutorial::AddressBook &message, tag_type);
void serialize_value(ostream_size &, const ::tutorial::Person::PhoneNumber &message);
void serialize_value(ostream_writer &, const ::tutorial::Person::PhoneNumber &message);
void serialize_value(ostream_buffer &, const ::tutorial::Person::PhoneNumber &message);
void deserialize_value(istream_reader &, ::tutorial::Person::PhoneNumber &message, tag_type);
void deserialize_value(istream_buffer &, ::tutorial::Person::PhoneNumber &message, tag_type);
} // namespace detail
} // namespace spb::pb
namespace spb::json
{
/**
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
template <spb::resizable_container Container> [[nodiscard]] Container serialize(const auto &message);

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
template <typename Message> [[nodiscard]] Message deserialize(const spb::size_container auto &json);

/**
 * @brief deserialize message from reader
 *
 * @param[in] reader function for handling reads
 * @param[in] options
 * @return deserialized message
 * @throws std::runtime_error on error
 */
template <typename Message> [[nodiscard]] Message deserialize(spb::io::reader reader);
namespace detail
{
void serialize_value(ostream_size &, const ::tutorial::Person &message);
void serialize_value(ostream_writer &, const ::tutorial::Person &message);
void serialize_value(ostream_buffer &, const ::tutorial::Person &message);
void deserialize_value(istream_reader &, ::tutorial::Person &message);
void deserialize_value(istream_buffer &, ::tutorial::Person &message);
void serialize_value(ostream_size &, const ::tutorial::AddressBook &message);
void serialize_value(ostream_writer &, const ::tutorial::AddressBook &message);
void serialize_value(ostream_buffer &, const ::tutorial::AddressBook &message);
void deserialize_value(istream_reader &, ::tutorial::AddressBook &message);
void deserialize_value(istream_buffer &, ::tutorial::AddressBook &message);
void serialize_value(ostream_size &, const ::tutorial::Person::PhoneNumber &message);
void serialize_value(ostream_writer &, const ::tutorial::Person::PhoneNumber &message);
void serialize_value(ostream_buffer &, const ::tutorial::Person::PhoneNumber &message);
void deserialize_value(istream_reader &, ::tutorial::Person::PhoneNumber &message);
void deserialize_value(istream_buffer &, ::tutorial::Person::PhoneNumber &message);
void serialize_value(ostream_size &, const ::tutorial::Person::PhoneType &message);
void serialize_value(ostream_writer &, const ::tutorial::Person::PhoneType &message);
void serialize_value(ostream_buffer &, const ::tutorial::Person::PhoneType &message);
void deserialize_value(istream_reader &, ::tutorial::Person::PhoneType &message);
void deserialize_value(istream_buffer &, ::tutorial::Person::PhoneType &message);
} // namespace detail

} // namespace spb::json
