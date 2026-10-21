#include "spb/concepts.h"
#include "spb/pb/serialize.hpp"
#include <array>
#include <name.pb.h>
#include <person.pb.h>
#include <proto/array.pb.h>
#include <proto/enum.pb.h>
#include <proto/map.pb.h>
#include <proto/options.pb.h>
#include <proto/simd.pb.h>
#include <reserved.pb.h>
#include <scalar.pb.h>
#include <span>
#include <spb/pb.hpp>
#include <string>
#include <string_view>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace
{
template <typename T>
concept HasValueMember = requires(T t) {
    { t.value };
};

template <typename T>
concept HasMapMember = requires(T t) {
    { t.map };
    requires spb::detail::proto_map<decltype(t.map)>;
};
} // namespace

static_assert(spb::detail::proto_field_bytes_resizable<std::vector<std::byte>>);
static_assert(spb::detail::proto_field_string_resizable<std::string>);
static_assert(!spb::detail::proto_field_bytes_resizable<std::array<std::byte, 4>>);
static_assert(!spb::detail::proto_field_string_resizable<std::array<char, 4>>);

namespace std
{
auto operator==(const std::span<const std::byte> &lhs, const std::span<const std::byte> &rhs) noexcept -> bool
{
    return lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}
} // namespace std

namespace Test
{
auto operator==(const Test::Name &lhs, const Test::Name &rhs) noexcept -> bool
{
    return lhs.name == rhs.name;
}
auto operator==(const Test::Variant &lhs, const Test::Variant &rhs) noexcept -> bool
{
    return lhs.oneof_field == rhs.oneof_field;
}

auto operator==(const TestPerson &lhs, const TestPerson &rhs) noexcept -> bool
{
    return lhs.value == rhs.value;
}

} // namespace Test

namespace example
{
auto operator==(const SomeMessage &lhs, const SomeMessage &rhs) noexcept -> bool
{
    return lhs.values == rhs.values;
}
} // namespace example

namespace PhoneBook
{
auto operator==(const Person::PhoneNumber &lhs, const Person::PhoneNumber &rhs) noexcept -> bool
{
    return lhs.number == rhs.number && lhs.type == rhs.type;
}

auto operator==(const Person &lhs, const Person &rhs) noexcept -> bool
{
    return lhs.name == rhs.name && lhs.id == rhs.id && lhs.email == rhs.email && lhs.phones == rhs.phones;
}
} // namespace PhoneBook

namespace Test::Scalar
{
auto operator==(const Empty &, const Empty &) noexcept -> bool
{
    return true;
}

auto operator==(const Simple &lhs, const Simple &rhs) noexcept -> bool
{
    return lhs.value == rhs.value;
}
} // namespace Test::Scalar

namespace reserved = UnitTest::cpp_keywords::private_::public_::int_::while_::do_;
namespace UnitTest::cpp_keywords::private_::public_::int_::while_::do_
{
auto operator==(const public_::private_ &lhs, const public_::private_ &rhs) noexcept -> bool
{
    return lhs.for_ == rhs.for_ && lhs.bool_ == rhs.bool_ && lhs.int_ == rhs.int_;
}

auto operator==(const public_ &lhs, const public_ &rhs) noexcept -> bool
{
    return lhs.p == rhs.p && lhs.while_ == rhs.while_;
}
} // namespace UnitTest::cpp_keywords::private_::public_::int_::while_::do_

namespace UnitTest::simd
{
auto operator==(const Data &lhs, const Data &rhs) noexcept -> bool
{
    return lhs.coordinates[0] == rhs.coordinates[0] && lhs.coordinates[1] == rhs.coordinates[1] &&
           lhs.coordinates[2] == rhs.coordinates[2] && lhs.coordinates[3] == rhs.coordinates[3];
}
} // namespace UnitTest::simd

namespace UnitTest::array
{
auto operator==(const Data &lhs, const Data &rhs) noexcept -> bool
{
    return lhs.words[0] == rhs.words[0] && lhs.words[1] == rhs.words[1] && lhs.words[2] == rhs.words[2] &&
           lhs.words[3] == rhs.words[3];
}
} // namespace UnitTest::array

namespace UnitTest::map
{
auto operator==(const Int32Int32 &lhs, const Int32Int32 &rhs) noexcept -> bool
{
    return lhs.map == rhs.map;
}
auto operator==(const StringString &lhs, const StringString &rhs) noexcept -> bool
{
    return lhs.map == rhs.map;
}
auto operator==(const Int32String &lhs, const Int32String &rhs) noexcept -> bool
{
    return lhs.map == rhs.map;
}
auto operator==(const StringInt32 &lhs, const StringInt32 &rhs) noexcept -> bool
{
    return lhs.map == rhs.map;
}
auto operator==(const Name &lhs, const Name &rhs) noexcept -> bool
{
    return lhs.name == rhs.name;
}
auto operator==(const StringName &lhs, const StringName &rhs) noexcept -> bool
{
    return lhs.map == rhs.map;
}
auto operator==(const UnorderedInt32Int32 &lhs, const UnorderedInt32Int32 &rhs) noexcept -> bool
{
    return lhs.map == rhs.map;
}
} // namespace UnitTest::map

namespace
{
template <size_t N> auto to_array(const char (&string)[N])
{
    auto result = std::array<std::byte, N - 1>();
    memcpy(result.data(), &string, result.size());
    return result;
}

template <size_t N> auto to_string(const char (&string)[N])
{
    auto result = std::array<char, N - 1>();
    memcpy(result.data(), &string, result.size());
    return result;
}

auto to_bytes(std::string_view str) -> std::vector<std::byte>
{
    auto span = std::span<std::byte>((std::byte *)str.data(), str.size());
    return {span.data(), span.data() + span.size()};
}

auto add_length_prepfix(std::string_view protobuf) -> std::string
{
    uint8_t buffer[12];
    auto stream = spb::pb::detail::ostream_buffer(buffer);
    spb::pb::detail::serialize_varint(stream, protobuf.size());
    return std::string((char *)buffer, stream.p_buffer - buffer) + std::string(protobuf);
}

template <typename T> void pb_test(const T &value, std::string_view protobuf)
{
    const auto protobuf_length_prefixed = add_length_prepfix(protobuf);

    {
        auto serialized = spb::pb::serialize<std::vector<std::byte>>(value);
        CHECK(serialized.size() == protobuf.size());
        auto proto = std::string_view((char *)serialized.data(), serialized.size());
        CHECK(proto == protobuf);
    }

    {
        auto serialized = spb::pb::serialize<std::vector<std::byte>>(value, {.delimited = true});
        CHECK(serialized.size() == protobuf_length_prefixed.size());
        auto proto = std::string_view((char *)serialized.data(), serialized.size());
        CHECK(proto == protobuf_length_prefixed);
    }

    {
        auto serialized = spb::pb::serialize(value);
        CHECK(serialized == protobuf);
        auto size = spb::pb::serialize_size(value);
        CHECK(size == protobuf.size());
    }

    {
        auto serialized = spb::pb::serialize(value, {.delimited = true});
        CHECK(serialized == protobuf_length_prefixed);
        auto size = spb::pb::serialize_size(value, {.delimited = true});
        CHECK(size == protobuf_length_prefixed.size());
    }

    {
        auto serialized = std::string();
        auto writer = [&serialized](const void *data, size_t size) { serialized.append((char *)data, size); };
        auto serialized_size = spb::pb::serialize(value, writer);
        auto size            = spb::pb::serialize_size(value);
        CHECK(serialized_size == size);
        CHECK(serialized == protobuf);
        CHECK(size == protobuf.size());
    }

    {
        auto serialized = std::string();
        auto writer = [&serialized](const void *data, size_t size) { serialized.append((char *)data, size); };
        auto serialized_size = spb::pb::serialize(value, writer, {.delimited = true});
        auto size            = spb::pb::serialize_size(value, {.delimited = true});
        CHECK(serialized_size == size);
        CHECK(serialized == protobuf_length_prefixed);
        CHECK(size == protobuf_length_prefixed.size());
    }

    {
        auto deserialized = spb::pb::deserialize<T>(protobuf);
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }

    {
        auto deserialized = spb::pb::deserialize<T>(protobuf_length_prefixed, {.delimited = true});
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }

    {
        auto deserialized = T();
        auto reader       = [protobuf](void *data, size_t size) mutable
        {
            const auto copy_size = std::min(protobuf.size(), size);
            memcpy(data, protobuf.data(), copy_size);
            protobuf.remove_prefix(copy_size);
            return copy_size;
        };
        auto size = spb::pb::deserialize(deserialized, reader);
        CHECK(size == protobuf.size());
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }

    {
        auto deserialized = T();
        auto reader = [protobuf = std::string_view(protobuf_length_prefixed)](void *data, size_t size) mutable
        {
            const auto copy_size = std::min(protobuf.size(), size);
            memcpy(data, protobuf.data(), copy_size);
            protobuf.remove_prefix(copy_size);
            return copy_size;
        };
        auto size = spb::pb::deserialize(deserialized, reader, {.delimited = true});
        CHECK(size == protobuf_length_prefixed.size());
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }

    {
        auto deserialized = T();
        spb::pb::deserialize(deserialized, protobuf);
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }

    {
        auto deserialized = T();
        spb::pb::deserialize(deserialized, protobuf_length_prefixed, {.delimited = true});
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }
}

template <typename T> void json_test(const T &value, std::string_view json)
{
    {
        auto serialized = std::string();
        auto size       = spb::json::serialize(value, serialized);
        CHECK(size == json.size());
        CHECK(serialized == json);
    }

    {
        auto serialized = spb::json::serialize(value);
        CHECK(serialized == json);
        auto size = spb::json::serialize_size(value);
        CHECK(size == json.size());
    }

    {
        auto serialized = std::string();
        auto writer = [&serialized](const void *data, size_t size) { serialized.append((char *)data, size); };
        auto size   = spb::json::serialize(value, writer);
        CHECK(size == json.size());
        CHECK(serialized == json);
    }

    {
        auto deserialized = spb::json::deserialize<T>(json);
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }
    {
        auto deserialized = T();
        spb::json::deserialize(deserialized, json);
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }
    {
        auto deserialized = T();
        auto reader       = [json](void *data, size_t size) mutable
        {
            const auto copy_size = std::min(json.size(), size);
            memcpy(data, json.data(), copy_size);
            json.remove_prefix(copy_size);
            return copy_size;
        };
        auto size = spb::json::deserialize(deserialized, reader);
        CHECK(size == json.size());
        if constexpr (HasValueMember<T>)
        {
            using valueT = decltype(T::value);
            CHECK(valueT(deserialized.value) == valueT(value.value));
        }
        else
        {
            CHECK(deserialized == value);
        }
    }
}

template <typename T> void pb_json_test(const T &value, std::string_view protobuf, std::string_view json)
{
    SUBCASE("pb")
    {
        pb_test(value, protobuf);
    }
    SUBCASE("json")
    {
        json_test(value, json);
    }
}
} // namespace
using namespace std::literals;

