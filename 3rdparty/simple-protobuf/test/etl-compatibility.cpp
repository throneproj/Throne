#include <cstdint>
#include <etl-person.pb.h>
#include <etl-scalar.pb.h>
#include <google/protobuf/util/json_util.h>
#include <gpb-etl-person.pb.h>
#include <gpb-etl-scalar.pb.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace std
{
template < size_t S >
auto operator==( const string & lhs, const ::etl::vector< byte, S > & rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && ( memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0 );
}
template < size_t S >
auto operator==( const ::etl::string< S > & lhs, const string & rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && ( memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0 );
}
}// namespace std

namespace Test
{

namespace Etl::Scalar
{

template < typename T >
concept HasValueMember = requires( T t ) {
    {
        t.value
    };
};

template < typename T >
    requires HasValueMember< T >
auto operator==( const T & lhs, const T & rhs ) noexcept -> bool
{
    return lhs.value == rhs.value;
}
}// namespace Etl::Scalar
}// namespace Test

namespace PhoneBook::Etl
{
auto operator==( const Person::PhoneNumber & lhs, const Person::PhoneNumber & rhs ) noexcept -> bool
{
    return lhs.number == rhs.number && lhs.type == rhs.type;
}

auto operator==( const Person & lhs, const Person & rhs ) noexcept -> bool
{
    return lhs.name == rhs.name && lhs.id == rhs.id && lhs.email == rhs.email && lhs.phones == rhs.phones;
}
}// namespace PhoneBook::Etl

namespace
{
auto to_bytes( std::string_view str ) -> etl::vector< std::byte, 8 >
{
    auto span = std::span< std::byte >( ( std::byte * ) str.data( ), str.size( ) );
    return { span.data( ), span.data( ) + span.size( ) };
}

template < typename T >
concept is_gpb_repeated = requires( T t ) {
    {
        t.value( 0 )
    };
};

template < typename T >
auto opt_size( const std::optional< T > & opt ) -> std::size_t
{
    if( opt.has_value( ) )
    {
        return opt.value( ).size( );
    }
    return 0;
}

template < typename T, size_t S >
auto opt_size( const etl::vector< T, S > & opt ) -> std::size_t
{
    return opt.size( );
}

template < typename GPB, typename SPB >
void gpb_test( const SPB & spb )
{
    auto gpb            = GPB( );
    auto spb_serialized = spb::pb::serialize( spb );

    REQUIRE( gpb.ParseFromString( spb_serialized ) );
    if constexpr( is_gpb_repeated< GPB > )
    {
        REQUIRE( gpb.value( ).size( ) == opt_size( spb.value ) );
        for( size_t i = 0; i < opt_size( spb.value ); ++i )
        {
            REQUIRE( gpb.value( i ) == spb.value[ i ] );
        }
    }
    else
    {
        REQUIRE( spb.value == gpb.value( ) );
    }
    auto gpb_serialized = std::string( );
    REQUIRE( gpb.SerializeToString( &gpb_serialized ) );
    REQUIRE( spb::pb::deserialize< SPB >( gpb_serialized ) == spb );
}

template < typename GPB, typename SPB >
void gpb_json( const SPB & spb )
{
    auto gpb            = GPB( );
    auto spb_serialized = spb::json::serialize( spb );

    auto parse_options = google::protobuf::util::JsonParseOptions{ };
    ( void ) JsonStringToMessage( spb_serialized, &gpb, parse_options );

    if constexpr( is_gpb_repeated< GPB > )
    {
        REQUIRE( gpb.value( ).size( ) == opt_size( spb.value ) );
        for( size_t i = 0; i < opt_size( spb.value ); ++i )
        {
            REQUIRE( gpb.value( i ) == spb.value[ i ] );
        }
    }
    else
    {
        REQUIRE( spb.value == gpb.value( ) );
    }
    auto json_string                         = std::string( );
    auto print_options                       = google::protobuf::util::JsonPrintOptions( );
    print_options.preserve_proto_field_names = true;

    ( void ) MessageToJsonString( gpb, &json_string, print_options );
    REQUIRE( spb::json::deserialize< SPB >( json_string ) == spb );

    print_options.preserve_proto_field_names = false;
    json_string.clear( );
    ( void ) MessageToJsonString( gpb, &json_string, print_options );
    REQUIRE( spb::json::deserialize< SPB >( json_string ) == spb );
}

template < typename GPB, typename SPB >
void gpb_compatibility( const SPB & spb )
{
    SUBCASE( "gpb serialize/deserialize" )
    {
        gpb_test< GPB, SPB >( spb );
    }
    SUBCASE( "json serialize/deserialize" )
    {
        gpb_json< GPB, SPB >( spb );
    }
}

}// namespace

using namespace std::literals;

