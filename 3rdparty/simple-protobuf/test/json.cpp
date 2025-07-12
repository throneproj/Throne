#include "spb/json.hpp"
#include <cstdint>
#include <memory>
#include <name.pb.h>
#include <person.pb.h>
#include <scalar.pb.h>
#include <spb/json/deserialize.hpp>
#include <spb/json/serialize.hpp>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace std
{
auto operator==( const std::span< const std::byte > & lhs,
                 const std::span< const std::byte > & rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0;
}
}// namespace std

namespace
{
template < typename T >
concept HasValueMember = requires( T t ) {
    { t.value };
};

template < size_t N >
auto to_array( const char ( &string )[ N ] )
{
    auto result = std::array< std::byte, N - 1 >( );
    memcpy( result.data( ), &string, result.size( ) );
    return result;
}

auto to_bytes( std::string_view str ) -> std::vector< std::byte >
{
    auto span = std::span< std::byte >( ( std::byte * ) str.data( ), str.size( ) );
    return { span.data( ), span.data( ) + span.size( ) };
}

template < typename T >
void json_test( const T & value, std::string_view json )
{
    {
        auto serialized = spb::json::serialize( value );
        CHECK( serialized == json );
        auto size = spb::json::serialize_size( value );
        CHECK( size == json.size( ) );
    }

    {
        auto deserialized = spb::json::deserialize< T >( json );
        if constexpr( HasValueMember< T > )
        {
            using valueT = decltype( T::value );
            CHECK( valueT( deserialized.value ) == valueT( value.value ) );
        }
        else
        {
            CHECK( deserialized == value );
        }
    }
    {
        auto deserialized = T( );
        spb::json::deserialize( deserialized, json );
        if constexpr( HasValueMember< T > )
        {
            using valueT = decltype( T::value );
            CHECK( valueT( deserialized.value ) == valueT( value.value ) );
        }
        else
        {
            CHECK( deserialized == value );
        }
    }
}

}// namespace

namespace Test
{
auto operator==( const Test::Name & lhs, const Test::Name & rhs ) noexcept -> bool
{
    return lhs.name == rhs.name && lhs.bkfvdzz == rhs.bkfvdzz;
}
}// namespace Test

using namespace std::literals;