TEST_CASE("protobuf")
{
    SUBCASE("options")
    {
        SUBCASE("max_count")
        {
            const auto ints_packed =
                spb::pb::serialize<std::string>(Test::MaxCountIntPacked{.value = {0, 1, 2, 3, 4}});
            const auto ints = spb::pb::serialize<std::string>(Test::MaxCountInt{.value = {0, 1, 2, 3, 4}});
            const auto strings =
                spb::pb::serialize<std::string>(Test::MaxCountString{.value = {"0", "1", "2", "3", "4"}});
            const auto bytes   = spb::pb::serialize<std::string>(Test::MaxCountBytes{
                .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3"), to_bytes("4")}});
            const auto person  = Test::TestPerson{.value = "3"};
            const auto persons = spb::pb::serialize<std::string>(
                Test::MaxCountPerson{.value = {person, person, person, person, person}});

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountFieldIntPacked{.value = {0, 1, 2, 3, 4}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountFieldIntPacked>(ints_packed));
            pb_json_test(Test::MaxCountFieldIntPacked{.value = {0, 1, 2, 3}}, "\x0a\x04\x00\x01\x02\x03"sv,
                         R"({"value":[0,1,2,3]})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxCountFieldInt{.value = {0, 1, 2, 3, 4}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountFieldInt>(ints));
            pb_json_test(Test::MaxCountFieldInt{.value = {0, 1, 2, 3}}, "\x08\x00\x08\x01\x08\x02\x08\x03"sv,
                         R"({"value":[0,1,2,3]})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxCountMsgCommentInt{.value = {0, 1, 2, 3, 4}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgCommentInt>(ints));
            pb_json_test(Test::MaxCountMsgCommentInt{.value = {0, 1, 2, 3}},
                         "\x08\x00\x08\x01\x08\x02\x08\x03"sv, R"({"value":[0,1,2,3]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountFieldCommentIntPacked{.value = {0, 1, 2, 3, 4}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountFieldCommentIntPacked>(ints_packed));
            pb_json_test(Test::MaxCountFieldCommentIntPacked{.value = {0, 1, 2, 3}},
                         "\x0a\x04\x00\x01\x02\x03"sv, R"({"value":[0,1,2,3]})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxCountMsgIntPacked{.value = {0, 1, 2, 3, 4}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(ints_packed));
            pb_json_test(Test::MaxCountMsgIntPacked{.value = {0, 1, 2, 3}}, "\x0a\x04\x00\x01\x02\x03"sv,
                         R"({"value":[0,1,2,3]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountFieldString{.value = {"0", "1", "2", "3", "4"}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(strings));
            pb_json_test(Test::MaxCountFieldString{.value = {"0", "1", "2", "3"}},
                         "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                         R"({"value":["0","1","2","3"]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountMsgCommentString{.value = {"0", "1", "2", "3", "4"}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(strings));
            pb_json_test(Test::MaxCountMsgCommentString{.value = {"0", "1", "2", "3"}},
                         "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                         R"({"value":["0","1","2","3"]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountMsgString{.value = {"0", "1", "2", "3", "4"}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(strings));
            pb_json_test(Test::MaxCountMsgString{.value = {"0", "1", "2", "3"}},
                         "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                         R"({"value":["0","1","2","3"]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountFieldCommentString{.value = {"0", "1", "2", "3", "4"}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(strings));
            pb_json_test(Test::MaxCountFieldCommentString{.value = {"0", "1", "2", "3"}},
                         "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                         R"({"value":["0","1","2","3"]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(Test::MaxCountFieldBytes{
                .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3"), to_bytes("4")}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(bytes));
            pb_json_test(
                Test::MaxCountFieldBytes{
                    .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3")},
                },
                "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                R"({"value":["MA==","MQ==","Mg==","Mw=="]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(Test::MaxCountMsgCommentBytes{
                .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3"), to_bytes("4")}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(bytes));
            pb_json_test(
                Test::MaxCountMsgCommentBytes{
                    .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3")},
                },
                "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                R"({"value":["MA==","MQ==","Mg==","Mw=="]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(Test::MaxCountMsgbytes{
                .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3"), to_bytes("4")}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(bytes));
            pb_json_test(
                Test::MaxCountMsgbytes{
                    .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3")},
                },
                "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                R"({"value":["MA==","MQ==","Mg==","Mw=="]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(Test::MaxCountFieldCommentBytes{
                .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3"), to_bytes("4")}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(bytes));
            pb_json_test(
                Test::MaxCountFieldCommentBytes{
                    .value = {to_bytes("0"), to_bytes("1"), to_bytes("2"), to_bytes("3")},
                },
                "\x0a\x01\x30\x0a\x01\x31\x0a\x01\x32\x0a\x01\x33"sv,
                R"({"value":["MA==","MQ==","Mg==","Mw=="]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountFieldPerson{.value = {person, person, person, person, person}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(persons));
            pb_json_test(
                Test::MaxCountFieldPerson{
                    .value = {person, person, person, person},
                },
                "\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33"sv,
                R"({"value":[{"value":"3"},{"value":"3"},{"value":"3"},{"value":"3"}]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountFieldCommentPerson{.value = {person, person, person, person, person}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(persons));
            pb_json_test(
                Test::MaxCountFieldCommentPerson{
                    .value = {person, person, person, person},
                },
                "\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33"sv,
                R"({"value":[{"value":"3"},{"value":"3"},{"value":"3"},{"value":"3"}]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountMsgPerson{.value = {person, person, person, person, person}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(persons));
            pb_json_test(
                Test::MaxCountMsgPerson{
                    .value = {person, person, person, person},
                },
                "\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33"sv,
                R"({"value":[{"value":"3"},{"value":"3"},{"value":"3"},{"value":"3"}]})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxCountMsgCommentPerson{.value = {person, person, person, person, person}}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxCountMsgIntPacked>(persons));
            pb_json_test(
                Test::MaxCountMsgCommentPerson{
                    .value = {person, person, person, person},
                },
                "\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33\x0a\x03\x0a\x01\x33"sv,
                R"({"value":[{"value":"3"},{"value":"3"},{"value":"3"},{"value":"3"}]})");
        }
        SUBCASE("max_size")
        {
            const auto long_string =
                spb::pb::serialize<std::string>(Test::MaxSizeOptString{.value = "01234"});

            const auto long_bytes =
                spb::pb::serialize<std::string>(Test::MaxSizeOptBytes{.value = to_bytes("01234")});

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeOptStringField{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptStringField>(long_string));
            pb_json_test(Test::MaxSizeOptStringField{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeReqStringField{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqStringField>(long_string));
            pb_json_test(Test::MaxSizeReqStringField{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxSizeOptBytesField{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptBytesField>(long_bytes));
            pb_json_test(Test::MaxSizeOptBytesField{.value = to_bytes("0123")}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"MDEyMw=="})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxSizeReqBytesField{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqBytesField>(long_bytes));
            pb_json_test(Test::MaxSizeReqBytesField{.value = to_bytes("0123")}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"MDEyMw=="})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeOptStringComment{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptStringComment>(long_string));
            pb_json_test(Test::MaxSizeOptStringComment{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeReqStringComment{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqStringComment>(long_string));
            pb_json_test(Test::MaxSizeReqStringComment{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxSizeOptBytesComment{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptBytesComment>(long_bytes));
            pb_json_test(Test::MaxSizeOptBytesComment{.value = to_bytes("0123")},
                         "\x0a\x04\x30\x31\x32\x33"sv, R"({"value":"MDEyMw=="})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxSizeReqBytesComment{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqBytesComment>(long_bytes));
            pb_json_test(Test::MaxSizeReqBytesComment{.value = to_bytes("0123")},
                         "\x0a\x04\x30\x31\x32\x33"sv, R"({"value":"MDEyMw=="})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(Test::MaxSizeOptStringMsg{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptStringMsg>(long_string));
            pb_json_test(Test::MaxSizeOptStringMsg{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(Test::MaxSizeReqStringMsg{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqStringMsg>(long_string));
            pb_json_test(Test::MaxSizeReqStringMsg{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeOptBytesMsg{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptBytesMsg>(long_bytes));
            pb_json_test(Test::MaxSizeOptBytesMsg{.value = to_bytes("0123")}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"MDEyMw=="})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeReqBytesMsg{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqBytesMsg>(long_bytes));
            pb_json_test(Test::MaxSizeReqBytesMsg{.value = to_bytes("0123")}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"MDEyMw=="})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeOptStringMsgComment{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptStringMsgComment>(long_string));
            pb_json_test(Test::MaxSizeOptStringMsgComment{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS(
                (void)spb::pb::serialize<std::string>(Test::MaxSizeReqStringMsgComment{.value = "01234"}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqStringMsgComment>(long_string));
            pb_json_test(Test::MaxSizeReqStringMsgComment{.value = "0123"}, "\x0a\x04\x30\x31\x32\x33"sv,
                         R"({"value":"0123"})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxSizeOptBytesMsgComment{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeOptBytesMsgComment>(long_bytes));
            pb_json_test(Test::MaxSizeOptBytesMsgComment{.value = to_bytes("0123")},
                         "\x0a\x04\x30\x31\x32\x33"sv, R"({"value":"MDEyMw=="})");

            CHECK_THROWS((void)spb::pb::serialize<std::string>(
                Test::MaxSizeReqBytesMsgComment{.value = to_bytes("01234")}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::MaxSizeReqBytesMsgComment>(long_bytes));
            pb_json_test(Test::MaxSizeReqBytesMsgComment{.value = to_bytes("0123")},
                         "\x0a\x04\x30\x31\x32\x33"sv, R"({"value":"MDEyMw=="})");
        }
    }
    SUBCASE("tag")
    {
        SUBCASE("large field numbers")
        {
            pb_json_test(Test::Scalar::LargeFieldNumber{"hello"}, "\xa2\x06\x05hello",
                         R"({"value":"hello"})");
            pb_json_test(Test::Scalar::VeryLargeFieldNumber{"hello"}, "\xfa\xff\xff\xff\x0f\x05hello",
                         R"({"value":"hello"})");
        }
    }
    SUBCASE("enum")
    {
        SUBCASE("invalid")
        {
            CHECK_THROWS((void)spb::pb::serialize(
                Test::Scalar::ReqEnumAlias{.value = (Test::Scalar::ReqEnumAlias::Enum)3}));
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqEnumAlias>("\x08\x03"sv));
        }
        SUBCASE("alias")
        {
            SUBCASE("required")
            {
                pb_json_test(
                    Test::Scalar::ReqEnumAlias{.value = Test::Scalar::ReqEnumAlias::Enum::EAA_STARTED},
                    "\x08\x01", R"({"value":"EAA_STARTED"})");
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptEnumAlias{}, "", "{}");
                pb_json_test(
                    Test::Scalar::OptEnumAlias{.value = Test::Scalar::OptEnumAlias::Enum::EAA_STARTED},
                    "\x08\x01", R"({"value":"EAA_STARTED"})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepEnumAlias{}, "", "{}");
                pb_json_test(
                    Test::Scalar::RepEnumAlias{.value = {Test::Scalar::RepEnumAlias::Enum::EAA_STARTED}},
                    "\x08\x01", R"({"value":["EAA_STARTED"]})");
                pb_json_test(
                    Test::Scalar::RepEnumAlias{.value = {Test::Scalar::RepEnumAlias::Enum::EAA_STARTED,
                                                         Test::Scalar::RepEnumAlias::Enum::EAA_RUNNING}},
                    "\x08\x01\x08\x01", R"({"value":["EAA_STARTED","EAA_STARTED"]})");
            }
        }
    }
    SUBCASE("string")
    {
        SUBCASE("required")
        {
            pb_json_test(Test::Scalar::ReqString{}, "\x0a\x00"sv, "{}");
            pb_json_test(Test::Scalar::ReqString{.value = "hello"}, "\x0a\x05hello", R"({"value":"hello"})");
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqString>("\x0a\x05hell"sv));
            SUBCASE("escaped")
            {
                pb_json_test(Test::Scalar::ReqString{.value = "\"\\/\b\f\n\r\t"}, "\x0a\x08\"\\/\b\f\n\r\t",
                             R"({"value":"\"\\/\b\f\n\r\t"})");
                pb_json_test(Test::Scalar::ReqString{.value = "\"hello\t"}, "\x0a\x07\"hello\t",
                             R"({"value":"\"hello\t"})");
            }
            SUBCASE("utf8")
            {
                pb_json_test(Test::Scalar::ReqString{.value = "h\x16\xc3\x8c\xE3\x9B\x8B\xF0\x90\x87\xB3o"},
                             "\x0a\x0ch\x16\xc3\x8c\xE3\x9B\x8B\xF0\x90\x87\xB3o",
                             R"({"value":"h\u0016\u00cc\u36cb\ud800\uddf3o"})");
                SUBCASE("invalid")
                {
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqString>("\x0a\x02h\x80"sv));
                    CHECK_THROWS(
                        (void)spb::json::serialize<std::string>(Test::Scalar::ReqString{.value = "h\x80"}));
                    CHECK_THROWS(
                        (void)spb::json::deserialize<Test::Scalar::ReqString>(R"({"value":"h\u02w1"})"sv));
                    CHECK_THROWS(
                        (void)spb::json::deserialize<Test::Scalar::ReqString>(R"({"value":"h\u02"})"sv));
                    CHECK_THROWS(
                        (void)spb::json::deserialize<Test::Scalar::ReqString>(R"({"value":"h\ud800\u"})"sv));
                    CHECK_THROWS(
                        (void)spb::json::deserialize<Test::Scalar::ReqString>(R"({"value":"h\ud800\u1"})"sv));
                    CHECK_THROWS((void)spb::json::deserialize<Test::Scalar::ReqString>(
                        R"({"value":"h\ud800\udbff"})"sv));
                    CHECK_THROWS((void)spb::json::deserialize<Test::Scalar::ReqString>(
                        R"({"value":"h\ud800\ue000"})"sv));
                }
            }
        }
        SUBCASE("optional")
        {
            pb_json_test(Test::Scalar::OptString{}, "", "{}");
            // pb_json_test(Test::Scalar::OptString{.value = ""}, "\x0a\x00"sv, "{}");
            pb_json_test(Test::Scalar::OptString{.value = "hello"}, "\x0a\x05hello", R"({"value":"hello"})");
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptString>("\x08\x05hello"sv));
        }
        SUBCASE("repeated")
        {
            pb_json_test(Test::Scalar::RepString{}, "", "{}");
            pb_json_test(Test::Scalar::RepString{.value = {"hello"}}, "\x0a\x05hello",
                         R"({"value":["hello"]})");
            pb_json_test(Test::Scalar::RepString{.value = {"hello", "world"}}, "\x0a\x05hello\x0a\x05world",
                         R"({"value":["hello","world"]})");
        }
        SUBCASE("fixed")
        {
            SUBCASE("required")
            {
                // pb_json_test( Test::Scalar::ReqString{ }, "\x0a\x06\x00\x00\x00\x00\x00\x00"sv,
                // R"({"value":"hello"})" );
                pb_json_test(Test::Scalar::ReqStringFixed{.value = to_string("hello1")}, "\x0a\x06hello1",
                             R"({"value":"hello1"})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqStringFixed>("\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqStringFixed>("\x0a\x07hello12"sv));
                SUBCASE("escaped")
                {
                    pb_json_test(Test::Scalar::ReqStringFixed{.value = to_string("\"\\/\n\r\t")},
                                 "\x0a\x06\"\\/\n\r\t", R"({"value":"\"\\/\n\r\t"})");
                    pb_json_test(Test::Scalar::ReqStringFixed{.value = to_string("hello\t")},
                                 "\x0a\x06hello\t", R"({"value":"hello\t"})");
                }
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptStringFixed{}, "", "{}");
                pb_json_test(Test::Scalar::OptStringFixed{.value = to_string("hello1")}, "\x0a\x06hello1",
                             R"({"value":"hello1"})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptStringFixed>("\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptStringFixed>("\x0a\x07hello12"sv));
                CHECK_THROWS(
                    (void)spb::json::deserialize<Test::Scalar::OptStringFixed>(R"({"value":"hello12"})"sv));
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepStringFixed{}, "", "{}");
                pb_json_test(Test::Scalar::RepStringFixed{.value = {to_string("hello1")}}, "\x0a\x06hello1",
                             R"({"value":["hello1"]})");
                pb_json_test(
                    Test::Scalar::RepStringFixed{.value = {to_string("hello1"), to_string("world1")}},
                    "\x0a\x06hello1\x0a\x06world1", R"({"value":["hello1","world1"]})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepStringFixed>(
                    "\x0a\x06hello1\x0a\x05world"sv));
                CHECK_THROWS((void)spb::json::deserialize<Test::Scalar::RepStringFixed>(
                    R"({"value":["hello1","world"]})"sv));
            }
        }
    }
    SUBCASE("bool")
    {
        SUBCASE("required")
        {
            pb_json_test(Test::Scalar::ReqBool{}, "\x08\x00"sv, R"({"value":false})");
            pb_json_test(Test::Scalar::ReqBool{.value = true}, "\x08\x01", R"({"value":true})");
            pb_json_test(Test::Scalar::ReqBool{.value = false}, "\x08\x00"sv, R"({"value":false})");
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqBool>("\x08\x02"sv));
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqBool>("\x08\xff\x01"sv));
        }
        SUBCASE("optional")
        {
            pb_json_test(Test::Scalar::OptBool{}, "", "{}");
            pb_json_test(Test::Scalar::OptBool{.value = true}, "\x08\x01", R"({"value":true})");
            pb_json_test(Test::Scalar::OptBool{.value = false}, "\x08\x00"sv, R"({"value":false})");
        }
        SUBCASE("repeated")
        {
            pb_json_test(Test::Scalar::RepBool{}, "", "{}");
            pb_json_test(Test::Scalar::RepBool{.value = {true}}, "\x08\x01", R"({"value":[true]})");
            pb_json_test(Test::Scalar::RepBool{.value = {true, false}}, "\x08\x01\x08\x00"sv,
                         R"({"value":[true,false]})");
            pb_json_test(Test::Scalar::RepBool{.value = {}}, "", "{}");
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepBool>("\x08\x01\x08"sv));

            SUBCASE("packed")
            {
                pb_json_test(Test::Scalar::RepPackBool{}, "", "{}");
                pb_json_test(Test::Scalar::RepPackBool{.value = {true}}, "\x0a\x01\x01",
                             R"({"value":[true]})");
                pb_json_test(Test::Scalar::RepPackBool{.value = {true, false}}, "\x0a\x02\x01\x00"sv,
                             R"({"value":[true,false]})");
                pb_json_test(Test::Scalar::RepPackBool{.value = {}}, "", "{}");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackBool>("\x0a\x02\x08"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackBool>("\x0a\x01\x08"sv));
            }
        }
    }
    SUBCASE("enum")
    {
        SUBCASE("required")
        {
            pb_json_test(Test::Scalar::ReqEnum{.value = Test::Scalar::ReqEnum::Enum::Enum_value}, "\x08\x01",
                         R"({"value":"Enum_value"})");
        }
        SUBCASE("optional")
        {
            pb_json_test(Test::Scalar::OptEnum{}, "", "{}");
            pb_json_test(Test::Scalar::OptEnum{.value = Test::Scalar::OptEnum::Enum::Enum_value}, "\x08\x01",
                         R"({"value":"Enum_value"})");
        }
        SUBCASE("repeated")
        {
            pb_json_test(Test::Scalar::RepEnum{}, "", "{}");
            pb_json_test(Test::Scalar::RepEnum{.value = {Test::Scalar::RepEnum::Enum::Enum_value}},
                         "\x08\x01", R"({"value":["Enum_value"]})");
            pb_json_test(Test::Scalar::RepEnum{.value = {Test::Scalar::RepEnum::Enum::Enum_value,
                                                         Test::Scalar::RepEnum::Enum::Enum_value2,
                                                         Test::Scalar::RepEnum::Enum::Enum_value3}},
                         "\x08\x01\x08\x02\x08\x03",
                         R"({"value":["Enum_value","Enum_value2","Enum_value3"]})");
            pb_json_test(Test::Scalar::RepEnum{.value = {}}, "", "{}");

            SUBCASE("packed")
            {
                pb_json_test(Test::Scalar::RepPackEnum{}, "", "{}");
                pb_json_test(
                    Test::Scalar::RepPackEnum{.value = {Test::Scalar::RepPackEnum::Enum::Enum_value}},
                    "\x0a\x01\x01", R"({"value":["Enum_value"]})");
                pb_json_test(
                    Test::Scalar::RepPackEnum{.value = {Test::Scalar::RepPackEnum::Enum::Enum_value,
                                                        Test::Scalar::RepPackEnum::Enum::Enum_value3}},
                    "\x0a\x02\x01\x03", R"({"value":["Enum_value","Enum_value3"]})");
                pb_json_test(Test::Scalar::RepPackEnum{.value = {}}, "", "{}");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackEnum>("\x0a\x02\x42"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackEnum>("\x0a\x02\x42\xff"sv));

                pb_json_test(
                    example::SomeMessage{
                        .values = {example::SomeEnum::SOME_ENUM_ONE, example::SomeEnum::SOME_ENUM_TWO,
                                   example::SomeEnum::SOME_ENUM_THREE, example::SomeEnum::SOME_ENUM_FOUR,
                                   example::SomeEnum::SOME_ENUM_FIVE}},
                    "\x0A\x05\x01\x02\x03\x04\x05"sv,
                    R"({"values":["SOME_ENUM_ONE","SOME_ENUM_TWO","SOME_ENUM_THREE","SOME_ENUM_FOUR","SOME_ENUM_FIVE"]})");
            }
        }
    }
    SUBCASE("int")
    {
        SUBCASE("varint32")
        {
            SUBCASE("bitfield")
            {
                pb_json_test(Test::Scalar::ReqInt8_1{.value = -1},
                             "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqInt8_1{.value = 0}, "\x08\x00"sv, R"({"value":0})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt8_1>(
                    "\x08\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt8_1>("\x08\x01"sv));

                pb_json_test(Test::Scalar::ReqInt32_1{.value = -1},
                             "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqInt32_1{.value = 0}, "\x08\x00"sv, R"({"value":0})");

                pb_json_test(Test::Scalar::ReqInt8_2{.value = -2},
                             "\x08\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv, R"({"value":-2})");
                pb_json_test(Test::Scalar::ReqInt8_2{.value = -1},
                             "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqInt8_2{.value = 0}, "\x08\x00"sv, R"({"value":0})");
                pb_json_test(Test::Scalar::ReqInt8_2{.value = 1}, "\x08\x01"sv, R"({"value":1})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt8_1>(
                    "\x08\xfd\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt8_1>("\x08\x02"sv));
            }
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqInt32{.value = 0x42}, "\x08\x42", R"({"value":66})");
                pb_json_test(Test::Scalar::ReqInt32{.value = 0xff}, "\x08\xff\x01", R"({"value":255})");
                pb_json_test(Test::Scalar::ReqInt32{.value = -2},
                             "\x08\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv, R"({"value":-2})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt32>("\x08"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt32>("\x08\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt32>(
                    "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptInt32{}, "", "{}");
                pb_json_test(Test::Scalar::OptInt32{.value = 0x42}, "\x08\x42", R"({"value":66})");
                pb_json_test(Test::Scalar::OptInt32{.value = 0xff}, "\x08\xff\x01", R"({"value":255})");
                pb_json_test(Test::Scalar::OptInt32{.value = -2},
                             "\x08\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv, R"({"value":-2})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepInt32{}, "", "{}");
                pb_json_test(Test::Scalar::RepInt32{.value = {0x42}}, "\x08\x42", R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepInt32{.value = {0x42, 0x3}}, "\x08\x42\x08\x03",
                             R"({"value":[66,3]})");
                pb_json_test(Test::Scalar::RepInt32{.value = {}}, "", "{}");

                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackInt32{}, "", "{}");
                    pb_json_test(Test::Scalar::RepPackInt32{.value = {0x42}}, "\x0a\x01\x42",
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackInt32{.value = {0x42, 0x3}}, "\x0a\x02\x42\x03",
                                 R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepPackInt32{.value = {}}, "", "{}");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackInt32>("\x0a\x02\x42"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::RepPackInt32>("\x0a\x02\x42\xff"sv));
                }
            }
        }
        SUBCASE("varuint32")
        {
            SUBCASE("bitfield")
            {
                pb_json_test(Test::Scalar::ReqUint8_1{.value = 0}, "\x08\x00"sv, R"({"value":0})");
                pb_json_test(Test::Scalar::ReqUint8_1{.value = 1}, "\x08\x01"sv, R"({"value":1})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqUint8_1>(
                    "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqUint8_1>("\x08\x02"sv));

                pb_json_test(Test::Scalar::ReqUint32_1{.value = 0}, "\x08\x00"sv, R"({"value":0})");
                pb_json_test(Test::Scalar::ReqUint32_1{.value = 1}, "\x08\x01"sv, R"({"value":1})");

                pb_json_test(Test::Scalar::ReqUint8_2{.value = 0}, "\x08\x00"sv, R"({"value":0})");
                pb_json_test(Test::Scalar::ReqUint8_2{.value = 1}, "\x08\x01"sv, R"({"value":1})");
                pb_json_test(Test::Scalar::ReqUint8_2{.value = 2}, "\x08\x02"sv, R"({"value":2})");
                pb_json_test(Test::Scalar::ReqUint8_2{.value = 3}, "\x08\x03"sv, R"({"value":3})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqUint8_2>(
                    "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqUint8_1>("\x08\x04"sv));
            }

            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqUint32{.value = 0x42}, "\x08\x42", R"({"value":66})");
                pb_json_test(Test::Scalar::ReqUint32{.value = 0xff}, "\x08\xff\x01", R"({"value":255})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqUint32>("\x08"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqUint32>("\x08\xff"sv));
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqUint32>("\x08\xff\xff\xff\xff\xff\x0f"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqUint32>(
                    "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptUint32{}, "", "{}");
                pb_json_test(Test::Scalar::OptUint32{.value = 0x42}, "\x08\x42", R"({"value":66})");
                pb_json_test(Test::Scalar::OptUint32{.value = 0xff}, "\x08\xff\x01", R"({"value":255})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepUint32{}, "", "{}");
                pb_json_test(Test::Scalar::RepUint32{.value = {0x42}}, "\x08\x42", R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepUint32{.value = {0x42, 0x3}}, "\x08\x42\x08\x03",
                             R"({"value":[66,3]})");
                pb_json_test(Test::Scalar::RepUint32{.value = {}}, "", "{}");

                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackUint32{}, "", "{}");
                    pb_json_test(Test::Scalar::RepPackUint32{.value = {0x42}}, "\x0a\x01\x42",
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackUint32{.value = {0x42, 0x3}}, "\x0a\x02\x42\x03",
                                 R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepPackUint32{.value = {}}, "", "{}");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackUint32>("\x0a\x02\x42"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::RepPackUint32>("\x0a\x02\x42\xff"sv));
                }
            }
        }
        SUBCASE("varint64")
        {
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqInt64{.value = 0x42}, "\x08\x42", R"({"value":66})");
                pb_json_test(Test::Scalar::ReqInt64{.value = 0xff}, "\x08\xff\x01", R"({"value":255})");
                pb_json_test(Test::Scalar::ReqInt64{.value = -2},
                             "\x08\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01", R"({"value":-2})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt32>("\x08"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt32>("\x08\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt32>(
                    "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptInt64{}, "", "{}");
                pb_json_test(Test::Scalar::OptInt64{.value = 0x42}, "\x08\x42", R"({"value":66})");
                pb_json_test(Test::Scalar::OptInt64{.value = 0xff}, "\x08\xff\x01", R"({"value":255})");
                pb_json_test(Test::Scalar::OptInt64{.value = -2},
                             "\x08\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01", R"({"value":-2})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepInt64{}, "", "{}");
                pb_json_test(Test::Scalar::RepInt64{.value = {0x42}}, "\x08\x42", R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepInt64{.value = {0x42, 0x3}}, "\x08\x42\x08\x03",
                             R"({"value":[66,3]})");
                pb_json_test(Test::Scalar::RepInt64{.value = {}}, "", "{}");

                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackInt64{}, "", "{}");
                    pb_json_test(Test::Scalar::RepPackInt64{.value = {0x42}}, "\x0a\x01\x42",
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackInt64{.value = {0x42, 0x3}}, "\x0a\x02\x42\x03",
                                 R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepPackInt64{.value = {}}, "", "{}");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackInt64>("\x0a\x02\x42"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::RepPackInt64>("\x0a\x02\x42\xff"sv));
                }
            }
        }
        SUBCASE("svarint32")
        {
            SUBCASE("bitfield")
            {
                pb_json_test(Test::Scalar::ReqSint8_1{.value = -1}, "\x08\x01"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSint8_1{.value = 0}, "\x08\x00"sv, R"({"value":0})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint8_1>("\x08\x03"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint8_1>("\x08\x02"sv));

                pb_json_test(Test::Scalar::ReqSint32_1{.value = -1}, "\x08\x01"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSint32_1{.value = 0}, "\x08\x00"sv, R"({"value":0})");

                pb_json_test(Test::Scalar::ReqSint8_2{.value = -2}, "\x08\x03"sv, R"({"value":-2})");
                pb_json_test(Test::Scalar::ReqSint8_2{.value = -1}, "\x08\x01"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSint8_2{.value = 0}, "\x08\x00"sv, R"({"value":0})");
                pb_json_test(Test::Scalar::ReqSint8_2{.value = 1}, "\x08\x02"sv, R"({"value":1})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt8_1>(
                    "\x08\xfd\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqInt8_1>("\x08\x04"sv));
            }
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqSint32{.value = 0x42}, "\x08\x84\x01"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::ReqSint32{.value = 0xff}, "\x08\xfe\x03"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::ReqSint32{.value = -2}, "\x08\x03"sv, R"({"value":-2})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint32>("\x08"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint32>("\x08\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint32>(
                    "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptSint32{}, "", "{}");
                pb_json_test(Test::Scalar::OptSint32{.value = 0x42}, "\x08\x84\x01"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::OptSint32{.value = 0xff}, "\x08\xfe\x03"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::OptSint32{.value = -2}, "\x08\x03"sv, R"({"value":-2})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepSint32{}, "", "{}");
                pb_json_test(Test::Scalar::RepSint32{.value = {0x42}}, "\x08\x84\x01"sv, R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepSint32{.value = {0x42, -2}}, "\x08\x84\x01\x08\x03"sv,
                             R"({"value":[66,-2]})");
                pb_json_test(Test::Scalar::RepSint32{.value = {}}, "", "{}");

                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackSint32{.value = {0x42}}, "\x0a\x02\x84\x01"sv,
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackSint32{.value = {0x42, -2}}, "\x0a\x03\x84\x01\x03"sv,
                                 R"({"value":[66,-2]})");
                    pb_json_test(Test::Scalar::RepPackSint32{.value = {}}, "", "{}");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackSint32>("\x0a\x02\x42"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::RepPackSint32>("\x0a\x02\x42\xff"sv));
                }
            }
        }
        SUBCASE("svarint64")
        {
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqSint64{.value = 0x42}, "\x08\x84\x01"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::ReqSint64{.value = 0xff}, "\x08\xfe\x03"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::ReqSint64{.value = -2}, "\x08\x03"sv, R"({"value":-2})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint64>("\x08"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint64>("\x08\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSint64>(
                    "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptSint64{}, "", "{}");
                pb_json_test(Test::Scalar::OptSint64{.value = 0x42}, "\x08\x84\x01"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::OptSint64{.value = 0xff}, "\x08\xfe\x03"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::OptSint64{.value = -2}, "\x08\x03"sv, R"({"value":-2})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepSint64{}, "", "{}");
                pb_json_test(Test::Scalar::RepSint64{.value = {0x42}}, "\x08\x84\x01"sv, R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepSint64{.value = {0x42, -2}}, "\x08\x84\x01\x08\x03"sv,
                             R"({"value":[66,-2]})");
                pb_json_test(Test::Scalar::RepSint64{.value = {}}, "", "{}");

                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackSint64{.value = {0x42}}, "\x0a\x02\x84\x01"sv,
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackSint64{.value = {0x42, -2}}, "\x0a\x03\x84\x01\x03"sv,
                                 R"({"value":[66,-2]})");
                    pb_json_test(Test::Scalar::RepPackSint64{.value = {}}, "", "{}");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackSint64>("\x0a\x02\x84"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::RepPackSint64>("\x0a\x03\x84\x01"sv));
                }
            }
        }
        SUBCASE("fixed32")
        {
            SUBCASE("bitfield")
            {
                pb_json_test(Test::Scalar::ReqFixed32_32_1{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                             R"({"value":0})");
                pb_json_test(Test::Scalar::ReqFixed32_32_1{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                             R"({"value":1})");

                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_32_1>("\x0d\xff\xff\xff\xff"sv));
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_32_1>("\x0d\x02\x00\x00\x00"sv));

                pb_json_test(Test::Scalar::ReqFixed32_32_2{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                             R"({"value":0})");
                pb_json_test(Test::Scalar::ReqFixed32_32_2{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                             R"({"value":1})");
                pb_json_test(Test::Scalar::ReqFixed32_32_2{.value = 2}, "\x0d\x02\x00\x00\x00"sv,
                             R"({"value":2})");
                pb_json_test(Test::Scalar::ReqFixed32_32_2{.value = 3}, "\x0d\x03\x00\x00\x00"sv,
                             R"({"value":3})");

                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_32_2>("\x0d\xff\xff\xff\xff"sv));
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_32_2>("\x0d\x04\x00\x00\x00"sv));
            }
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqFixed32{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                             R"({"value":66})");
                pb_json_test(Test::Scalar::ReqFixed32{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                             R"({"value":255})");
                pb_json_test(Test::Scalar::ReqFixed32{.value = uint32_t(-2)}, "\x0d\xfe\xff\xff\xff"sv,
                             R"({"value":4294967294})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed32>("\x0d\x42\x00\x00"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptFixed32{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                             R"({"value":66})");
                pb_json_test(Test::Scalar::OptFixed32{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                             R"({"value":255})");
                pb_json_test(Test::Scalar::OptFixed32{.value = -2}, "\x0d\xfe\xff\xff\xff"sv,
                             R"({"value":4294967294})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepFixed32{.value = {0x42}}, "\x0d\x42\x00\x00\x00"sv,
                             R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepFixed32{.value = {0x42, 0x3}},
                             "\x0d\x42\x00\x00\x00\x0d\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                pb_json_test(Test::Scalar::RepFixed32{.value = {}}, "", "{}");

                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackFixed32{.value = {0x42}}, "\x0a\x04\x42\x00\x00\x00"sv,
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackFixed32{.value = {0x42, 0x3}},
                                 "\x0a\x08\x42\x00\x00\x00\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepPackFixed32{.value = {}}, "", "{}");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackFixed32>(
                        "\x0a\x08\x42\x00\x00\x00\x03\x00\x00"sv));
                }
            }
            SUBCASE("uint8")
            {
                SUBCASE("bitfield")
                {
                    pb_json_test(Test::Scalar::ReqFixed32_8_1{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                                 R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqFixed32_8_1{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                                 R"({"value":1})");

                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_8_1>("\x0d\xff\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_8_1>("\x0d\x02\x00\x00\x00"sv));

                    pb_json_test(Test::Scalar::ReqFixed32_8_2{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                                 R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqFixed32_8_2{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                                 R"({"value":1})");
                    pb_json_test(Test::Scalar::ReqFixed32_8_2{.value = 2}, "\x0d\x02\x00\x00\x00"sv,
                                 R"({"value":2})");
                    pb_json_test(Test::Scalar::ReqFixed32_8_2{.value = 3}, "\x0d\x03\x00\x00\x00"sv,
                                 R"({"value":3})");

                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_8_2>("\x0d\xff\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_8_2>("\x0d\x04\x00\x00\x00"sv));
                }
                SUBCASE("required")
                {
                    CHECK(sizeof(Test::Scalar::ReqFixed32_8::value) == sizeof(uint8_t));
                    pb_json_test(Test::Scalar::ReqFixed32_8{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":66})");
                    pb_json_test(Test::Scalar::ReqFixed32_8{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                                 R"({"value":255})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_8>("\x0d\xfe\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_8>("\x0d\x42\x00\x00"sv));
                }
                SUBCASE("optional")
                {
                    pb_json_test(Test::Scalar::OptFixed32_8{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":66})");
                    pb_json_test(Test::Scalar::OptFixed32_8{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                                 R"({"value":255})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_8>("\x0d\xfe\xff\xff\xff"sv));
                }
                SUBCASE("repeated")
                {
                    pb_json_test(Test::Scalar::RepFixed32_8{.value = {0x42}}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepFixed32_8{.value = {0x42, 0x3}},
                                 "\x0d\x42\x00\x00\x00\x0d\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepFixed32_8{.value = {}}, "", "{}");

                    SUBCASE("packed")
                    {
                        pb_json_test(Test::Scalar::RepPackFixed32_8{.value = {0x42}},
                                     "\x0a\x04\x42\x00\x00\x00"sv, R"({"value":[66]})");
                        pb_json_test(Test::Scalar::RepPackFixed32_8{.value = {0x42, 0x3}},
                                     "\x0a\x08\x42\x00\x00\x00\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                        pb_json_test(Test::Scalar::RepPackFixed32_8{.value = {}}, "", "{}");
                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackFixed32_8>(
                            "\x0a\x08\x42\x00\x00\x00\x03\x00\x00"sv));
                    }
                }
            }
            SUBCASE("uint16")
            {
                SUBCASE("bitfield")
                {
                    pb_json_test(Test::Scalar::ReqFixed32_16_1{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                                 R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqFixed32_16_1{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                                 R"({"value":1})");

                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_16_1>("\x0d\xff\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_16_1>("\x0d\x02\x00\x00\x00"sv));

                    pb_json_test(Test::Scalar::ReqFixed32_16_2{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                                 R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqFixed32_16_2{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                                 R"({"value":1})");
                    pb_json_test(Test::Scalar::ReqFixed32_16_2{.value = 2}, "\x0d\x02\x00\x00\x00"sv,
                                 R"({"value":2})");
                    pb_json_test(Test::Scalar::ReqFixed32_16_2{.value = 3}, "\x0d\x03\x00\x00\x00"sv,
                                 R"({"value":3})");

                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_16_2>("\x0d\xff\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_16_2>("\x0d\x04\x00\x00\x00"sv));
                }
                SUBCASE("required")
                {
                    CHECK(sizeof(Test::Scalar::ReqFixed32_16::value) == sizeof(uint16_t));

                    pb_json_test(Test::Scalar::ReqFixed32_16{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":66})");
                    pb_json_test(Test::Scalar::ReqFixed32_16{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                                 R"({"value":255})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_16>("\x0d\xfe\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_16>("\x0d\x42\x00\x00"sv));
                }
                SUBCASE("optional")
                {
                    pb_json_test(Test::Scalar::OptFixed32_16{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":66})");
                    pb_json_test(Test::Scalar::OptFixed32_16{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                                 R"({"value":255})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqFixed32_16>("\x0d\xfe\xff\xff\xff"sv));
                }
                SUBCASE("repeated")
                {
                    pb_json_test(Test::Scalar::RepFixed32_16{.value = {0x42}}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepFixed32_16{.value = {0x42, 0x3}},
                                 "\x0d\x42\x00\x00\x00\x0d\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepFixed32_16{.value = {}}, "", "{}");

                    SUBCASE("packed")
                    {
                        pb_json_test(Test::Scalar::RepPackFixed32_16{.value = {0x42}},
                                     "\x0a\x04\x42\x00\x00\x00"sv, R"({"value":[66]})");
                        pb_json_test(Test::Scalar::RepPackFixed32_16{.value = {0x42, 0x3}},
                                     "\x0a\x08\x42\x00\x00\x00\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                        pb_json_test(Test::Scalar::RepPackFixed32_16{.value = {}}, "", "{}");
                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepPackFixed32_16>(
                            "\x0a\x08\x42\x00\x00\x00\x03\x00\x00"sv));
                    }
                }
            }
        }
        SUBCASE("fixed64")
        {
            SUBCASE("bitfield")
            {
                pb_json_test(Test::Scalar::ReqSfixed64_64_1{.value = -1},
                             "\x09\xff\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_1{.value = 0},
                             "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_1>(
                    "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_1>(
                    "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv));

                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = -2},
                             "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-2})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = -1},
                             "\x09\xff\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = 0},
                             "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = 1},
                             "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":1})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_2>(
                    "\x09\xfd\xff\xff\xff\xff\xff\xff\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_2>(
                    "\x09\x02\x00\x00\x00\x00\x00\x00\x00"sv));
            }
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqFixed64{.value = 0x42},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::ReqFixed64{.value = 0xff},
                             "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::ReqFixed64{.value = uint64_t(-2)},
                             "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":18446744073709551614})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed32>(
                    "\x09\x42\x00\x00\x00\x00\x00\x00"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptFixed64{.value = 0x42},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::OptFixed64{.value = 0xff},
                             "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::OptFixed64{.value = -2}, "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv,
                             R"({"value":18446744073709551614})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepFixed64{.value = {0x42}},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepFixed64{.value = {0x42, 0x3}},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00\x09\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                             R"({"value":[66,3]})");
                pb_json_test(Test::Scalar::RepFixed64{.value = {}}, "", "{}");
                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackFixed64{.value = {0x42}},
                                 "\x0a\x08\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackFixed64{.value = {0x42, 0x3}},
                                 "\x0a\x10\x42\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                                 R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepPackFixed64{.value = {}}, "", "{}");
                }
            }
            SUBCASE("uint8")
            {
                SUBCASE("bitfield")
                {
                    pb_json_test(Test::Scalar::ReqFixed64_8_1{.value = 0},
                                 "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqFixed64_8_1{.value = 1},
                                 "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":1})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed64_8_1>(
                        "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed64_8_1>(
                        "\x09\x02\x00\x00\x00\x00\x00\x00\x00"sv));

                    pb_json_test(Test::Scalar::ReqFixed64_8_2{.value = 0},
                                 "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqFixed64_8_2{.value = 1},
                                 "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":1})");
                    pb_json_test(Test::Scalar::ReqFixed64_8_2{.value = 2},
                                 "\x09\x02\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":2})");
                    pb_json_test(Test::Scalar::ReqFixed64_8_2{.value = 3},
                                 "\x09\x03\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":3})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed64_8_2>(
                        "\x09\xfd\xff\xff\xff\xff\xff\xff\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed64_8_2>(
                        "\x09\x04\x00\x00\x00\x00\x00\x00\x00"sv));
                }
                SUBCASE("required")
                {
                    CHECK(sizeof(Test::Scalar::ReqFixed64_8::value) == sizeof(uint8_t));

                    pb_json_test(Test::Scalar::ReqFixed64_8{.value = 0x42},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                    pb_json_test(Test::Scalar::ReqFixed64_8{.value = 0xff},
                                 "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":255})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed64_8>(
                        "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFixed64_8>(
                        "\x09\x42\x00\x00\x00\x00\x00\x00"sv));
                }
                SUBCASE("optional")
                {
                    pb_json_test(Test::Scalar::OptFixed64_8{.value = 0x42},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                    pb_json_test(Test::Scalar::OptFixed64_8{.value = 0xff},
                                 "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":255})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptFixed64_8>(
                        "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv));
                }
                SUBCASE("repeated")
                {
                    pb_json_test(Test::Scalar::RepFixed64_8{.value = {0x42}},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepFixed64_8{.value = {0x42, 0x3}},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00\x09\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                                 R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepFixed64_8{.value = {}}, "", "{}");
                    SUBCASE("packed")
                    {
                        pb_json_test(Test::Scalar::RepPackFixed64_8{.value = {0x42}},
                                     "\x0a\x08\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                        pb_json_test(
                            Test::Scalar::RepPackFixed64_8{.value = {0x42, 0x3}},
                            "\x0a\x10\x42\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                            R"({"value":[66,3]})");
                        pb_json_test(Test::Scalar::RepPackFixed64_8{.value = {}}, "", "{}");
                    }
                }
            }
        }
        SUBCASE("sfixed32")
        {
            SUBCASE("bitfield")
            {
                pb_json_test(Test::Scalar::ReqSfixed32_32_1{.value = -1}, "\x0d\xff\xff\xff\xff"sv,
                             R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSfixed32_32_1{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                             R"({"value":0})");
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_32_1>("\x0d\xfe\xff\xff\xff"sv));
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_32_1>("\x0d\x01\x00\x00\x00"sv));

                pb_json_test(Test::Scalar::ReqSfixed32_32_2{.value = -2}, "\x0d\xfe\xff\xff\xff"sv,
                             R"({"value":-2})");
                pb_json_test(Test::Scalar::ReqSfixed32_32_2{.value = -1}, "\x0d\xff\xff\xff\xff"sv,
                             R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSfixed32_32_2{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                             R"({"value":0})");
                pb_json_test(Test::Scalar::ReqSfixed32_32_2{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                             R"({"value":1})");
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_32_2>("\x0d\xfd\xff\xff\xff"sv));
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_32_2>("\x0d\x02\x00\x00\x00"sv));
            }
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqSfixed32{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                             R"({"value":66})");
                pb_json_test(Test::Scalar::ReqSfixed32{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                             R"({"value":255})");
                pb_json_test(Test::Scalar::ReqSfixed32{.value = -2}, "\x0d\xfe\xff\xff\xff"sv,
                             R"({"value":-2})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed32>("\x0d\x42\x00\x00"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptSfixed32{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                             R"({"value":66})");
                pb_json_test(Test::Scalar::OptSfixed32{.value = 0xff}, "\x0d\xff\x00\x00\x00"sv,
                             R"({"value":255})");
                pb_json_test(Test::Scalar::OptSfixed32{.value = -2}, "\x0d\xfe\xff\xff\xff"sv,
                             R"({"value":-2})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepSfixed32{.value = {0x42}}, "\x0d\x42\x00\x00\x00"sv,
                             R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepSfixed32{.value = {0x42, 0x3}},
                             "\x0d\x42\x00\x00\x00\x0d\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                pb_json_test(Test::Scalar::RepSfixed32{.value = {}}, "", "{}");

                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackSfixed32{.value = {0x42}}, "\x0a\x04\x42\x00\x00\x00"sv,
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackSfixed32{.value = {0x42, 0x3}},
                                 "\x0a\x08\x42\x00\x00\x00\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepPackSfixed32{.value = {}}, "", "{}");
                }
            }
            SUBCASE("int8")
            {
                SUBCASE("bitfield")
                {
                    pb_json_test(Test::Scalar::ReqSfixed32_8_1{.value = -1}, "\x0d\xff\xff\xff\xff"sv,
                                 R"({"value":-1})");
                    pb_json_test(Test::Scalar::ReqSfixed32_8_1{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                                 R"({"value":0})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_8_1>("\x0d\xfe\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_8_1>("\x0d\x01\x00\x00\x00"sv));

                    pb_json_test(Test::Scalar::ReqSfixed32_8_2{.value = -2}, "\x0d\xfe\xff\xff\xff"sv,
                                 R"({"value":-2})");
                    pb_json_test(Test::Scalar::ReqSfixed32_8_2{.value = -1}, "\x0d\xff\xff\xff\xff"sv,
                                 R"({"value":-1})");
                    pb_json_test(Test::Scalar::ReqSfixed32_8_2{.value = 0}, "\x0d\x00\x00\x00\x00"sv,
                                 R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqSfixed32_8_2{.value = 1}, "\x0d\x01\x00\x00\x00"sv,
                                 R"({"value":1})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_8_2>("\x0d\xfd\xff\xff\xff"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_8_2>("\x0d\x02\x00\x00\x00"sv));
                }
                SUBCASE("required")
                {
                    CHECK(sizeof(Test::Scalar::ReqSfixed32_8::value) == sizeof(uint8_t));

                    pb_json_test(Test::Scalar::ReqSfixed32_8{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":66})");
                    pb_json_test(Test::Scalar::ReqSfixed32_8{.value = -2}, "\x0d\xfe\xff\xff\xff"sv,
                                 R"({"value":-2})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_8>("\x0d\xff\x00\x00\x00"sv));
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::ReqSfixed32_8>("\x0d\x42\x00\x00"sv));
                }
                SUBCASE("optional")
                {
                    pb_json_test(Test::Scalar::OptSfixed32_8{.value = 0x42}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":66})");
                    pb_json_test(Test::Scalar::OptSfixed32_8{.value = -2}, "\x0d\xfe\xff\xff\xff"sv,
                                 R"({"value":-2})");
                    CHECK_THROWS(
                        (void)spb::pb::deserialize<Test::Scalar::OptSfixed32_8>("\x0d\xff\x00\x00\x00"sv));
                }
                SUBCASE("repeated")
                {
                    pb_json_test(Test::Scalar::RepSfixed32_8{.value = {0x42}}, "\x0d\x42\x00\x00\x00"sv,
                                 R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepSfixed32_8{.value = {0x42, 0x3}},
                                 "\x0d\x42\x00\x00\x00\x0d\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepSfixed32_8{.value = {}}, "", "{}");

                    SUBCASE("packed")
                    {
                        pb_json_test(Test::Scalar::RepPackSfixed32_8{.value = {0x42}},
                                     "\x0a\x04\x42\x00\x00\x00"sv, R"({"value":[66]})");
                        pb_json_test(Test::Scalar::RepPackSfixed32_8{.value = {0x42, 0x3}},
                                     "\x0a\x08\x42\x00\x00\x00\x03\x00\x00\x00"sv, R"({"value":[66,3]})");
                        pb_json_test(Test::Scalar::RepPackSfixed32_8{.value = {}}, "", "{}");
                    }
                }
            }
        }
        SUBCASE("sfixed64")
        {
            SUBCASE("bitfield")
            {
                pb_json_test(Test::Scalar::ReqSfixed64_64_1{.value = -1},
                             "\x09\xff\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_1{.value = 0},
                             "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_1>(
                    "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_1>(
                    "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv));

                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = -2},
                             "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-2})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = -1},
                             "\x09\xff\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-1})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = 0},
                             "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                pb_json_test(Test::Scalar::ReqSfixed64_64_2{.value = 1},
                             "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":1})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_2>(
                    "\x09\xfd\xff\xff\xff\xff\xff\xff\xff"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_64_2>(
                    "\x09\x02\x00\x00\x00\x00\x00\x00\x00"sv));
            }
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqSfixed64{.value = 0x42},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::ReqSfixed64{.value = 0xff},
                             "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::ReqSfixed64{.value = -2}, "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv,
                             R"({"value":-2})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64>(
                    "\x09\x42\x00\x00\x00\x00\x00\x00"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptSfixed64{.value = 0x42},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                pb_json_test(Test::Scalar::OptSfixed64{.value = 0xff},
                             "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":255})");
                pb_json_test(Test::Scalar::OptSfixed64{.value = -2}, "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv,
                             R"({"value":-2})");
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepSfixed64{.value = {0x42}},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                pb_json_test(Test::Scalar::RepSfixed64{.value = {0x42, 0x3}},
                             "\x09\x42\x00\x00\x00\x00\x00\x00\x00\x09\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                             R"({"value":[66,3]})");
                pb_json_test(Test::Scalar::RepSfixed64{.value = {}}, "", "{}");
                SUBCASE("packed")
                {
                    pb_json_test(Test::Scalar::RepPackSfixed64{.value = {0x42}},
                                 "\x0a\x08\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepPackSfixed64{.value = {0x42, 0x3}},
                                 "\x0a\x10\x42\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                                 R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepPackSfixed64{.value = {}}, "", "{}");
                }
            }
            SUBCASE("int8")
            {
                SUBCASE("bitfield")
                {
                    pb_json_test(Test::Scalar::ReqSfixed64_8_1{.value = -1},
                                 "\x09\xff\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-1})");
                    pb_json_test(Test::Scalar::ReqSfixed64_8_1{.value = 0},
                                 "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_8_1>(
                        "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_8_1>(
                        "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv));

                    pb_json_test(Test::Scalar::ReqSfixed64_8_2{.value = -2},
                                 "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-2})");
                    pb_json_test(Test::Scalar::ReqSfixed64_8_2{.value = -1},
                                 "\x09\xff\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-1})");
                    pb_json_test(Test::Scalar::ReqSfixed64_8_2{.value = 0},
                                 "\x09\x00\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":0})");
                    pb_json_test(Test::Scalar::ReqSfixed64_8_2{.value = 1},
                                 "\x09\x01\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":1})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_8_2>(
                        "\x09\xfd\xff\xff\xff\xff\xff\xff\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_8_2>(
                        "\x09\x02\x00\x00\x00\x00\x00\x00\x00"sv));
                }
                SUBCASE("required")
                {
                    CHECK(sizeof(Test::Scalar::ReqSfixed64_8::value) == sizeof(uint8_t));

                    pb_json_test(Test::Scalar::ReqSfixed64_8{.value = 0x42},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                    pb_json_test(Test::Scalar::ReqSfixed64_8{.value = -2},
                                 "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-2})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_8>(
                        "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqSfixed64_8>(
                        "\x09\x42\x00\x00\x00\x00\x00\x00"sv));
                }
                SUBCASE("optional")
                {
                    pb_json_test(Test::Scalar::OptSfixed64_8{.value = 0x42},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":66})");
                    pb_json_test(Test::Scalar::OptSfixed64_8{.value = -2},
                                 "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv, R"({"value":-2})");
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptSfixed64_8>(
                        "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv));
                }
                SUBCASE("repeated")
                {
                    pb_json_test(Test::Scalar::RepSfixed64_8{.value = {0x42}},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                    pb_json_test(Test::Scalar::RepSfixed64_8{.value = {0x42, 0x3}},
                                 "\x09\x42\x00\x00\x00\x00\x00\x00\x00\x09\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                                 R"({"value":[66,3]})");
                    pb_json_test(Test::Scalar::RepSfixed64_8{.value = {}}, "", "{}");
                    SUBCASE("packed")
                    {
                        pb_json_test(Test::Scalar::RepPackSfixed64_8{.value = {0x42}},
                                     "\x0a\x08\x42\x00\x00\x00\x00\x00\x00\x00"sv, R"({"value":[66]})");
                        pb_json_test(
                            Test::Scalar::RepPackSfixed64_8{.value = {0x42, 0x3}},
                            "\x0a\x10\x42\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"sv,
                            R"({"value":[66,3]})");
                        pb_json_test(Test::Scalar::RepPackSfixed64_8{.value = {}}, "", "{}");
                    }
                }
            }
        }
    }
    SUBCASE("float")
    {
        SUBCASE("required")
        {
            pb_json_test(Test::Scalar::ReqFloat{.value = 42.0f}, "\x0d\x00\x00\x28\x42"sv, R"({"value":42})");
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqFloat>("\x0d\x00\x00\x28"sv));
        }
        SUBCASE("optional")
        {
            pb_json_test(Test::Scalar::OptFloat{.value = 42.3f}, "\x0d\x33\x33\x29\x42"sv,
                         R"({"value":42.3})");
        }
        SUBCASE("repeated")
        {
            pb_json_test(Test::Scalar::RepFloat{.value = {42.3f}}, "\x0d\x33\x33\x29\x42"sv,
                         R"({"value":[42.3]})");
            pb_json_test(Test::Scalar::RepFloat{.value = {42.0f, 42.3f}},
                         "\x0d\x00\x00\x28\x42\x0d\x33\x33\x29\x42"sv, R"({"value":[42,42.3]})");
            pb_json_test(Test::Scalar::RepFloat{.value = {}}, "", "{}");
        }
    }
    SUBCASE("double")
    {
        SUBCASE("required")
        {
            pb_json_test(Test::Scalar::ReqDouble{.value = 42.0}, "\x09\x00\x00\x00\x00\x00\x00\x45\x40"sv,
                         R"({"value":42})");
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqDouble>("\x0d\x00\x00\x28"sv));
        }
        SUBCASE("optional")
        {
            pb_json_test(Test::Scalar::OptDouble{.value = 42.3}, "\x09\x66\x66\x66\x66\x66\x26\x45\x40"sv,
                         R"({"value":42.3})");
        }
        SUBCASE("repeated")
        {
            pb_json_test(Test::Scalar::RepDouble{.value = {42.3}}, "\x09\x66\x66\x66\x66\x66\x26\x45\x40"sv,
                         R"({"value":[42.3]})");
            pb_json_test(Test::Scalar::RepDouble{.value = {42.3, 3.0}},
                         "\x09\x66\x66\x66\x66\x66\x26\x45\x40\x09\x00\x00\x00\x00\x00\x00\x08\x40"sv,
                         R"({"value":[42.3,3]})");
            pb_json_test(Test::Scalar::RepDouble{.value = {}}, "", "{}");
        }
    }
    SUBCASE("bytes")
    {
        SUBCASE("required")
        {
            pb_json_test(Test::Scalar::ReqBytes{}, "\x0a\x00"sv, "{}");
            pb_json_test(Test::Scalar::ReqBytes{.value = to_bytes("hello")}, "\x0a\x05hello"sv,
                         R"({"value":"aGVsbG8="})");
            pb_json_test(Test::Scalar::ReqBytes{.value = to_bytes("\x00\x01\x02"sv)},
                         "\x0a\x03\x00\x01\x02"sv, R"({"value":"AAEC"})");
            pb_json_test(Test::Scalar::ReqBytes{.value = to_bytes("\x00\x01\x02\x03\x04"sv)},
                         "\x0a\x05\x00\x01\x02\x03\x04"sv, R"({"value":"AAECAwQ="})");
            CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqBytes>("\x0a\x05hell"sv));
        }
        SUBCASE("optional")
        {
            pb_json_test(Test::Scalar::OptBytes{}, "", "{}");
            pb_json_test(Test::Scalar::OptBytes{.value = to_bytes("hello")}, "\x0a\x05hello"sv,
                         R"({"value":"aGVsbG8="})");
            pb_json_test(Test::Scalar::OptBytes{.value = to_bytes("\x00\x01\x02"sv)},
                         "\x0a\x03\x00\x01\x02"sv, R"({"value":"AAEC"})");
            pb_json_test(Test::Scalar::OptBytes{.value = to_bytes("\x00\x01\x02\x03\x04"sv)},
                         "\x0a\x05\x00\x01\x02\x03\x04"sv, R"({"value":"AAECAwQ="})");
        }
        SUBCASE("repeated")
        {
            pb_json_test(Test::Scalar::RepBytes{}, "", "{}");
            pb_json_test(Test::Scalar::RepBytes{.value = {to_bytes("hello")}}, "\x0a\x05hello"sv,
                         R"({"value":["aGVsbG8="]})");
            pb_json_test(Test::Scalar::RepBytes{.value = {to_bytes("\x00\x01\x02"sv), to_bytes("hello")}},
                         "\x0a\x03\x00\x01\x02\x0a\x05hello"sv, R"({"value":["AAEC","aGVsbG8="]})");
            pb_json_test(Test::Scalar::RepBytes{.value = {to_bytes("\x00\x01\x02\x03\x04"sv)}},
                         "\x0a\x05\x00\x01\x02\x03\x04"sv, R"({"value":["AAECAwQ="]})");
        }
        SUBCASE("fixed")
        {
            SUBCASE("required")
            {
                pb_json_test(Test::Scalar::ReqBytesFixed{}, "\x0a\x08\x00\x00\x00\x00\x00\x00\x00\x00"sv,
                             R"({"value":"AAAAAAAAAAA="})");
                pb_json_test(Test::Scalar::ReqBytesFixed{.value = to_array("hello123")}, "\x0a\x08hello123"sv,
                             R"({"value":"aGVsbG8xMjM="})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqBytesFixed>("\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::ReqBytesFixed>("\x0a\x09hello1234"sv));
            }
            SUBCASE("optional")
            {
                pb_json_test(Test::Scalar::OptBytesFixed{}, "", "{}");
                pb_json_test(Test::Scalar::OptBytesFixed{.value = to_array("hello123")}, "\x0a\x08hello123"sv,
                             R"({"value":"aGVsbG8xMjM="})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptBytesFixed>("\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptBytesFixed>("\x0a\x09hello1234"sv));
            }
            SUBCASE("repeated")
            {
                pb_json_test(Test::Scalar::RepBytesFixed{}, "", "{}");
                pb_json_test(Test::Scalar::RepBytesFixed{.value = {to_array("hello123")}},
                             "\x0a\x08hello123"sv, R"({"value":["aGVsbG8xMjM="]})");
                pb_json_test(
                    Test::Scalar::RepBytesFixed{.value = {to_array("hello123"), to_array("hello321")}},
                    "\x0a\x08hello123\x0a\x08hello321"sv, R"({"value":["aGVsbG8xMjM=","aGVsbG8zMjE="]})");
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepBytesFixed>("\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::RepBytesFixed>("\x0a\x09hello1234"sv));
            }
        }
    }
    SUBCASE("variant")
    {
        SUBCASE("int")
        {
            pb_json_test(Test::Variant{.oneof_field = 0x42U}, "\x08\x42", R"({"var_int":66})");
        }
        SUBCASE("string")
        {
            pb_json_test(Test::Variant{.oneof_field = "hello"}, "\x12\x05hello", R"({"var_string":"hello"})");
        }
        SUBCASE("bytes")
        {
            pb_json_test(Test::Variant{.oneof_field = to_bytes("hello")}, "\x1A\x05hello",
                         R"({"var_bytes":"aGVsbG8="})");
        }
        SUBCASE("name")
        {
            pb_json_test(Test::Variant{.oneof_field = Test::Name{.name = "John"}}, "\x22\x06\x0A\x04John",
                         R"({"name":{"name":"John"}})");
        }
    }
    SUBCASE("map")
    {
        SUBCASE("int32/int32")
        {
            CHECK(std::is_same_v<decltype(UnitTest::map::Int32Int32::map), std::map<int32_t, int32_t>>);

            pb_json_test(UnitTest::map::Int32Int32{.map = {{1, 2}}}, "\x0a\x04\x08\x01\x10\x02",
                         R"({"map":{"1":2}})");
            pb_json_test(UnitTest::map::Int32Int32{.map = {{1, 2}, {2, 3}}},
                         "\x0a\x04\x08\x01\x10\x02\x0a\x04\x08\x02\x10\x03", R"({"map":{"1":2,"2":3}})");
        }
        SUBCASE("string/string")
        {
            pb_json_test(UnitTest::map::StringString{.map = {{"hello", "world"}}},
                         "\x0a\x0e\x0a\x05hello\x12\x05world", R"({"map":{"hello":"world"}})");
            pb_json_test(UnitTest::map::StringString{.map = {{"hello", "world"}, {"name", "john"}}},
                         "\x0a\x0e\x0a\x05hello\x12\x05world\x0a\x0c\x0a\x04name\x12\x04john",
                         R"({"map":{"hello":"world","name":"john"}})");
            pb_json_test(UnitTest::map::StringString{.map = {{"hello", ""}}},
                         "\x0a\x09\x0a\x05hello\x12\x00"sv, R"({"map":{"hello":""}})");
        }
        SUBCASE("int32/string")
        {
            pb_json_test(UnitTest::map::Int32String{.map = {{1, "hello"}}}, "\x0a\x09\x08\x01\x12\x05hello",
                         R"({"map":{"1":"hello"}})");
        }
        SUBCASE("string/int32")
        {
            pb_json_test(UnitTest::map::StringInt32{.map = {{"hello", 2}}}, "\x0a\x09\x0a\x05hello\x10\x02",
                         R"({"map":{"hello":2}})");
        }
        SUBCASE("string/name")
        {
            pb_json_test(UnitTest::map::StringName{.map = {{"hello", {.name = "john"}}}},
                         "\x0a\x0f\x0a\x05hello\x12\x06\x0A\x04john", R"({"map":{"hello":{"name":"john"}}})");
        }
        SUBCASE("options")
        {
            CHECK(std::is_same_v<decltype(UnitTest::map::UnorderedInt32Int32::map),
                                 std::unordered_map<int32_t, int32_t>>);
            pb_json_test(UnitTest::map::UnorderedInt32Int32{.map = {{1, 2}}}, "\x0a\x04\x08\x01\x10\x02",
                         R"({"map":{"1":2}})");
        }
    }
    SUBCASE("empty")
    {
        pb_json_test(Test::Scalar::Empty{}, ""sv, "{}");

        SUBCASE("required")
        {
            pb_json_test(Test::Scalar::ReqEmpty{}, "\x0a\x00"sv, R"({"value":{}})");
        }
        SUBCASE("optional")
        {
            pb_json_test(Test::Scalar::OptEmpty{}, "", "{}");
            pb_json_test(Test::Scalar::OptEmpty{.value = Test::Scalar::Empty{}}, "\x0a\x00"sv,
                         R"({"value":{}})");
        }
        SUBCASE("repeated")
        {
            pb_json_test(Test::Scalar::RepEmpty{}, ""sv, "{}");
            pb_json_test(Test::Scalar::RepEmpty{.value = {Test::Scalar::Empty{}}}, "\x0a\x00"sv,
                         R"({"value":[{}]})");
            pb_json_test(Test::Scalar::RepEmpty{.value = {Test::Scalar::Empty{}, Test::Scalar::Empty{}}},
                         "\x0a\x00\x0a\x00"sv, R"({"value":[{},{}]})");
        }
    }
    SUBCASE("person")
    {
        pb_json_test(
            PhoneBook::Person{
                .name   = "John Doe",
                .id     = 123,
                .email  = "QXUeh@example.com",
                .phones = {PhoneBook::Person::PhoneNumber{
                    .number = "555-4321",
                    .type   = PhoneBook::Person::PhoneType::HOME,
                }},
            },
            "\x0a\x08John Doe\x10\x7b\x1a\x11QXUeh@example.com\x22\x0c\x0A\x08"
            "555-4321\x10\x01",
            R"({"name":"John Doe","id":123,"email":"QXUeh@example.com","phones":[{"number":"555-4321","type":"HOME"}]})");
        CHECK_THROWS((void)spb::pb::deserialize<PhoneBook::Person>(
            "\x0a\x08John Doe\x10\x7b\x1a\x11QXUeh@example.com\x22\x0d\x0A\x08"
            "555-4321\x10\x010\x00"sv));
    }
    SUBCASE("name")
    {
        pb_json_test(Test::Name{}, "", "{}");
    }
    SUBCASE("reserved")
    {
        pb_json_test(reserved::public_{.p = reserved::public_::private_{.for_ = 3, .bool_ = true}},
                     "\x0a\x04\x08\x03\x10\x01", R"({"p":{"for":3,"bool":true}})");
    }
    SUBCASE("simd")
    {
        auto data           = UnitTest::simd::Data{};
        data.coordinates[0] = 0;
        data.coordinates[1] = 1;
        data.coordinates[2] = 2;
        data.coordinates[3] = 3;
        pb_json_test(data, "\x0a\x10\x00\x00\x00\x00\x00\x00\x80\x3f\x00\x00\x00\x40\x00\x00\x40\x40"sv,
                     R"({"coordinates":[0,1,2,3]})");
        CHECK_THROWS((void)spb::pb::deserialize<UnitTest::simd::Data>(
            "\x0a\x0c\x00\x00\x00\x00\x00\x00\x80\x3f\x00\x00\x00\x40"sv));
        CHECK_THROWS((void)spb::pb::deserialize<UnitTest::simd::Data>(
            "\x0a\x11\x00\x00\x00\x00\x00\x00\x80\x3f\x00\x00\x00\x40\x00\x00\x40\x40\x50"sv));
        CHECK_THROWS((void)spb::json::deserialize<UnitTest::simd::Data>(R"({"coordinates":[0,1,2]})"sv));
        CHECK_THROWS((void)spb::json::deserialize<UnitTest::simd::Data>(R"({"coordinates":[0,1,2,3,4]})"sv));
    }
    SUBCASE("fixed size array")
    {
        pb_json_test(UnitTest::array::Data{.words = {0, 1, 2, 3}}, "\x0a\x04\x00\x01\x02\x03"sv,
                     R"({"words":[0,1,2,3]})");
        CHECK_THROWS((void)spb::pb::deserialize<UnitTest::array::Data>("\x0a\x03\x00\x01\x02"sv));
        CHECK_THROWS((void)spb::pb::deserialize<UnitTest::array::Data>("\x0a\x05\x00\x01\x02\x03\x04"sv));
        CHECK_THROWS((void)spb::json::deserialize<UnitTest::array::Data>(R"({"words":[0,1,2]})"sv));
        CHECK_THROWS((void)spb::json::deserialize<UnitTest::array::Data>(R"({"words":[0,1,2,3,4]})"sv));
    }
    SUBCASE("ignore")
    {
        SUBCASE("string")
        {
            SUBCASE("required")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x05hello"sv) ==
                      Test::Scalar::Simple{});
                CHECK_NOTHROW((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x05hell"sv));
            }
            SUBCASE("repeated")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x05hello\x0a\x05world"sv) ==
                      Test::Scalar::Simple{});
                CHECK_NOTHROW(
                    (void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x05hello\x0a\x05world"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x05hello\x0a\x05worl"sv));
            }
        }
        SUBCASE("bool")
        {
            SUBCASE("required")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x01"sv) == Test::Scalar::Simple{});
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x00"sv) == Test::Scalar::Simple{});
            }
            SUBCASE("repeated")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x01\x08\x00"sv) ==
                      Test::Scalar::Simple{});
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08\x01\x08"sv));
            }
        }
        SUBCASE("int")
        {
            SUBCASE("varint32")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x42"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\xff\x01"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\xfe\xff\xff\xff\x0f"sv) ==
                          Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                        "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x42\x08\x03"sv) ==
                          Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x01\x42"sv) ==
                              Test::Scalar::Simple{});
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x02\x42\x03"sv) ==
                              Test::Scalar::Simple{});
                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x02\x42"sv));
                    }
                }
            }
            SUBCASE("varint64")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x42"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\xff\x01"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x08\xfe\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv) == Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                        "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x42\x08\x03"sv) ==
                          Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x01\x42"sv) ==
                              Test::Scalar::Simple{});
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x02\x42\x03"sv) ==
                              Test::Scalar::Simple{});
                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x02\x42"sv));
                    }
                }
            }
            SUBCASE("svarint32")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x84\x01"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\xfe\x03"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x03"sv) == Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                        "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x84\x01"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x84\x01\x08\x03"sv) ==
                          Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x02\x84\x01"sv) ==
                              Test::Scalar::Simple{});
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x03\x84\x01\x03"sv) ==
                              Test::Scalar::Simple{});
                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x02\x42"sv));
                    }
                }
            }
            SUBCASE("svarint64")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x84\x01"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\xfe\x03"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x03"sv) == Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x08\xff"sv));
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                        "\x08\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x84\x01"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\x84\x01\x08\x03"sv) ==
                          Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x02\x84\x01"sv) ==
                              Test::Scalar::Simple{});
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x03\x84\x01\x03"sv) ==
                              Test::Scalar::Simple{});

                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x02\x84"sv));
                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x03\x84\x01"sv));
                    }
                }
            }
            SUBCASE("fixed32")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\x42\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\xff\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\xfe\xff\xff\xff"sv) ==
                          Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0d\x42\x00\x00"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\x42\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x0d\x42\x00\x00\x00\x0d\x03\x00\x00\x00"sv) == Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x04\x42\x00\x00\x00"sv) ==
                              Test::Scalar::Simple{});
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                                  "\x0a\x08\x42\x00\x00\x00\x03\x00\x00\x00"sv) == Test::Scalar::Simple{});
                        CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                            "\x0a\x08\x42\x00\x00\x00\x03\x00\x00"sv));
                    }
                }
            }
            SUBCASE("fixed64")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv) == Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                        "\x09\x42\x00\x00\x00\x00\x00\x00"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\x42\x00\x00\x00\x00\x00\x00\x00\x09\x03\x00\x00\x00\x00\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                                  "\x0a\x08\x42\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                        CHECK(
                            spb::pb::deserialize<Test::Scalar::Simple>(
                                "\x0a\x10\x42\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"sv) ==
                            Test::Scalar::Simple{});
                    }
                }
            }
            SUBCASE("sfixed32")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\x42\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\xff\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\xfe\xff\xff\xff"sv) ==
                          Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0d\x42\x00\x00"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\x42\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x0d\x42\x00\x00\x00\x0d\x03\x00\x00\x00"sv) == Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x04\x42\x00\x00\x00"sv) ==
                              Test::Scalar::Simple{});
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                                  "\x0a\x08\x42\x00\x00\x00\x03\x00\x00\x00"sv) == Test::Scalar::Simple{});
                    }
                }
            }
            SUBCASE("sfixed64")
            {
                SUBCASE("required")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\xff\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\xfe\xff\xff\xff\xff\xff\xff\xff"sv) == Test::Scalar::Simple{});
                    CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                        "\x09\x42\x00\x00\x00\x00\x00\x00"sv));
                }
                SUBCASE("repeated")
                {
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\x42\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                    CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                              "\x09\x42\x00\x00\x00\x00\x00\x00\x00\x09\x03\x00\x00\x00\x00\x00\x00\x00"sv) ==
                          Test::Scalar::Simple{});

                    SUBCASE("packed")
                    {
                        CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                                  "\x0a\x08\x42\x00\x00\x00\x00\x00\x00\x00"sv) == Test::Scalar::Simple{});
                        CHECK(
                            spb::pb::deserialize<Test::Scalar::Simple>(
                                "\x0a\x10\x42\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00"sv) ==
                            Test::Scalar::Simple{});
                    }
                }
            }
        }
        SUBCASE("float")
        {
            SUBCASE("required")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\x00\x00\x28\x42"sv) ==
                      Test::Scalar::Simple{});
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0d\x00\x00\x28"sv));
            }
            SUBCASE("repeated")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0d\x33\x33\x29\x42"sv) ==
                      Test::Scalar::Simple{});
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                          "\x0d\x00\x00\x28\x42\x0d\x33\x33\x29\x42"sv) == Test::Scalar::Simple{});
            }
        }
        SUBCASE("double")
        {
            SUBCASE("required")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x09\x00\x00\x00\x00\x00\x00\x45\x40"sv) ==
                      Test::Scalar::Simple{});
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0d\x00\x00\x28"sv));
            }
            SUBCASE("optional")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x09\x66\x66\x66\x66\x66\x26\x45\x40"sv) ==
                      Test::Scalar::Simple{});
            }
            SUBCASE("repeated")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x09\x66\x66\x66\x66\x66\x26\x45\x40"sv) ==
                      Test::Scalar::Simple{});
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                          "\x09\x66\x66\x66\x66\x66\x26\x45\x40\x09\x00\x00\x00\x00\x00\x00\x08\x40"sv) ==
                      Test::Scalar::Simple{});
            }
        }
        SUBCASE("bytes")
        {
            SUBCASE("required")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x05hello"sv) ==
                      Test::Scalar::Simple{});
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x03\x00\x01\x02"sv) ==
                      Test::Scalar::Simple{});
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x05\x00\x01\x02\x03\x04"sv) ==
                      Test::Scalar::Simple{});
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0a\x05hell"sv));
            }
            SUBCASE("repeated")
            {
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x05hello"sv) ==
                      Test::Scalar::Simple{});
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x03\x00\x01\x02"sv) ==
                      Test::Scalar::Simple{});
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x05\x00\x01\x02\x03\x04"sv) ==
                      Test::Scalar::Simple{});
            }
        }
        SUBCASE("struct")
        {
            CHECK(spb::pb::deserialize<Test::Scalar::Simple>(
                      "\x0a\x08John Doe\x10\x7b\x1a\x11QXUeh@example.com\x22\x0c\x0A\x08"
                      "555-4321\x10\x01"sv) == Test::Scalar::Simple{});
            CHECK_NOTHROW((void)spb::pb::deserialize<Test::Scalar::Empty>(
                "\x0a\x08John Doe\x10\x7b\x1a\x11QXUeh@example.com\x22\x0c\x0A\x08"
                "555-4321\x10\x01"sv));
        }
        SUBCASE("invalid")
        {
            SUBCASE("wire type")
            {
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptInt32>("\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptInt32>("\x0d\x00\x00\x28\x42"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::OptString>("\x0d\x00\x00\x28\x42"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x0f\x05hello"sv));
            }
            SUBCASE("stream")
            {
                CHECK_THROWS((void)spb::pb::deserialize<Test::Recursive>("\x0a\x05\x0a\x05hello"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Recursive>("\x0a\x08\x0a\x05hello"sv));
            }
            SUBCASE("tag")
            {
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>(
                    "\xff\xff\xff\xff\xff\xff\xff\xff\x01"sv));
                CHECK_THROWS((void)spb::pb::deserialize<Test::Scalar::Empty>("\x07\x05hello"sv));
            }
        }
        SUBCASE("options")
        {
            SUBCASE("delimited")
            {
                CHECK(spb::pb::serialize(Test::Scalar::ReqString{.value = "hello"}, {.delimited = true}) ==
                      "\x07\x0a\x05hello");
                CHECK(spb::pb::serialize(Test::Scalar::Simple{.value = "hello"}, {.delimited = true}) ==
                      "\x08\xA2\x06\x05hello");
                CHECK(spb::pb::deserialize<Test::Scalar::Simple>("\x08\xA2\x06\x05hello"sv,
                                                                 {.delimited = true}) ==
                      Test::Scalar::Simple{.value = "hello"});
                CHECK_THROWS(
                    (void)spb::pb::deserialize<Test::Scalar::Simple>("\x0a\x05hello"sv, {.delimited = true}));
            }
        }
    }
}