TEST_CASE( "string" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqString >( Test::Etl::Scalar::ReqString{ .value = "hello" } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::OptString >( Test::Etl::Scalar::OptString{ .value = "hello" } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::RepString >( Test::Etl::Scalar::RepString{ .value = { "hello" } } );
        gpb_compatibility< Test::Etl::Scalar::gpb::RepString >( Test::Etl::Scalar::RepString{ .value = { "hello", "world" } } );
    }
}
TEST_CASE( "bool" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqBool >( Test::Etl::Scalar::ReqBool{ .value = true } );
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqBool >( Test::Etl::Scalar::ReqBool{ .value = false } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::OptBool >( Test::Etl::Scalar::OptBool{ .value = true } );
        gpb_compatibility< Test::Etl::Scalar::gpb::OptBool >( Test::Etl::Scalar::OptBool{ .value = false } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::RepBool >( Test::Etl::Scalar::RepBool{ .value = { true } } );
        gpb_compatibility< Test::Etl::Scalar::gpb::RepBool >( Test::Etl::Scalar::RepBool{ .value = { true, false } } );

        SUBCASE( "packed" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepPackBool >( Test::Etl::Scalar::RepPackBool{ .value = { true } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepPackBool >( Test::Etl::Scalar::RepPackBool{ .value = { true, false } } );
        }
    }
}
TEST_CASE( "int" )
{
    SUBCASE( "varint32" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqInt32 >( Test::Etl::Scalar::ReqInt32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqInt32 >( Test::Etl::Scalar::ReqInt32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqInt32 >( Test::Etl::Scalar::ReqInt32{ .value = -2 } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptInt32 >( Test::Etl::Scalar::OptInt32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptInt32 >( Test::Etl::Scalar::OptInt32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptInt32 >( Test::Etl::Scalar::OptInt32{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepInt32 >( Test::Etl::Scalar::RepInt32{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepInt32 >( Test::Etl::Scalar::RepInt32{ .value = { 0x42, 0x3 } } );

            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackInt32 >( Test::Etl::Scalar::RepPackInt32{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackInt32 >( Test::Etl::Scalar::RepPackInt32{ .value = { 0x42, 0x3 } } );
            }
        }
    }
    SUBCASE( "varint64" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqInt64 >( Test::Etl::Scalar::ReqInt64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqInt64 >( Test::Etl::Scalar::ReqInt64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqInt64 >( Test::Etl::Scalar::ReqInt64{ .value = -2 } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptInt64 >( Test::Etl::Scalar::OptInt64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptInt64 >( Test::Etl::Scalar::OptInt64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptInt64 >( Test::Etl::Scalar::OptInt64{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepInt64 >( Test::Etl::Scalar::RepInt64{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepInt64 >( Test::Etl::Scalar::RepInt64{ .value = { 0x42, 0x3 } } );

            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackInt64 >( Test::Etl::Scalar::RepPackInt64{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackInt64 >( Test::Etl::Scalar::RepPackInt64{ .value = { 0x42, 0x3 } } );
            }
        }
    }
    SUBCASE( "svarint32" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSint32 >( Test::Etl::Scalar::ReqSint32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSint32 >( Test::Etl::Scalar::ReqSint32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSint32 >( Test::Etl::Scalar::ReqSint32{ .value = -2 } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSint32 >( Test::Etl::Scalar::OptSint32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSint32 >( Test::Etl::Scalar::OptSint32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSint32 >( Test::Etl::Scalar::OptSint32{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSint32 >( Test::Etl::Scalar::RepSint32{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSint32 >( Test::Etl::Scalar::RepSint32{ .value = { 0x42, -2 } } );

            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSint32 >( Test::Etl::Scalar::RepPackSint32{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSint32 >( Test::Etl::Scalar::RepPackSint32{ .value = { 0x42, -2 } } );
            }
        }
    }
    SUBCASE( "svarint64" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSint64 >( Test::Etl::Scalar::ReqSint64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSint64 >( Test::Etl::Scalar::ReqSint64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSint64 >( Test::Etl::Scalar::ReqSint64{ .value = -2 } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSint64 >( Test::Etl::Scalar::OptSint64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSint64 >( Test::Etl::Scalar::OptSint64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSint64 >( Test::Etl::Scalar::OptSint64{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSint64 >( Test::Etl::Scalar::RepSint64{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSint64 >( Test::Etl::Scalar::RepSint64{ .value = { 0x42, -2 } } );

            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSint64 >( Test::Etl::Scalar::RepPackSint64{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSint64 >( Test::Etl::Scalar::RepPackSint64{ .value = { 0x42, -2 } } );
            }
        }
    }
    SUBCASE( "fixed32" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqFixed32 >( Test::Etl::Scalar::ReqFixed32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqFixed32 >( Test::Etl::Scalar::ReqFixed32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqFixed32 >( Test::Etl::Scalar::ReqFixed32{ .value = uint32_t( -2 ) } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptFixed32 >( Test::Etl::Scalar::OptFixed32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptFixed32 >( Test::Etl::Scalar::OptFixed32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptFixed32 >( Test::Etl::Scalar::OptFixed32{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepFixed32 >( Test::Etl::Scalar::RepFixed32{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepFixed32 >( Test::Etl::Scalar::RepFixed32{ .value = { 0x42, 0x3 } } );

            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackFixed32 >( Test::Etl::Scalar::RepPackFixed32{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackFixed32 >( Test::Etl::Scalar::RepPackFixed32{ .value = { 0x42, 0x3 } } );
            }
        }
    }
    SUBCASE( "fixed64" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqFixed64 >( Test::Etl::Scalar::ReqFixed64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqFixed64 >( Test::Etl::Scalar::ReqFixed64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqFixed64 >( Test::Etl::Scalar::ReqFixed64{ .value = uint64_t( -2 ) } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptFixed64 >( Test::Etl::Scalar::OptFixed64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptFixed64 >( Test::Etl::Scalar::OptFixed64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptFixed64 >( Test::Etl::Scalar::OptFixed64{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepFixed64 >( Test::Etl::Scalar::RepFixed64{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepFixed64 >( Test::Etl::Scalar::RepFixed64{ .value = { 0x42, 0x3 } } );
            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackFixed64 >( Test::Etl::Scalar::RepPackFixed64{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackFixed64 >( Test::Etl::Scalar::RepPackFixed64{ .value = { 0x42, 0x3 } } );
            }
        }
    }
    SUBCASE( "sfixed32" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSfixed32 >( Test::Etl::Scalar::ReqSfixed32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSfixed32 >( Test::Etl::Scalar::ReqSfixed32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSfixed32 >( Test::Etl::Scalar::ReqSfixed32{ .value = -2 } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSfixed32 >( Test::Etl::Scalar::OptSfixed32{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSfixed32 >( Test::Etl::Scalar::OptSfixed32{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSfixed32 >( Test::Etl::Scalar::OptSfixed32{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSfixed32 >( Test::Etl::Scalar::RepSfixed32{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSfixed32 >( Test::Etl::Scalar::RepSfixed32{ .value = { 0x42, 0x3 } } );

            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSfixed32 >( Test::Etl::Scalar::RepPackSfixed32{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSfixed32 >( Test::Etl::Scalar::RepPackSfixed32{ .value = { 0x42, 0x3 } } );
            }
        }
    }
    SUBCASE( "sfixed64" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSfixed64 >( Test::Etl::Scalar::ReqSfixed64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSfixed64 >( Test::Etl::Scalar::ReqSfixed64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::ReqSfixed64 >( Test::Etl::Scalar::ReqSfixed64{ .value = -2 } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSfixed64 >( Test::Etl::Scalar::OptSfixed64{ .value = 0x42 } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSfixed64 >( Test::Etl::Scalar::OptSfixed64{ .value = 0xff } );
            gpb_compatibility< Test::Etl::Scalar::gpb::OptSfixed64 >( Test::Etl::Scalar::OptSfixed64{ .value = -2 } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSfixed64 >( Test::Etl::Scalar::RepSfixed64{ .value = { 0x42 } } );
            gpb_compatibility< Test::Etl::Scalar::gpb::RepSfixed64 >( Test::Etl::Scalar::RepSfixed64{ .value = { 0x42, 0x3 } } );

            SUBCASE( "packed" )
            {
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSfixed64 >( Test::Etl::Scalar::RepPackSfixed64{ .value = { 0x42 } } );
                gpb_compatibility< Test::Etl::Scalar::gpb::RepPackSfixed64 >( Test::Etl::Scalar::RepPackSfixed64{ .value = { 0x42, 0x3 } } );
            }
        }
    }
}
TEST_CASE( "float" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqFloat >( Test::Etl::Scalar::ReqFloat{ .value = 42.0 } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::OptFloat >( Test::Etl::Scalar::OptFloat{ .value = 42.3 } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::RepFloat >( Test::Etl::Scalar::RepFloat{ .value = { 42.3 } } );
        gpb_compatibility< Test::Etl::Scalar::gpb::RepFloat >( Test::Etl::Scalar::RepFloat{ .value = { 42.0, 42.3 } } );
    }
}
TEST_CASE( "double" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqDouble >( Test::Etl::Scalar::ReqDouble{ .value = 42.0 } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::OptDouble >( Test::Etl::Scalar::OptDouble{ .value = 42.3 } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::RepDouble >( Test::Etl::Scalar::RepDouble{ .value = { 42.3 } } );
        gpb_compatibility< Test::Etl::Scalar::gpb::RepDouble >( Test::Etl::Scalar::RepDouble{ .value = { 42.3, 3.0 } } );
    }
}
TEST_CASE( "bytes" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqBytes >( Test::Etl::Scalar::ReqBytes{ .value = to_bytes( "hello" ) } );
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqBytes >( Test::Etl::Scalar::ReqBytes{ .value = to_bytes( "\x00\x01\x02"sv ) } );
        gpb_compatibility< Test::Etl::Scalar::gpb::ReqBytes >( Test::Etl::Scalar::ReqBytes{ .value = to_bytes( "\x00\x01\x02\x03\x04"sv ) } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::OptBytes >( Test::Etl::Scalar::OptBytes{ .value = to_bytes( "hello" ) } );
        gpb_compatibility< Test::Etl::Scalar::gpb::OptBytes >( Test::Etl::Scalar::OptBytes{ .value = to_bytes( "\x00\x01\x02"sv ) } );
        gpb_compatibility< Test::Etl::Scalar::gpb::OptBytes >( Test::Etl::Scalar::OptBytes{ .value = to_bytes( "\x00\x01\x02\x03\x04"sv ) } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Etl::Scalar::gpb::RepBytes >( Test::Etl::Scalar::RepBytes{ .value = { to_bytes( "hello" ) } } );
        gpb_compatibility< Test::Etl::Scalar::gpb::RepBytes >( Test::Etl::Scalar::RepBytes{ .value = { to_bytes( "\x00\x01\x02"sv ) } } );
        gpb_compatibility< Test::Etl::Scalar::gpb::RepBytes >( Test::Etl::Scalar::RepBytes{ .value = { to_bytes( "\x00\x01\x02\x03\x04"sv ) } } );
    }
}
TEST_CASE( "person" )
{
    auto gpb = PhoneBook::Etl::gpb::Person( );
    auto spb = PhoneBook::Etl::Person{
        .name   = "John Doe",
        .id     = 123,
        .email  = "QXUeh@example.com",
        .phones = {
            PhoneBook::Etl::Person::PhoneNumber{
                .number = "555-4321",
                .type   = PhoneBook::Etl::Person::PhoneType::HOME,
            },
            PhoneBook::Etl::Person::PhoneNumber{
                .number = "999-1234",
                .type   = PhoneBook::Etl::Person::PhoneType::MOBILE,
            },
        }
    };
    SUBCASE( "gpb serialize/deserialize" )
    {
        auto spb_serialized = spb::pb::serialize( spb );

        REQUIRE( gpb.ParseFromString( spb_serialized ) );
        REQUIRE( gpb.name( ) == spb.name );
        REQUIRE( gpb.id( ) == spb.id );
        REQUIRE( gpb.email( ) == spb.email );
        REQUIRE( gpb.phones_size( ) == 2 );
        for( auto i = 0; i < gpb.phones_size( ); i++ )
        {
            REQUIRE( gpb.phones( i ).number( ) == spb.phones[ i ].number );
            REQUIRE( int( gpb.phones( i ).type( ) ) == int( spb.phones[ i ].type.value( ) ) );
        }

        auto gpb_serialized = std::string( );
        REQUIRE( gpb.SerializeToString( &gpb_serialized ) );
        REQUIRE( spb::pb::deserialize< PhoneBook::Etl::Person >( gpb_serialized ) == spb );
    }
    SUBCASE( "json serialize/deserialize" )
    {
        auto spb_json = spb::json::serialize( spb );

        auto parse_options = google::protobuf::util::JsonParseOptions{ };
        ( void ) JsonStringToMessage( spb_json, &gpb, parse_options );

        REQUIRE( gpb.name( ) == spb.name );
        REQUIRE( gpb.id( ) == spb.id );
        REQUIRE( gpb.email( ) == spb.email );
        REQUIRE( gpb.phones_size( ) == 2 );
        for( auto i = 0; i < gpb.phones_size( ); i++ )
        {
            REQUIRE( gpb.phones( i ).number( ) == spb.phones[ i ].number );
            REQUIRE( int( gpb.phones( i ).type( ) ) == int( spb.phones[ i ].type.value( ) ) );
        }

        auto json_string                         = std::string( );
        auto print_options                       = google::protobuf::util::JsonPrintOptions( );
        print_options.preserve_proto_field_names = true;

        ( void ) MessageToJsonString( gpb, &json_string, print_options );
        REQUIRE( spb::json::deserialize< PhoneBook::Etl::Person >( json_string ) == spb );

        print_options.preserve_proto_field_names = false;
        json_string.clear( );
        ( void ) MessageToJsonString( gpb, &json_string, print_options );
        REQUIRE( spb::json::deserialize< PhoneBook::Etl::Person >( json_string ) == spb );
    }
}
