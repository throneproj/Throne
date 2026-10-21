#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <spb/json.hpp>
#include <spb/pb.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace SPB::Options
{
//- enum type: int8, uint8, int16, uint16, int32
enum class Enum8 : uint8_t
{
    MOBILE = 0,
    HOME   = 1,
    WORK   = 2,
};
enum class Enum16 : int16_t
{
    MOBILE = 0,
    HOME   = 1,
    WORK   = 2,
};
// int can also be: int8, uint8, int16, uint16
struct Integers
{
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    // alternative syntax, to avoid import "spb.proto";
    std::optional<int8_t> i8;
    std::optional<int16_t> i16;
    std::optional<int32_t> i32;
    std::optional<int64_t> i64;
    std::vector<int8_t> r8;
    std::vector<int16_t> r16;
    std::vector<int32_t> r32;
    std::vector<int64_t> r64;
};
// int bit-fields: `int8:1`, `uint8:2`, ...
struct BitFields
{
    uint8_t id_major : 4;
    uint8_t id_minor : 4;
};
// repeated fields can have maximal number of elements
// this is not directly visible in the generated pb.h
struct MaximumCount
{
    struct Person
    {
        uint32_t age;
    };
    // can be set for a single field
    std::vector<int32_t> up_to_4_int;
    std::vector<Person> up_to_5_persons;
};
// bytes and string can have maximum size
// this is not directly visible in the generated pb.h
struct MaximumSize
{
    // can be set for a single field
    std::optional<std::string> up_to_4_chars;
    std::optional<std::vector<std::byte>> up_to_5_bytes;
    std::string up_to_6_chars;
    std::vector<std::byte> up_to_6_bytes;
    // can be also combined with the `.max_count`
    std::vector<std::string> up_to_7_short_strings;
    std::vector<std::vector<std::byte>> up_to_7_short_bytes;
};
// container types for (`optional`, `repeated`, `string`, `bytes`, `map`)
// if you prefer for example boost, you can make all `optional`s in the whole
// file boost like, just uncomment the following line
// option (spb_fileopt).optional = "boost::optional<$>";
struct Containers
{
    struct Repeated
    {
        // default repeated is an std::vector<$>
        std::vector<int32_t> default_i;
        // packed repeated fields can have fixed size
        std::array<int32_t, 10> fixed_size;
    };
    struct String
    {
        struct SubStrings
        {
            std::vector<char> vec_of_chars1;
            std::optional<std::vector<char>> vec_of_chars2;
            //- an array of fixed size strings
            std::vector<std::array<char, 8>> fixed_string;
        };
        // default string is an std::string
        std::string default_string;
    };
    struct Bytes
    {
        struct SubBytes
        {
            // an array of fixed size bytes
            std::vector<std::array<std::byte, 8>> fixed_bytes;
        };
        // default bytes is an std::vector<std::byte>
        std::vector<std::byte> default_bytes;
    };
    struct Maps
    {
        // default map is an std::map<>
        std::map<int32_t, int32_t> default_map;
        // an unordered_map map
        std::unordered_map<int32_t, int32_t> my_map;
    };
    struct Optional
    {
        // if messages depends on each other, one of them
        // will became std::unique_ptr<$> to break the dependency chain
        struct CyclicDependency
        {
            struct MessageB;

            struct MessageA
            {
                std::optional<int32_t> i;
                std::unique_ptr<MessageB> b;
            };
            struct MessageC
            {
                std::optional<int32_t> i;
                std::optional<MessageA> a;
            };
            struct MessageB
            {
                std::optional<int32_t> i;
                std::optional<MessageC> c;
            };
        };
        // default optional is an std::optional<$>
        std::optional<int32_t> default_i;
    };
};
} // namespace SPB::Options

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
void serialize_value(ostream_size &, const ::SPB::Options::Integers &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Integers &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Integers &message);
void deserialize_value(istream_reader &, ::SPB::Options::Integers &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Integers &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::BitFields &message);
void serialize_value(ostream_writer &, const ::SPB::Options::BitFields &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::BitFields &message);
void deserialize_value(istream_reader &, ::SPB::Options::BitFields &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::BitFields &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::MaximumCount &message);
void serialize_value(ostream_writer &, const ::SPB::Options::MaximumCount &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::MaximumCount &message);
void deserialize_value(istream_reader &, ::SPB::Options::MaximumCount &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::MaximumCount &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::MaximumSize &message);
void serialize_value(ostream_writer &, const ::SPB::Options::MaximumSize &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::MaximumSize &message);
void deserialize_value(istream_reader &, ::SPB::Options::MaximumSize &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::MaximumSize &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::MaximumCount::Person &message);
void serialize_value(ostream_writer &, const ::SPB::Options::MaximumCount::Person &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::MaximumCount::Person &message);
void deserialize_value(istream_reader &, ::SPB::Options::MaximumCount::Person &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::MaximumCount::Person &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Repeated &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Repeated &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Repeated &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Repeated &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Repeated &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::String &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::String &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::String &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::String &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::String &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Bytes &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Bytes &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Bytes &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Bytes &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Bytes &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Maps &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Maps &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Maps &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Maps &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Maps &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Optional &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Optional &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Optional &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Optional &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Optional &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::String::SubStrings &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::String::SubStrings &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::String::SubStrings &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::String::SubStrings &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::String::SubStrings &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Bytes::SubBytes &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Bytes::SubBytes &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Bytes::SubBytes &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Bytes::SubBytes &message, tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Bytes::SubBytes &message, tag_type);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Optional::CyclicDependency &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Optional::CyclicDependency &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Optional::CyclicDependency &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Optional::CyclicDependency &message,
                       tag_type);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Optional::CyclicDependency &message,
                       tag_type);