TEST_CASE( "json" )
{
    SUBCASE( "dj2hash" )
    {
        const auto hash           = spb::json::detail::djb2_hash( "hello" );
        const auto collision      = spb::json::detail::djb2_hash( "narpjy" );
        const auto hash2          = spb::json::detail::djb2_hash( "world" );
        const auto name_hash      = spb::json::detail::djb2_hash( "name" );
        const auto name_collision = spb::json::detail::djb2_hash( "bkfvdzz" );
        const auto hash3          = spb::json::detail::djb2_hash( { } );
        CHECK( hash == collision );
        CHECK( name_hash == name_collision );
        CHECK( hash3 != 0 );
        CHECK( hash != 0 );
        CHECK( hash2 != 0 );
        CHECK( hash != hash2 );
        CHECK( hash3 != hash2 );
        CHECK( hash3 != hash );
    }
    SUBCASE( "deserialize" )
    {
        SUBCASE( "ignore" )
        {
            SUBCASE( "empty" )
            {
                CHECK( spb::json::deserialize< Test::Name >( R"({})"sv ) == Test::Name{ } );
                CHECK(
                    spb::json::deserialize< Test::Name >(
                        R"({                                                                                                                                                                                                                                                                                                                                                                                                                })"sv ) ==
                    Test::Name{ } );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >( ""sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >( R"(})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >( R"({)"sv ) );
            }
            SUBCASE( "string" )
            {
                CHECK( spb::json::deserialize< Test::Name >( R"({"string":"string"})"sv ) ==
                       Test::Name{ } );
                CHECK( spb::json::deserialize< Test::Name >( R"({"bkfvdzz":"string"})"sv ) ==
                       Test::Name{ .bkfvdzz = "string" } );
                CHECK(
                    spb::json::deserialize< Test::Name >(
                        R"({"string                                                                                                                                                                                                                                                                                                                                                                                                                ":"string                                                                                                                                                                                                                                                                                                                                                                                                                "})"sv ) ==
                    Test::Name{ } );
            }
            SUBCASE( "int" )
            {
                CHECK( spb::json::deserialize< Test::Name >( R"({"integer":42})"sv ) ==
                       Test::Name{ } );
                CHECK( spb::json::deserialize< Test::Name >( R"({"bkfvdzz1":42})"sv ) ==
                       Test::Name{ } );
            }
            SUBCASE( "float" )
            {
                CHECK( spb::json::deserialize< Test::Name >( R"({"fl":42.0, "fl2":0})"sv ) ==
                       Test::Name{ } );
            }
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::deserialize< Test::Name >( R"({"repeated":[42,"hello"]})"sv ) ==
                       Test::Name{ } );
                CHECK( spb::json::deserialize< Test::Name >( R"({"repeated":[]})"sv ) ==
                       Test::Name{ } );
                CHECK( spb::json::deserialize< Test::Name >( R"({"name1":[]})"sv ) ==
                       Test::Name{ } );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< Test::Name >( R"({"repeated":[})"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< Test::Name >( R"({"repeated":[)"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< Test::Name >( R"({"repeated":[42)"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< Test::Name >( R"({"repeated":[42,)"sv ) );
            }
            SUBCASE( "bool" )
            {
                CHECK( spb::json::deserialize< Test::Name >(
                           R"({"value":true, "value2":false})"sv ) == Test::Name{ } );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >(
                    R"({"value":tru, "value2":false})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >(
                    R"({"value":true, "value2":fals})"sv ) );
            }
            SUBCASE( "null" )
            {
                CHECK( spb::json::deserialize< Test::Name >( R"({"value":null})"sv ) ==
                       Test::Name{ } );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< Test::Name >( R"({"value":nul})"sv ) );
            }
            SUBCASE( "object" )
            {
                CHECK( spb::json::deserialize< Test::Name >( R"({"value":{}})"sv ) ==
                       Test::Name{ } );
                CHECK( spb::json::deserialize< Test::Name >(
                           R"({"value":{"key":"value", "key2":[42]}})"sv ) == Test::Name{ } );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >( R"({"value"})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >(
                    R"({"value":{"key":"value", "key2":[42]})"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< Test::Name >( R"({"value":{"key":}})"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< Test::Name >( R"({"value":{"key"}})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >(
                    R"({"value":{"key":"value")"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Name >(
                    R"({"value":{"key":"value",)"sv ) );
            }
        }
        SUBCASE( "person" )
        {
            constexpr std::string_view jsons[] = {
                R"({"name": "John Doe","id": 123,"email": "QXUeh@example.com", "phones": [ {"number": "555-4321","type": "HOME"}]})",
                R"({"name": "John Doe","id": 123,"email": "QXUeh@example.com", "phones": [ {"number": "555-4321","type": 1}]})",
                R"({"name2": "Jack","name": "John Doe","id": 123,"email": "QXUeh@example.com", "phones": [ {"number": "555-4321","type": "HOME"}]})",
            };
            for( auto json : jsons )
            {
                const auto person = spb::json::deserialize< PhoneBook::Person >( json );
                CHECK( person.name == "John Doe" );
                CHECK( person.id == 123 );
                CHECK( person.email == "QXUeh@example.com" );
                CHECK( person.phones.size( ) == 1 );
                CHECK( person.phones[ 0 ].number == "555-4321" );
                CHECK( person.phones[ 0 ].type == PhoneBook::Person::PhoneType::HOME );
            }
        }
        SUBCASE( "enum" )
        {
            CHECK( spb::json::deserialize< PhoneBook::Person::PhoneType >( "\"HOME\""sv ) ==
                   PhoneBook::Person::PhoneType::HOME );
            CHECK( Test::Scalar::ReqEnumAlias::Enum::EAA_STARTED ==
                   Test::Scalar::ReqEnumAlias::Enum::EAA_RUNNING );
            CHECK( spb::json::deserialize< Test::Scalar::ReqEnumAlias::Enum >(
                       "\"EAA_STARTED\""sv ) == Test::Scalar::ReqEnumAlias::Enum::EAA_STARTED );
            CHECK( spb::json::deserialize< Test::Scalar::ReqEnumAlias::Enum >(
                       "\"EAA_STARTED\""sv ) == Test::Scalar::ReqEnumAlias::Enum::EAA_RUNNING );
            CHECK( spb::json::deserialize< Test::Scalar::ReqEnumAlias::Enum >(
                       "\"EAA_RUNNING\""sv ) == Test::Scalar::ReqEnumAlias::Enum::EAA_STARTED );
            CHECK( spb::json::deserialize< Test::Scalar::ReqEnumAlias::Enum >(
                       "\"EAA_RUNNING\""sv ) == Test::Scalar::ReqEnumAlias::Enum::EAA_RUNNING );
            REQUIRE_THROWS(
                ( void ) spb::json::deserialize< PhoneBook::Person::PhoneType >( "3"sv ) );
            REQUIRE_THROWS(
                ( void ) spb::json::deserialize< PhoneBook::Person::PhoneType >( "HME"sv ) );
        }
        SUBCASE( "bool" )
        {
            {
                CHECK( spb::json::deserialize< bool >( "true"sv ) == true );
                CHECK( spb::json::deserialize< bool >( "false"sv ) == false );
                CHECK_THROWS( ( void ) spb::json::deserialize< bool >( "hello"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< bool >( "true1"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< bool >( "true_"sv ) );
                auto value = false;
                CHECK_NOTHROW( spb::json::deserialize( value, "true"sv ) );
                CHECK( value );
                CHECK_NOTHROW( spb::json::deserialize( value, "false"sv ) );
                CHECK( value == false );
            }
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::deserialize< std::vector< bool > >( R"([true,false])"sv ) ==
                       std::vector< bool >{ true, false } );
                CHECK( spb::json::deserialize< std::vector< bool > >( R"([])"sv ) ==
                       std::vector< bool >{ } );
                CHECK( spb::json::deserialize< std::vector< bool > >( R"(null)"sv ) ==
                       std::vector< bool >{ } );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< bool > >( R"()"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::vector< bool > >( R"(true)"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::vector< bool > >( R"([null])"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::vector< bool > >( R"("hello")"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< bool > >( R"([)"sv ) );
            }
            SUBCASE( "optional" )
            {
                CHECK( spb::json::deserialize< std::optional< bool > >( "true"sv ) ==
                       std::optional< bool >( true ) );
                CHECK( spb::json::deserialize< std::optional< bool > >( "false"sv ) ==
                       std::optional< bool >( false ) );
                CHECK( spb::json::deserialize< std::optional< bool > >( "null"sv ) ==
                       std::optional< bool >( ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< bool > >( "hello"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::optional< bool > >( R"()"sv ) );

                auto value = std::optional< bool >( );
                CHECK_NOTHROW( spb::json::deserialize( value, "true"sv ) );
                CHECK( value == true );
                CHECK_NOTHROW( spb::json::deserialize( value, "false"sv ) );
                CHECK( value == false );
            }
            SUBCASE( "ptr" )
            {
                CHECK( *spb::json::deserialize< std::unique_ptr< bool > >( "true"sv ) == true );
                CHECK( *spb::json::deserialize< std::unique_ptr< bool > >( "false"sv ) == false );
                CHECK( spb::json::deserialize< std::unique_ptr< bool > >( "null"sv ) ==
                       std::unique_ptr< bool >( ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::unique_ptr< bool > >( "hello"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::unique_ptr< bool > >( R"()"sv ) );

                auto value = std::unique_ptr< bool >( );
                CHECK_NOTHROW( spb::json::deserialize( value, "true"sv ) );
                CHECK( *value == true );
                CHECK_NOTHROW( spb::json::deserialize( value, "false"sv ) );
                CHECK( *value == false );
            }
        }
        SUBCASE( "float" )
        {
            CHECK( spb::json::deserialize< float >( "0"sv ) == 0 );
            CHECK( spb::json::deserialize< float >( "42"sv ) == 42 );
            CHECK( spb::json::deserialize< float >( "3.14"sv ) == 3.14f );
            CHECK( spb::json::deserialize< float >( "0.0"sv ) == 0.0f );
            CHECK( spb::json::deserialize< float >( "-0.0"sv ) == -0.0f );
            CHECK( spb::json::deserialize< float >( "-3.14"sv ) == -3.14f );
            CHECK( spb::json::deserialize< float >( "3.14e+10"sv ) == 3.14e+10f );
            CHECK( spb::json::deserialize< float >( "3.14e-10"sv ) == 3.14e-10f );
            CHECK( spb::json::deserialize< float >( "3.14E+10"sv ) == 3.14E+10f );
            CHECK( spb::json::deserialize< float >( "3.14E-10"sv ) == 3.14E-10f );
            CHECK_THROWS( ( void ) spb::json::deserialize< float >( "hello"sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< float >( ""sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< float >( "true"sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< float >( R"("hello")"sv ) );
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::deserialize< std::vector< float > >( "[]"sv ) ==
                       std::vector< float >{ } );
                CHECK( spb::json::deserialize< std::vector< float > >( "null"sv ) ==
                       std::vector< float >{ } );
                CHECK( spb::json::deserialize< std::vector< float > >( "[42]"sv ) ==
                       std::vector< float >{ 42 } );
                CHECK( spb::json::deserialize< std::vector< float > >( "[42,3.14]"sv ) ==
                       std::vector< float >{ 42, 3.14f } );
                CHECK( spb::json::deserialize< std::vector< float > >( "[42,3.14,0.0]"sv ) ==
                       std::vector< float >{ 42, 3.14f, 0.0f } );
                CHECK( spb::json::deserialize< std::vector< float > >( "[42,3.14,-0.0]"sv ) ==
                       std::vector< float >{ 42, 3.14f, -0.0f } );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< float > >( "42"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< float > >( "true"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::vector< float > >( "hello"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< float > >( R"([)"sv ) );
            }
            SUBCASE( "optional" )
            {
                auto value = std::optional< float >( );
                CHECK_NOTHROW( spb::json::deserialize( value, "42"sv ) );
                CHECK( *value == 42.0f );
                CHECK_NOTHROW( spb::json::deserialize( value, "3.14"sv ) );
                CHECK( *value == 3.14f );
                CHECK_NOTHROW( spb::json::deserialize( value, "0.0"sv ) );
                CHECK( *value == 0.0f );
                CHECK_NOTHROW( spb::json::deserialize( value, "-3.14"sv ) );
                CHECK( *value == -3.14f );
                CHECK( spb::json::deserialize< std::optional< float > >( "42"sv ) == 42 );
                CHECK( spb::json::deserialize< std::optional< float > >( "null"sv ) ==
                       std::nullopt );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< float > >( "hello"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::optional< float > >( ""sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< float > >( "true"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< float > >( R"("hello")"sv ) );
            }
        }
        SUBCASE( "int32" )
        {

            CHECK( spb::json::deserialize< int32_t >( "42"sv ) == 42 );
            CHECK( spb::json::deserialize< int32_t >( "\"42\""sv ) == 42 );
            CHECK( spb::json::deserialize< int32_t >( "-42"sv ) == -42 );
            CHECK( spb::json::deserialize< int32_t >( "0"sv ) == 0 );
            CHECK( spb::json::deserialize< int32_t >( "\"-0\""sv ) == 0 );
            CHECK( spb::json::deserialize< int32_t >( "2147483647"sv ) == 2147483647 );
            CHECK( spb::json::deserialize< int32_t >( "-2147483648"sv ) == -2147483648 );
            CHECK_THROWS( ( void ) spb::json::deserialize< int32_t >( "hello"sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< int32_t >( ""sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< int32_t >( "true"sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< int32_t >( R"("hello")"sv ) );
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::deserialize< std::vector< int32_t > >( R"([42,-42,0])"sv ) ==
                       std::vector< int32_t >{ 42, -42, 0 } );
                CHECK( spb::json::deserialize< std::vector< int32_t > >(
                           R"([-2147483648,2147483647])"sv ) ==
                       std::vector< int32_t >{ -2147483648, 2147483647 } );
                CHECK( spb::json::deserialize< std::vector< int32_t > >( R"([])"sv ) ==
                       std::vector< int32_t >( ) );
                CHECK( spb::json::deserialize< std::vector< int32_t > >( R"(null)"sv ) ==
                       std::vector< int32_t >( ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::vector< int32_t > >( R"([)"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::vector< int32_t > >( R"(])"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::vector< int32_t > >( R"([42,)"sv ) );
            }
            SUBCASE( "optional" )
            {
                CHECK( spb::json::deserialize< std::optional< int32_t > >( "42"sv ) ==
                       std::optional< int32_t >( 42 ) );
                CHECK( spb::json::deserialize< std::optional< int32_t > >( "-42"sv ) ==
                       std::optional< int32_t >( -42 ) );
                CHECK( spb::json::deserialize< std::optional< int32_t > >( "0"sv ) ==
                       std::optional< int32_t >( 0 ) );
                CHECK( spb::json::deserialize< std::optional< int32_t > >( "2147483647"sv ) ==
                       std::optional< int32_t >( 2147483647 ) );
                CHECK( spb::json::deserialize< std::optional< int32_t > >( "-2147483648"sv ) ==
                       std::optional< int32_t >( -2147483648 ) );
                CHECK( spb::json::deserialize< std::optional< int32_t > >( "null"sv ) ==
                       std::optional< int32_t >( ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< int32_t > >( "hello"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< int32_t > >( R"()"sv ) );
            }
            SUBCASE( "ptr" )
            {
                auto value = std::unique_ptr< int32_t >( );
                CHECK_NOTHROW( spb::json::deserialize( value, "42"sv ) );
                CHECK( *value == 42 );
                CHECK_NOTHROW( spb::json::deserialize( value, "-42"sv ) );
                CHECK( *value == -42 );
                CHECK_NOTHROW( spb::json::deserialize( value, "0"sv ) );
                CHECK( *value == 0 );
                CHECK_NOTHROW( spb::json::deserialize( value, "2147483647"sv ) );
                CHECK( *value == 2147483647 );
                CHECK_NOTHROW( spb::json::deserialize( value, "-2147483648"sv ) );
                CHECK( *value == -2147483648 );
                CHECK_NOTHROW( spb::json::deserialize( value, "null"sv ) );
                CHECK( value == nullptr );
            }
            SUBCASE( "bitfield" )
            {
                CHECK( spb::json::deserialize< Test::Scalar::ReqUint8_1 >( R"({"value":0})"sv )
                           .value == 0 );
                CHECK( spb::json::deserialize< Test::Scalar::ReqUint8_1 >( R"({"value":1})"sv )
                           .value == 1 );
                CHECK_THROWS( ( void ) spb::json::deserialize< Test::Scalar::ReqUint8_1 >(
                    R"({"value":2})"sv ) );
            }
        }

        SUBCASE( "repeated" )
        {
            CHECK(
                spb::json::deserialize< std::vector< std::string > >( R"(["hello","world"])"sv ) ==
                std::vector< std::string >{ "hello", "world" } );
            CHECK( spb::json::deserialize< std::vector< std::string > >( R"([])"sv ) ==
                   std::vector< std::string >{ } );
            CHECK( spb::json::deserialize< std::vector< std::string > >( R"(null)"sv ) ==
                   std::vector< std::string >{ } );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::string > >( R"()"sv ) );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::string > >( R"(true)"sv ) );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::string > >( R"("hello"])"sv ) );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::string > >( R"([)"sv ) );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::string > >( R"(["hello")"sv ) );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::string > >( R"(["hello",)"sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< std::string > >(
                R"(["hello",])"sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< std::string > >(
                R"(["hello","world")"sv ) );
        }
        SUBCASE( "bytes" )
        {
            CHECK( spb::json::deserialize< std::vector< std::byte > >( R"("AAECAwQ=")"sv ) ==
                   to_bytes( "\x00\x01\x02\x03\x04"sv ) );
            CHECK( spb::json::deserialize< std::vector< std::byte > >( R"("aGVsbG8=")"sv ) ==
                   to_bytes( "hello" ) );
            CHECK( spb::json::deserialize< std::vector< std::byte > >( R"(null)"sv ) ==
                   std::vector< std::byte >{ } );
            CHECK( spb::json::deserialize< std::vector< std::byte > >( R"("")"sv ) ==
                   std::vector< std::byte >{ } );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::byte > >( R"(true)"sv ) );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::byte > >( R"("AAECAwQ")"sv ) );
            CHECK_THROWS(
                ( void ) spb::json::deserialize< std::vector< std::byte > >( R"([])"sv ) );
            CHECK_THROWS( ( void ) spb::json::deserialize< std::vector< std::byte > >( R"()"sv ) );
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::deserialize< std::vector< std::vector< std::byte > > >(
                           R"(["AAECAwQ="])"sv ) ==
                       std::vector< std::vector< std::byte > >{
                           to_bytes( "\x00\x01\x02\x03\x04"sv ) } );
                CHECK( spb::json::deserialize< std::vector< std::vector< std::byte > > >(
                           R"(["AAECAwQ=","aGVsbG8="])"sv ) ==
                       std::vector< std::vector< std::byte > >{
                           to_bytes( "\x00\x01\x02\x03\x04"sv ), to_bytes( "hello" ) } );
                CHECK( spb::json::deserialize< std::vector< std::vector< std::byte > > >(
                           R"([])"sv ) == std::vector< std::vector< std::byte > >{ } );
                CHECK( spb::json::deserialize< std::vector< std::vector< std::byte > > >(
                           R"(null)"sv ) == std::vector< std::vector< std::byte > >{ } );
                CHECK( spb::json::deserialize< std::vector< std::vector< std::byte > > >(
                           R"([""])"sv ) ==
                       std::vector< std::vector< std::byte > >{ std::vector< std::byte >{} } );
            }
            SUBCASE( "optional" )
            {
                CHECK( spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                           R"(null)"sv ) == std::nullopt );
                CHECK( spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                           R"("AAECAwQ=")"sv ) ==
                       std::vector< std::byte >{ std::byte( 0 ), std::byte( 1 ), std::byte( 2 ),
                                                 std::byte( 3 ), std::byte( 4 ) } );
                CHECK( spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                           R"("aGVsbG8=")"sv ) ==
                       std::vector< std::byte >{ std::byte( 'h' ), std::byte( 'e' ),
                                                 std::byte( 'l' ), std::byte( 'l' ),
                                                 std::byte( 'o' ) } );
                CHECK( spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                           R"("")"sv ) == std::vector< std::byte >{ } );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                        R"(true)"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                        R"("AAECAwQ")"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                        R"([])"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::optional< std::vector< std::byte > > >(
                        R"()"sv ) );
            }
        }
        SUBCASE( "map" )
        {
            SUBCASE( "int32/int32" )
            {
                CHECK( spb::json::deserialize< std::map< int32_t, int32_t > >( R"({"1":2})"sv ) ==
                       std::map< int32_t, int32_t >{ { 1, 2 } } );
                CHECK( spb::json::deserialize< std::map< int32_t, int32_t > >(
                           R"({"1":2,"2":3})"sv ) ==
                       std::map< int32_t, int32_t >{ { 1, 2 }, { 2, 3 } } );
                CHECK( spb::json::deserialize< std::map< int32_t, int32_t > >( R"({})"sv ) ==
                       std::map< int32_t, int32_t >{ } );
                CHECK( spb::json::deserialize< std::map< int32_t, int32_t > >( R"(null)"sv ) ==
                       std::map< int32_t, int32_t >{ } );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::map< int32_t, int32_t > >( R"()"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< int32_t, int32_t > >(
                    R"("hello")"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< int32_t, int32_t > >(
                    R"({"hello":2})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< int32_t, int32_t > >(
                    R"({"1":"hello"})"sv ) );
            }
            SUBCASE( "string/string" )
            {
                CHECK( spb::json::deserialize< std::map< std::string, std::string > >(
                           R"({"hello":"world"})"sv ) ==
                       std::map< std::string, std::string >{ { "hello", "world" } } );
                CHECK( spb::json::deserialize< std::map< std::string, std::string > >(
                           R"({})"sv ) == std::map< std::string, std::string >{ } );
                CHECK( spb::json::deserialize< std::map< std::string, std::string > >(
                           R"(null)"sv ) == std::map< std::string, std::string >{ } );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::map< std::string, std::string > >(
                        R"()"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::map< std::string, std::string > >(
                        R"({"1":["hello"]})"sv ) );
                CHECK_THROWS(
                    ( void ) spb::json::deserialize< std::map< std::string, std::string > >(
                        R"({"hello":{"hello":"world"}]})"sv ) );
            }
            SUBCASE( "int32/string" )
            {
                CHECK( spb::json::deserialize< std::map< int32_t, std::string > >(
                           R"({"1":"hello"})"sv ) ==
                       std::map< int32_t, std::string >{ { 1, "hello" } } );
                CHECK( spb::json::deserialize< std::map< int32_t, std::string > >( R"({})"sv ) ==
                       std::map< int32_t, std::string >{ } );
                CHECK( spb::json::deserialize< std::map< int32_t, std::string > >( R"(null)"sv ) ==
                       std::map< int32_t, std::string >{ } );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< int32_t, std::string > >(
                    R"()"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< int32_t, std::string > >(
                    R"({"hello":"world"})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< int32_t, std::string > >(
                    R"({"1":1})"sv ) );
            }
            SUBCASE( "string/int32" )
            {
                CHECK( spb::json::deserialize< std::map< std::string, int32_t > >(
                           R"({"hello":2})"sv ) ==
                       std::map< std::string, int32_t >{ { "hello", 2 } } );
                CHECK( spb::json::deserialize< std::map< std::string, int32_t > >( R"({})"sv ) ==
                       std::map< std::string, int32_t >{ } );
                CHECK( spb::json::deserialize< std::map< std::string, int32_t > >( R"(null)"sv ) ==
                       std::map< std::string, int32_t >{ } );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< std::string, int32_t > >(
                    R"()"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< std::string, int32_t > >(
                    R"({"2","hello"})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< std::string, int32_t > >(
                    R"({"1":})"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::map< std::string, int32_t > >(
                    R"({"hello":2)"sv ) );
            }
            SUBCASE( "string/name" )
            {
                CHECK( spb::json::deserialize< std::map< std::string, Test::Name > >(
                           R"({"hello":{"name":"john"}})"sv ) ==
                       std::map< std::string, Test::Name >{ { "hello", { .name = "john" } } } );
            }
        }
        SUBCASE( "string" )
        {
            SUBCASE( "escape" )
            {
                for( int c = 0; c <= 0xff; c++ )
                {
                    const auto esc = c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' ||
                        c == 'n' || c == 'r' || c == 't';

                    char buffer[] = { '"', '\\', char( c ), '"', 0 };
                    if( !esc )
                    {
                        CHECK_THROWS( ( void ) spb::json::deserialize< std::string >(
                            std::string_view( buffer ) ) );
                    }
                }

                CHECK( spb::json::deserialize< std::string >( R"("hello")"sv ) == "hello" );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::string >( R"(hello")"sv ) );
                CHECK_THROWS( ( void ) spb::json::deserialize< std::string >( R"("hello)"sv ) );
                SUBCASE( "escaped" )
                {
                    CHECK( spb::json::deserialize< std::string >( R"("\"")"sv ) == "\"" );
                    CHECK( spb::json::deserialize< std::string >( R"("\\")"sv ) == "\\" );
                    CHECK( spb::json::deserialize< std::string >( R"("\/")"sv ) == "/" );
                    CHECK( spb::json::deserialize< std::string >( R"("\b")"sv ) == "\b" );
                    CHECK( spb::json::deserialize< std::string >( R"("\f")"sv ) == "\f" );
                    CHECK( spb::json::deserialize< std::string >( R"("\n")"sv ) == "\n" );
                    CHECK( spb::json::deserialize< std::string >( R"("\r")"sv ) == "\r" );
                    CHECK( spb::json::deserialize< std::string >( R"("\t")"sv ) == "\t" );
                    CHECK( spb::json::deserialize< std::string >( R"("\"\b\f\n\r\t\"")"sv ) ==
                           "\"\b\f\n\r\t\"" );
                    CHECK( spb::json::deserialize< std::string >( R"("\nhell\to")"sv ) ==
                           "\nhell\to" );
                }
            }
        }
        SUBCASE( "variant" )
        {
            SUBCASE( "int" )
            {
                auto variant = spb::json::deserialize< Test::Variant >( R"({"var_int":42})"sv );
                CHECK( variant.oneof_field.index( ) == 0 );
                CHECK( std::get< 0 >( variant.oneof_field ) == 42 );
            }
            SUBCASE( "string" )
            {
                auto variant =
                    spb::json::deserialize< Test::Variant >( R"({"var_string":"hello"})"sv );
                CHECK( variant.oneof_field.index( ) == 1 );
                CHECK( std::get< 1 >( variant.oneof_field ) == std::string( "hello" ) );
            }
            SUBCASE( "bytes" )
            {
                auto variant =
                    spb::json::deserialize< Test::Variant >( R"({"var_bytes":"aGVsbG8="})"sv );
                CHECK( variant.oneof_field.index( ) == 2 );
                CHECK( std::get< 2 >( variant.oneof_field ) ==
                       std::vector< std::byte >{ std::byte( 'h' ), std::byte( 'e' ),
                                                 std::byte( 'l' ), std::byte( 'l' ),
                                                 std::byte( 'o' ) } );
            }
            SUBCASE( "name" )
            {
                auto variant =
                    spb::json::deserialize< Test::Variant >( R"({"name":{"name":"John"}})"sv );
                CHECK( variant.oneof_field.index( ) == 3 );
                CHECK( std::get< 3 >( variant.oneof_field ) == Test::Name{ .name = "John" } );
            }
            SUBCASE( "collision" )
            {
                auto variant =
                    spb::json::deserialize< Test::Variant >( R"({"bkfvdzz":{"name":"John"}})"sv );
                CHECK( variant.oneof_field.index( ) == 0 );
            }
        }
    }
    SUBCASE( "serialize" )
    {
        SUBCASE( "string" )
        {
            CHECK( spb::json::serialize< std::string, std::string >( "john" ) == R"("john")" );
            SUBCASE( "escaped" )
            {
                CHECK( spb::json::serialize< std::string, std::string >( "\"" ) == R"("\"")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\\" ) == R"("\\")" );
                CHECK( spb::json::serialize< std::string, std::string >( "/" ) == R"("/")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\b" ) == R"("\b")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\f" ) == R"("\f")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\n" ) == R"("\n")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\r" ) == R"("\r")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\t" ) == R"("\t")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\"\\/\b\f\n\r\t" ) ==
                       R"("\"\\/\b\f\n\r\t")" );
                CHECK( spb::json::serialize< std::string, std::string >( "\"hello\t" ) ==
                       R"("\"hello\t")" );
            }
            SUBCASE( "optional" )
            {
                CHECK( spb::json::serialize< std::string, std::optional< std::string > >(
                           std::nullopt ) == "" );
                CHECK( spb::json::serialize< std::string, std::optional< std::string > >(
                           "hello" ) == R"("hello")" );
            }
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::serialize< std::string, std::vector< std::string > >(
                           { "hello", "world" } ) == R"(["hello","world"])" );
                CHECK( spb::json::serialize< std::string, std::vector< std::string > >( { } ) ==
                       "" );
            }
        }
        SUBCASE( "bool" )
        {
            CHECK( spb::json::serialize( true ) == "true" );
            CHECK( spb::json::serialize( false ) == "false" );
            SUBCASE( "optional" )
            {
                CHECK( spb::json::serialize< std::string, std::optional< bool > >( std::nullopt ) ==
                       "" );
                CHECK( spb::json::serialize< std::string, std::optional< bool > >( true ) ==
                       "true" );
                CHECK( spb::json::serialize< std::string, std::optional< bool > >( false ) ==
                       "false" );
            }
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::serialize< std::string, std::vector< bool > >(
                           { true, false } ) == R"([true,false])" );
                CHECK( spb::json::serialize< std::string, std::vector< bool > >( { } ) == "" );
            }
        }
        SUBCASE( "int" )
        {
            CHECK( spb::json::serialize( 42 ) == "42" );
            SUBCASE( "optional" )
            {
                CHECK( spb::json::serialize< std::string, std::optional< int > >( std::nullopt ) ==
                       "" );
                CHECK( spb::json::serialize< std::string, std::optional< int > >( 42 ) == "42" );
            }
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::serialize< std::string, std::vector< int > >( { 42 } ) ==
                       R"([42])" );
                CHECK( spb::json::serialize< std::string, std::vector< int > >( { 42, 3 } ) ==
                       R"([42,3])" );
                CHECK( spb::json::serialize< std::string, std::vector< int > >( { } ) == "" );
            }
        }
        SUBCASE( "double" )
        {
            CHECK( spb::json::serialize( 42.0 ) == "42" );
            SUBCASE( "optional" )
            {
                CHECK( spb::json::serialize< std::string, std::optional< double > >(
                           std::nullopt ) == "" );
                CHECK( spb::json::serialize< std::string, std::optional< double > >( 42.3 ) ==
                       "42.3" );
            }
            SUBCASE( "repeated" )
            {
                CHECK( spb::json::serialize< std::string, std::vector< double > >( { 42.3 } ) ==
                       R"([42.3])" );
                CHECK( spb::json::serialize< std::string, std::vector< double > >(
                           { 42.3, 3.0 } ) == R"([42.3,3])" );
                CHECK( spb::json::serialize< std::string, std::vector< double > >( { } ) == "" );
            }
        }
        SUBCASE( "bytes" )
        {
            CHECK( spb::json::serialize( to_bytes( "\x00\x01\x02"sv ) ) == R"("AAEC")" );

            CHECK( spb::json::serialize< std::string, std::vector< std::byte > >(
                       to_bytes( "\x00\x01\x02\x03\x04"sv ) ) == R"("AAECAwQ=")" );
            CHECK( spb::json::serialize< std::string, std::vector< std::byte > >(
                       to_bytes( "hello"sv ) ) == R"("aGVsbG8=")" );
            CHECK( spb::json::serialize< std::string, std::vector< std::byte > >( { } ) == "" );

            SUBCASE( "repeated" )
            {
                CHECK( spb::json::serialize< std::string, std::vector< std::vector< std::byte > > >(
                           std::vector< std::vector< std::byte > >{
                               to_bytes( "\x00\x01\x02\x03\x04"sv ) } ) == R"(["AAECAwQ="])" );
                CHECK( spb::json::serialize< std::string, std::vector< std::vector< std::byte > > >(
                           std::vector< std::vector< std::byte > >{
                               to_bytes( "\x00\x01\x02\x03\x04"sv ), to_bytes( "hello"sv ) } ) ==
                       R"(["AAECAwQ=","aGVsbG8="])" );
                CHECK( spb::json::serialize< std::string, std::vector< std::vector< std::byte > > >(
                           std::vector< std::vector< std::byte > >{ } ) == "" );
            }
            SUBCASE( "optional" )
            {
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::nullopt ) == "" );
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::vector< std::byte >{ to_bytes( "\x00\x01\x02\x03\x04"sv ) } ) ==
                    R"("AAECAwQ=")" );
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::vector< std::byte >{ to_bytes( "hello"sv ) } ) == R"("aGVsbG8=")" );
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::vector< std::byte >{ } ) == "" );
            }
            SUBCASE( "fixed" )
            {
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::nullopt ) == "" );
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::vector< std::byte >{ to_bytes( "\x00\x01\x02\x03\x04"sv ) } ) ==
                    R"("AAECAwQ=")" );
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::vector< std::byte >{ to_bytes( "hello"sv ) } ) == R"("aGVsbG8=")" );
                CHECK(
                    spb::json::serialize< std::string, std::optional< std::vector< std::byte > > >(
                        std::vector< std::byte >{ } ) == "" );
            }
        }
        SUBCASE( "variant" )
        {
            SUBCASE( "int" )
            {
                CHECK( spb::json::serialize( Test::Variant{ .oneof_field = 42U } ) ==
                       R"({"var_int":42})" );
            }
            SUBCASE( "string" )
            {
                CHECK( spb::json::serialize( Test::Variant{ .oneof_field = "hello" } ) ==
                       R"({"var_string":"hello"})" );
            }
            SUBCASE( "bytes" )
            {
                CHECK(
                    spb::json::serialize( Test::Variant{
                        .oneof_field = std::vector< std::byte >{
                            std::byte( 'h' ), std::byte( 'e' ), std::byte( 'l' ), std::byte( 'l' ),
                            std::byte( 'o' ) } } ) == R"({"var_bytes":"aGVsbG8="})" );
            }
            SUBCASE( "name" )
            {
                CHECK( spb::json::serialize(
                           Test::Variant{ .oneof_field = Test::Name{ .name = "John" } } ) ==
                       R"({"name":{"name":"John"}})" );
            }
        }
        SUBCASE( "map" )
        {
            SUBCASE( "int32/int32" )
            {
                CHECK( spb::json::serialize( std::map< int32_t, int32_t >{ { 1, 2 } } ) ==
                       R"({"1":2})" );
                CHECK( spb::json::serialize( std::map< int32_t, int32_t >{ { 1, 2 }, { 2, 3 } } ) ==
                       R"({"1":2,"2":3})" );
                CHECK( spb::json::serialize( std::map< int32_t, int32_t >{ } ) == "" );
            }
            SUBCASE( "string/string" )
            {
                CHECK( spb::json::serialize( std::map< std::string, std::string >{
                           { "hello", "world" } } ) == R"({"hello":"world"})" );
                CHECK( spb::json::serialize( std::map< std::string, std::string >{ } ) == "" );
            }
            SUBCASE( "int32/string" )
            {
                CHECK( spb::json::serialize( std::map< int32_t, std::string >{ { 1, "hello" } } ) ==
                       ( R"({"1":"hello"})" ) );
                CHECK( spb::json::serialize( std::map< int32_t, std::string >{ } ) == "" );
            }
            SUBCASE( "string/int32" )
            {
                CHECK( spb::json::serialize( std::map< std::string, int32_t >{ { "hello", 2 } } ) ==
                       R"({"hello":2})" );
                CHECK( spb::json::serialize( std::map< std::string, int32_t >{ } ) == "" );
            }
            SUBCASE( "string/name" )
            {
                CHECK( spb::json::serialize( std::map< std::string, Test::Name >{
                           { "hello", { .name = "john" } } } ) == R"({"hello":{"name":"john"}})" );
            }
        }
        SUBCASE( "enum" )
        {
            CHECK( spb::json::serialize( PhoneBook::Person::PhoneType::HOME ) == "\"HOME\"" );
        }
        SUBCASE( "person" )
        {
            CHECK( spb::json::serialize( PhoneBook::Person{
                       .name   = "John Doe",
                       .id     = 123,
                       .email  = "QXUeh@example.com",
                       .phones = {
                           PhoneBook::Person::PhoneNumber{
                               .number = "555-4321",
                               .type   = PhoneBook::Person::PhoneType::HOME,
                           },
                       },
                   } ) == R"({"name":"John Doe","id":123,"email":"QXUeh@example.com","phones":[{"number":"555-4321","type":"HOME"}]})" );
        }
        SUBCASE( "name" )
        {
            CHECK( spb::json::serialize( Test::Name{ } ) == R"({})" );
            CHECK( spb::json::serialize_size( Test::Name{ } ) == 2 );
        }
    }
}