void serialize_value(ostream_size &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void serialize_value(ostream_writer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void serialize_value(ostream_buffer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void deserialize_value(istream_reader &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message, tag_type);
void deserialize_value(istream_buffer &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message, tag_type);
void serialize_value(ostream_size &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void serialize_value(ostream_writer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void serialize_value(ostream_buffer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void deserialize_value(istream_reader &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message, tag_type);
void deserialize_value(istream_buffer &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message, tag_type);
void serialize_value(ostream_size &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void serialize_value(ostream_writer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void serialize_value(ostream_buffer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void deserialize_value(istream_reader &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message, tag_type);
void deserialize_value(istream_buffer &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message, tag_type);
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
void serialize_value(ostream_size &, const ::SPB::Options::Integers &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Integers &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Integers &message);
void deserialize_value(istream_reader &, ::SPB::Options::Integers &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Integers &message);
void serialize_value(ostream_size &, const ::SPB::Options::BitFields &message);
void serialize_value(ostream_writer &, const ::SPB::Options::BitFields &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::BitFields &message);
void deserialize_value(istream_reader &, ::SPB::Options::BitFields &message);
void deserialize_value(istream_buffer &, ::SPB::Options::BitFields &message);
void serialize_value(ostream_size &, const ::SPB::Options::MaximumCount &message);
void serialize_value(ostream_writer &, const ::SPB::Options::MaximumCount &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::MaximumCount &message);
void deserialize_value(istream_reader &, ::SPB::Options::MaximumCount &message);
void deserialize_value(istream_buffer &, ::SPB::Options::MaximumCount &message);
void serialize_value(ostream_size &, const ::SPB::Options::MaximumSize &message);
void serialize_value(ostream_writer &, const ::SPB::Options::MaximumSize &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::MaximumSize &message);
void deserialize_value(istream_reader &, ::SPB::Options::MaximumSize &message);
void deserialize_value(istream_buffer &, ::SPB::Options::MaximumSize &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers &message);
void serialize_value(ostream_size &, const ::SPB::Options::MaximumCount::Person &message);
void serialize_value(ostream_writer &, const ::SPB::Options::MaximumCount::Person &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::MaximumCount::Person &message);
void deserialize_value(istream_reader &, ::SPB::Options::MaximumCount::Person &message);
void deserialize_value(istream_buffer &, ::SPB::Options::MaximumCount::Person &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Repeated &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Repeated &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Repeated &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Repeated &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Repeated &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::String &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::String &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::String &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::String &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::String &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Bytes &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Bytes &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Bytes &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Bytes &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Bytes &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Maps &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Maps &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Maps &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Maps &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Maps &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Optional &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Optional &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Optional &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Optional &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Optional &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::String::SubStrings &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::String::SubStrings &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::String::SubStrings &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::String::SubStrings &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::String::SubStrings &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Bytes::SubBytes &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Bytes::SubBytes &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Bytes::SubBytes &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Bytes::SubBytes &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Bytes::SubBytes &message);
void serialize_value(ostream_size &, const ::SPB::Options::Containers::Optional::CyclicDependency &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Containers::Optional::CyclicDependency &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Containers::Optional::CyclicDependency &message);
void deserialize_value(istream_reader &, ::SPB::Options::Containers::Optional::CyclicDependency &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Containers::Optional::CyclicDependency &message);
void serialize_value(ostream_size &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void serialize_value(ostream_writer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void serialize_value(ostream_buffer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void deserialize_value(istream_reader &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void deserialize_value(istream_buffer &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageA &message);
void serialize_value(ostream_size &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void serialize_value(ostream_writer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void serialize_value(ostream_buffer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void deserialize_value(istream_reader &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void deserialize_value(istream_buffer &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageC &message);
void serialize_value(ostream_size &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void serialize_value(ostream_writer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void serialize_value(ostream_buffer &,
                     const ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void deserialize_value(istream_reader &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void deserialize_value(istream_buffer &,
                       ::SPB::Options::Containers::Optional::CyclicDependency::MessageB &message);
void serialize_value(ostream_size &, const ::SPB::Options::Enum8 &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Enum8 &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Enum8 &message);
void deserialize_value(istream_reader &, ::SPB::Options::Enum8 &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Enum8 &message);
void serialize_value(ostream_size &, const ::SPB::Options::Enum16 &message);
void serialize_value(ostream_writer &, const ::SPB::Options::Enum16 &message);
void serialize_value(ostream_buffer &, const ::SPB::Options::Enum16 &message);
void deserialize_value(istream_reader &, ::SPB::Options::Enum16 &message);
void deserialize_value(istream_buffer &, ::SPB::Options::Enum16 &message);
} // namespace detail

} // namespace spb::json
