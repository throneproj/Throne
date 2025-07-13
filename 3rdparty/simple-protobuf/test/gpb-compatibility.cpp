#include "spb/json.hpp"
#include "spb/utf8.h"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <google/protobuf/util/delimited_message_util.h>
#include <google/protobuf/util/json_util.h>
#include <gpb-name.pb.h>
#include <gpb-person.pb.h>
#include <gpb-scalar.pb.h>
#include <initializer_list>
#include <limits>
#include <name.pb.h>
#include <optional>
#include <person.pb.h>
#include <scalar.pb.h>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

namespace std
{
auto operator==( span< byte > lhs, span< byte > rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && ( memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0 );
}

auto operator==( span< char > lhs, span< char > rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && ( memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0 );
}

auto operator==( const array< char, 6 > & lhs, const string & rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && ( memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0 );
}

auto operator==( const string & lhs, const vector< byte > & rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && ( memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0 );
}

template < size_t N >
auto operator==( const string & lhs, const array< byte, N > & rhs ) noexcept -> bool
{
    return lhs.size( ) == rhs.size( ) && ( memcmp( lhs.data( ), rhs.data( ), lhs.size( ) ) == 0 );
}

}// namespace std

namespace Test
{
auto operator==( const Test::Name & lhs, const Test::Name & rhs ) noexcept -> bool
{
    return lhs.name == rhs.name;
}
auto operator==( const Test::Variant & lhs, const Test::Variant & rhs ) noexcept -> bool
{
    return lhs.oneof_field == rhs.oneof_field;
}

namespace Scalar
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
}// namespace Scalar
}// namespace Test

namespace PhoneBook
{
auto operator==( const Person::PhoneNumber & lhs, const Person::PhoneNumber & rhs ) noexcept -> bool
{
    return lhs.number == rhs.number && lhs.type == rhs.type;
}

auto operator==( const Person & lhs, const Person & rhs ) noexcept -> bool
{
    return lhs.name == rhs.name && lhs.id == rhs.id && lhs.email == rhs.email && lhs.phones == rhs.phones;
}
}// namespace PhoneBook

namespace
{
template < size_t N >
auto to_string( const char ( &string )[ N ] )
{
    auto result = std::array< char, N - 1 >( );
    memcpy( result.data( ), &string, result.size( ) );
    return result;
}

auto to_bytes( std::string_view str ) -> std::vector< std::byte >
{
    auto span = std::span< std::byte >( ( std::byte * ) str.data( ), str.size( ) );
    return { span.data( ), span.data( ) + span.size( ) };
}

template < size_t N >
auto to_array( const char ( &string )[ N ] )
{
    auto result = std::array< std::byte, N - 1 >( );
    memcpy( result.data( ), &string, result.size( ) );
    return result;
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

template < typename T >
auto opt_size( const std::vector< T > & opt ) -> std::size_t
{
    return opt.size( );
}

template < typename T >
struct ExtractOptional
{
    using type = T;
};

template < typename T >
struct ExtractOptional< std::optional< T > >
{
    using type = T;
};

template < typename T >
auto enum_value( const T & value )
{
    return std::underlying_type_t< T >( value );
}

template < typename T >
auto enum_value( const std::optional< T > & value )
{
    return enum_value( value.value( ) );
}

template < typename GPB, typename SPB >
void gpb_test( const SPB & spb, const spb::pb::serialize_options & options = { } )
{
    using T = typename ExtractOptional< std::decay_t< decltype( SPB::value ) > >::type;

    auto gpb            = GPB( );
    auto spb_serialized = spb::pb::serialize( spb, options );

    if( options.delimited )
    {
        std::istringstream input_stream{ spb_serialized };
        auto raw_input_stream = google::protobuf::io::IstreamInputStream{ &input_stream };
        REQUIRE( google::protobuf::util::ParseDelimitedFromZeroCopyStream( &gpb, &raw_input_stream,
                                                                           nullptr ) );
    }
    else
    {
        REQUIRE( gpb.ParseFromString( spb_serialized ) );
    }

    if constexpr( is_gpb_repeated< GPB > )
    {
        REQUIRE( gpb.value( ).size( ) == opt_size( spb.value ) );
        for( size_t i = 0; i < opt_size( spb.value ); ++i )
        {
            using value_type = typename decltype( SPB::value )::value_type;
            if constexpr( std::is_enum_v< value_type > )
            {
                REQUIRE( enum_value( spb.value[ i ] ) == gpb.value( i ) );
            }
            else
            {
                REQUIRE( gpb.value( i ) == spb.value[ i ] );
            }
        }
    }
    else if constexpr( std::is_enum_v< T > )
    {
        REQUIRE( enum_value( spb.value ) == gpb.value( ) );
    }
    else
    {
        REQUIRE( spb.value == gpb.value( ) );
    }

    auto gpb_serialized = std::string( );
    if( options.delimited )
    {
        std::ostringstream output_stream;
        //- Wrapped so that the destructor of OstreamOutputStream is called,
        //- which flushes the stream.
        {
            auto raw_output_stream = google::protobuf::io::OstreamOutputStream{ &output_stream };
            REQUIRE( google::protobuf::util::SerializeDelimitedToZeroCopyStream(
                gpb, &raw_output_stream ) );
        }
        gpb_serialized = output_stream.str( );
    }
    else
    {
        REQUIRE( gpb.SerializeToString( &gpb_serialized ) );
    }

    REQUIRE( spb::pb::deserialize< SPB >( gpb_serialized,
                                          {
                                              .delimited = options.delimited,
                                          } )
                 .value == spb.value );
    REQUIRE( gpb_serialized == spb_serialized );
}

template < typename GPB, typename SPB >
void gpb_json( const SPB & spb )
{
    using T = typename ExtractOptional< std::decay_t< decltype( SPB::value ) > >::type;

    auto gpb            = GPB( );
    auto spb_serialized = spb::json::serialize( spb );

    auto parse_options = google::protobuf::util::JsonParseOptions{ };
    REQUIRE( JsonStringToMessage( spb_serialized, &gpb, parse_options ).ok( ) );

    if constexpr( is_gpb_repeated< GPB > )
    {
        REQUIRE( gpb.value( ).size( ) == opt_size( spb.value ) );
        for( size_t i = 0; i < opt_size( spb.value ); ++i )
        {
            using value_type = typename decltype( SPB::value )::value_type;
            if constexpr( std::is_enum_v< value_type > )
            {
                REQUIRE( enum_value( spb.value[ i ] ) == gpb.value( i ) );
            }
            else
            {
                REQUIRE( gpb.value( i ) == spb.value[ i ] );
            }
        }
    }
    else if constexpr( std::is_enum_v< T > )
    {
        REQUIRE( enum_value( spb.value ) == gpb.value( ) );
    }
    else
    {
        auto gpb_value = gpb.value( );
        REQUIRE( spb.value == gpb_value );
    }
    auto json_string                         = std::string( );
    auto print_options                       = google::protobuf::util::JsonPrintOptions( );
    print_options.preserve_proto_field_names = true;

    REQUIRE( MessageToJsonString( gpb, &json_string, print_options ).ok( ) );
    REQUIRE( spb::json::deserialize< SPB >( json_string ).value == spb.value );

    print_options.preserve_proto_field_names = false;
    json_string.clear( );
    REQUIRE( MessageToJsonString( gpb, &json_string, print_options ).ok( ) );
    REQUIRE( spb::json::deserialize< SPB >( json_string ).value == spb.value );
    if constexpr( std::is_integral_v< T > && sizeof( T ) < sizeof( int64_t ) )
    {
        auto gpb_value = gpb.value( );
        if( sizeof( gpb_value ) < sizeof( int64_t ) )
        {
            REQUIRE( json_string == spb_serialized );
        }
    }
}

template < typename GPB, typename SPB >
void gpb_compatibility( const SPB & spb )
{
    SUBCASE( "gpb serialize/deserialize" )
    {
        gpb_test< GPB, SPB >( spb );
        gpb_test< GPB, SPB >( spb, { .delimited = true } );
    }
    SUBCASE( "json serialize/deserialize" )
    {
        gpb_json< GPB, SPB >( spb );
    }
}

template < typename GPB, typename SPB, typename POD >
void gpb_compatibility_enum( )
{
    using T = typename ExtractOptional< std::decay_t< decltype( SPB::value ) > >::type;

    gpb_compatibility< GPB >( SPB{ .value = SPB::Enum::Enum_min } );
    gpb_compatibility< GPB >( SPB{ .value = SPB::Enum::Enum_max } );
    gpb_compatibility< GPB >( SPB{ .value = SPB::Enum::Enum_value } );

    CHECK( std::is_same_v< std::underlying_type_t< T >, POD > );

    if constexpr( !std::is_same_v< std::optional< T >, std::decay_t< decltype( SPB::value ) > > )
    {
        CHECK( sizeof( SPB ) == sizeof( POD ) );
    }
}

template < typename GPB, typename SPB >
void gpb_compatibility_enum_array( )
{
    gpb_compatibility< GPB >( SPB{ .value = { SPB::Enum::Enum_min } } );
    gpb_compatibility< GPB >( SPB{ .value = { SPB::Enum::Enum_max } } );
    gpb_compatibility< GPB >( SPB{ .value = { SPB::Enum::Enum_value } } );
    gpb_compatibility< GPB >( SPB{ .value = {
                                       SPB::Enum::Enum_min,
                                       SPB::Enum::Enum_max,
                                       SPB::Enum::Enum_value,
                                   } } );
}

template < typename GPB, typename SPB, typename POD >
void gpb_compatibility_value( )
{
    using T = typename ExtractOptional< std::decay_t< decltype( SPB::value ) > >::type;

    gpb_compatibility< GPB >( SPB{ .value = 0 } );
    gpb_compatibility< GPB >( SPB{ .value = 0x42 } );
    gpb_compatibility< GPB >( SPB{ .value = 0x7f } );
    gpb_compatibility< GPB >( SPB{ .value = std::numeric_limits< T >::max( ) } );
    gpb_compatibility< GPB >( SPB{ .value = std::numeric_limits< T >::max( ) / 2 } );
    gpb_compatibility< GPB >( SPB{ .value = std::numeric_limits< T >::min( ) } );
    gpb_compatibility< GPB >( SPB{ .value = T( -2 ) } );
    CHECK( std::is_same_v< T, POD > );

    if constexpr( !std::is_same_v< std::optional< T >, std::decay_t< decltype( SPB::value ) > > )
    {
        CHECK( sizeof( SPB ) == sizeof( POD ) );
    }
}

template < typename GPB, typename SPB, typename POD >
void gpb_compatibility_bitfield_value( std::initializer_list< POD > values )
{
    using T = std::decay_t< decltype( SPB::value ) >;
    static_assert( std::is_same_v< T, POD > );

    for( auto value : values )
    {
        gpb_compatibility< GPB >( SPB{ .value = value } );
    }
}

template < typename GPB, typename SPB >
void gpb_compatibility_array( )
{
    using T = typename decltype( SPB::value )::value_type;

    gpb_compatibility< GPB >( SPB{ .value = { 0 } } );
    gpb_compatibility< GPB >( SPB{ .value = { 0x42 } } );
    gpb_compatibility< GPB >( SPB{ .value = { 0, 0x42, 0x7f } } );
    gpb_compatibility< GPB >( SPB{ .value = {
                                       0,
                                       0x42,
                                       0x7f,
                                       std::numeric_limits< T >::max( ),
                                       std::numeric_limits< T >::max( ) / 2,
                                       std::numeric_limits< T >::min( ),
                                       T( -2 ),
                                   } } );
}

}// namespace

using namespace std::literals;

TEST_CASE( "string" )
{
    SUBCASE( "utf8" )
    {
        for( auto i = 0U; i < 0x10ffff; i++ )
        {
            char buffer[ 4 ];
            auto value = std::string( buffer, spb::detail::utf8::encode_point( i, buffer ) );
            if( value.empty( ) )
            {
                continue;
            }

            gpb_compatibility< Test::Scalar::gpb::ReqString >( Test::Scalar::ReqString{ .value = value } );
        }
    }
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Scalar::gpb::ReqString >( Test::Scalar::ReqString{ .value = "hello" } );
        gpb_compatibility< Test::Scalar::gpb::ReqString >( Test::Scalar::ReqString{ .value = "\"\\/\b\f\n\r\t" } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Scalar::gpb::OptString >( Test::Scalar::OptString{ .value = "hello" } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Scalar::gpb::RepString >( Test::Scalar::RepString{ .value = { "hello" } } );
        gpb_compatibility< Test::Scalar::gpb::RepString >( Test::Scalar::RepString{ .value = { "hello", "world" } } );
    }
    SUBCASE( "fixed" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Scalar::gpb::ReqStringFixed >( Test::Scalar::ReqStringFixed{ .value = to_string( "hello1" ) } );
            gpb_compatibility< Test::Scalar::gpb::ReqStringFixed >( Test::Scalar::ReqStringFixed{ .value = to_string( "\"\\/\n\r\t" ) } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Scalar::gpb::OptStringFixed >( Test::Scalar::OptStringFixed{ .value = to_string( "hello1" ) } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Scalar::gpb::RepStringFixed >( Test::Scalar::RepStringFixed{ .value = { to_string( "hello1" ) } } );
            gpb_compatibility< Test::Scalar::gpb::RepStringFixed >( Test::Scalar::RepStringFixed{ .value = { to_string( "hello1" ), to_string( "world1" ) } } );
        }
    }
}
TEST_CASE( "bool" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Scalar::gpb::ReqBool >( Test::Scalar::ReqBool{ .value = true } );
        gpb_compatibility< Test::Scalar::gpb::ReqBool >( Test::Scalar::ReqBool{ .value = false } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Scalar::gpb::OptBool >( Test::Scalar::OptBool{ .value = true } );
        gpb_compatibility< Test::Scalar::gpb::OptBool >( Test::Scalar::OptBool{ .value = false } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Scalar::gpb::RepBool >( Test::Scalar::RepBool{ .value = { true } } );
        gpb_compatibility< Test::Scalar::gpb::RepBool >( Test::Scalar::RepBool{ .value = { true, false } } );

        SUBCASE( "packed" )
        {
            gpb_compatibility< Test::Scalar::gpb::RepPackBool >( Test::Scalar::RepPackBool{ .value = { true } } );
            gpb_compatibility< Test::Scalar::gpb::RepPackBool >( Test::Scalar::RepPackBool{ .value = { true, false } } );
        }
    }
}
TEST_CASE( "int" )
{
    SUBCASE( "varint8" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt8_1, Test::Scalar::ReqInt8_1, int8_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt8_2, Test::Scalar::ReqInt8_2, int8_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqInt8, Test::Scalar::ReqInt8, int8_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptInt8, Test::Scalar::OptInt8, int8_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepInt8, Test::Scalar::RepInt8 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackInt8, Test::Scalar::RepPackInt8 >( );
            }
        }
    }
    SUBCASE( "varuint8" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint8_1, Test::Scalar::ReqUint8_1, uint8_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint8_2, Test::Scalar::ReqUint8_2, uint8_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqUint8, Test::Scalar::ReqUint8, uint8_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptUint8, Test::Scalar::OptUint8, uint8_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepUint8, Test::Scalar::RepUint8 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackUint8, Test::Scalar::RepPackUint8 >( );
            }
        }
    }
    SUBCASE( "varint16" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt16_1, Test::Scalar::ReqInt16_1, int16_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt16_2, Test::Scalar::ReqInt16_2, int16_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqInt16, Test::Scalar::ReqInt16, int16_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptInt16, Test::Scalar::OptInt16, int16_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepInt16, Test::Scalar::RepInt16 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackInt16, Test::Scalar::RepPackInt16 >( );
            }
        }
    }
    SUBCASE( "varuint16" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint16_1, Test::Scalar::ReqUint16_1, uint16_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint16_2, Test::Scalar::ReqUint16_2, uint16_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqUint16, Test::Scalar::ReqUint16, uint16_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptUint16, Test::Scalar::OptUint16, uint16_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepUint16, Test::Scalar::RepUint16 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackUint16, Test::Scalar::RepPackUint16 >( );
            }
        }
    }
    SUBCASE( "varint32" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt32_1, Test::Scalar::ReqInt32_1, int32_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt32_2, Test::Scalar::ReqInt32_2, int32_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqInt32, Test::Scalar::ReqInt32, int32_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptInt32, Test::Scalar::OptInt32, int32_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepInt32, Test::Scalar::RepInt32 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackInt32, Test::Scalar::RepPackInt32 >( );
            }
        }
    }
    SUBCASE( "varuint32" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint32_1, Test::Scalar::ReqUint32_1, uint32_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint32_2, Test::Scalar::ReqUint32_2, uint32_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqUint32, Test::Scalar::ReqUint32, uint32_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptUint32, Test::Scalar::OptUint32, uint32_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepUint32, Test::Scalar::RepUint32 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackUint32, Test::Scalar::RepPackUint32 >( );
            }
        }
    }
    SUBCASE( "varint64" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt64_1, Test::Scalar::ReqInt64_1, int64_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqInt64_2, Test::Scalar::ReqInt64_2, int64_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqInt64, Test::Scalar::ReqInt64, int64_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptInt64, Test::Scalar::OptInt64, int64_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepInt64, Test::Scalar::RepInt64 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackInt64, Test::Scalar::RepPackInt64 >( );
            }
        }
    }
    SUBCASE( "varuint64" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint8_1, Test::Scalar::ReqUint8_1, uint8_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqUint8_2, Test::Scalar::ReqUint8_2, uint8_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqUint64, Test::Scalar::ReqUint64, uint64_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptUint64, Test::Scalar::OptUint64, uint64_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepUint64, Test::Scalar::RepUint64 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackUint64, Test::Scalar::RepPackUint64 >( );
            }
        }
    }
    SUBCASE( "svarint8" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint8_1, Test::Scalar::ReqSint8_1, int8_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint8_2, Test::Scalar::ReqSint8_2, int8_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqSint8, Test::Scalar::ReqSint8, int8_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptSint8, Test::Scalar::OptSint8, int8_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepSint8, Test::Scalar::RepSint8 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackSint8, Test::Scalar::RepPackSint8 >( );
            }
        }
    }
    SUBCASE( "svarint16" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint16_1, Test::Scalar::ReqSint16_1, int16_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint16_2, Test::Scalar::ReqSint16_2, int16_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqSint16, Test::Scalar::ReqSint16, int16_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptSint16, Test::Scalar::OptSint16, int16_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepSint16, Test::Scalar::RepSint16 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackSint16, Test::Scalar::RepPackSint16 >( );
            }
        }
    }
    SUBCASE( "svarint32" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint32_1, Test::Scalar::ReqSint32_1, int32_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint32_2, Test::Scalar::ReqSint32_2, int32_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqSint32, Test::Scalar::ReqSint32, int32_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptSint32, Test::Scalar::OptSint32, int32_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepSint32, Test::Scalar::RepSint32 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackSint32, Test::Scalar::RepPackSint32 >( );
            }
        }
    }
    SUBCASE( "svarint64" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint64_1, Test::Scalar::ReqSint64_1, int64_t >( { -1, 0 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSint64_2, Test::Scalar::ReqSint64_2, int64_t >( { -2, -1, 0, 1 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqSint64, Test::Scalar::ReqSint64, int64_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptSint64, Test::Scalar::OptSint64, int64_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepSint64, Test::Scalar::RepSint64 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackSint64, Test::Scalar::RepPackSint64 >( );
            }
        }
    }
    SUBCASE( "fixed32" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed32_32_1, Test::Scalar::ReqFixed32_32_1, uint32_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed32_32_2, Test::Scalar::ReqFixed32_32_2, uint32_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqFixed32, Test::Scalar::ReqFixed32, uint32_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptFixed32, Test::Scalar::OptFixed32, uint32_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepFixed32, Test::Scalar::RepFixed32 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackFixed32, Test::Scalar::RepPackFixed32 >( );
            }
        }
        SUBCASE( "uint8" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed32_8_1, Test::Scalar::ReqFixed32_8_1, uint8_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed32_8_2, Test::Scalar::ReqFixed32_8_2, uint8_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqFixed32_8, Test::Scalar::ReqFixed32_8, uint8_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptFixed32_8, Test::Scalar::OptFixed32_8, uint8_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepFixed32_8, Test::Scalar::RepFixed32_8 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackFixed32_8, Test::Scalar::RepPackFixed32_8 >( );
                }
            }
        }
        SUBCASE( "uint16" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed32_16_1, Test::Scalar::ReqFixed32_16_1, uint16_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed32_16_2, Test::Scalar::ReqFixed32_16_2, uint16_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqFixed32_16, Test::Scalar::ReqFixed32_16, uint16_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptFixed32_16, Test::Scalar::OptFixed32_16, uint16_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepFixed32_16, Test::Scalar::RepFixed32_16 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackFixed32_16, Test::Scalar::RepPackFixed32_16 >( );
                }
            }
        }
    }
    SUBCASE( "fixed64" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_8_1, Test::Scalar::ReqFixed64_64_1, uint64_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_8_2, Test::Scalar::ReqFixed64_64_2, uint64_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqFixed64, Test::Scalar::ReqFixed64, uint64_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptFixed64, Test::Scalar::OptFixed64, uint64_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepFixed64, Test::Scalar::RepFixed64 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackFixed64, Test::Scalar::RepPackFixed64 >( );
            }
        }
        SUBCASE( "uint8" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_8_1, Test::Scalar::ReqFixed64_8_1, uint8_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_8_2, Test::Scalar::ReqFixed64_8_2, uint8_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqFixed64_8, Test::Scalar::ReqFixed64_8, uint8_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptFixed64_8, Test::Scalar::OptFixed64_8, uint8_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepFixed64_8, Test::Scalar::RepFixed64_8 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackFixed64_8, Test::Scalar::RepPackFixed64_8 >( );
                }
            }
        }
        SUBCASE( "uint16" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_16_1, Test::Scalar::ReqFixed64_16_1, uint16_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_16_2, Test::Scalar::ReqFixed64_16_2, uint16_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqFixed64_16, Test::Scalar::ReqFixed64_16, uint16_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptFixed64_16, Test::Scalar::OptFixed64_16, uint16_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepFixed64_16, Test::Scalar::RepFixed64_16 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackFixed64_16, Test::Scalar::RepPackFixed64_16 >( );
                }
            }
        }
        SUBCASE( "uint32" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_32_1, Test::Scalar::ReqFixed64_32_1, uint32_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqFixed64_32_2, Test::Scalar::ReqFixed64_32_2, uint32_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqFixed64_32, Test::Scalar::ReqFixed64_32, uint32_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptFixed64_32, Test::Scalar::OptFixed64_32, uint32_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepFixed64_32, Test::Scalar::RepFixed64_32 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackFixed64_32, Test::Scalar::RepPackFixed64_32 >( );
                }
            }
        }
    }
    SUBCASE( "sfixed32" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed32_32_1, Test::Scalar::ReqSfixed32_32_1, int32_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed32_32_2, Test::Scalar::ReqSfixed32_32_2, int32_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqSfixed32, Test::Scalar::ReqSfixed32, int32_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptSfixed32, Test::Scalar::OptSfixed32, int32_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepSfixed32, Test::Scalar::RepSfixed32 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackSfixed32, Test::Scalar::RepPackSfixed32 >( );
            }
        }
        SUBCASE( "int8" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed32_8_1, Test::Scalar::ReqSfixed32_8_1, int8_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed32_8_2, Test::Scalar::ReqSfixed32_8_2, int8_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqSfixed32_8, Test::Scalar::ReqSfixed32_8, int8_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptSfixed32_8, Test::Scalar::OptSfixed32_8, int8_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepSfixed32_8, Test::Scalar::RepSfixed32_8 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackSfixed32_8, Test::Scalar::RepPackSfixed32_8 >( );
                }
            }
        }
        SUBCASE( "int16" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed32_16_1, Test::Scalar::ReqSfixed32_16_1, int16_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed32_16_2, Test::Scalar::ReqSfixed32_16_2, int16_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqSfixed32_16, Test::Scalar::ReqSfixed32_16, int16_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptSfixed32_16, Test::Scalar::OptSfixed32_16, int16_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepSfixed32_16, Test::Scalar::RepSfixed32_16 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackSfixed32_16, Test::Scalar::RepPackSfixed32_16 >( );
                }
            }
        }
    }
    SUBCASE( "sfixed64" )
    {
        SUBCASE( "bitfield" )
        {
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_64_1, Test::Scalar::ReqSfixed64_64_1, int64_t >( { 0, 1 } );
            gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_64_2, Test::Scalar::ReqSfixed64_64_2, int64_t >( { 0, 1, 2, 3 } );
        }
        SUBCASE( "required" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::ReqSfixed64, Test::Scalar::ReqSfixed64, int64_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_value< Test::Scalar::gpb::OptSfixed64, Test::Scalar::OptSfixed64, int64_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_array< Test::Scalar::gpb::RepSfixed64, Test::Scalar::RepSfixed64 >( );

            SUBCASE( "packed" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepPackSfixed64, Test::Scalar::RepPackSfixed64 >( );
            }
        }
        SUBCASE( "int8" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_8_1, Test::Scalar::ReqSfixed64_8_1, int8_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_8_2, Test::Scalar::ReqSfixed64_8_2, int8_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqSfixed64_8, Test::Scalar::ReqSfixed64_8, int8_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptSfixed64_8, Test::Scalar::OptSfixed64_8, int8_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepSfixed64_8, Test::Scalar::RepSfixed64_8 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackSfixed64_8, Test::Scalar::RepPackSfixed64_8 >( );
                }
            }
        }
        SUBCASE( "int16" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_16_1, Test::Scalar::ReqSfixed64_16_1, int16_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_16_2, Test::Scalar::ReqSfixed64_16_2, int16_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqSfixed64_16, Test::Scalar::ReqSfixed64_16, int16_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptSfixed64_16, Test::Scalar::OptSfixed64_16, int16_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepSfixed64_16, Test::Scalar::RepSfixed64_16 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackSfixed64_16, Test::Scalar::RepPackSfixed64_16 >( );
                }
            }
        }
        SUBCASE( "int32" )
        {
            SUBCASE( "bitfield" )
            {
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_32_1, Test::Scalar::ReqSfixed64_32_1, int32_t >( { 0, 1 } );
                gpb_compatibility_bitfield_value< Test::Scalar::gpb::ReqSfixed64_32_2, Test::Scalar::ReqSfixed64_32_2, int32_t >( { 0, 1, 2, 3 } );
            }
            SUBCASE( "required" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::ReqSfixed64_32, Test::Scalar::ReqSfixed64_32, int32_t >( );
            }
            SUBCASE( "optional" )
            {
                gpb_compatibility_value< Test::Scalar::gpb::OptSfixed64_32, Test::Scalar::OptSfixed64_32, int32_t >( );
            }
            SUBCASE( "repeated" )
            {
                gpb_compatibility_array< Test::Scalar::gpb::RepSfixed64_32, Test::Scalar::RepSfixed64_32 >( );

                SUBCASE( "packed" )
                {
                    gpb_compatibility_array< Test::Scalar::gpb::RepPackSfixed64_32, Test::Scalar::RepPackSfixed64_32 >( );
                }
            }
        }
    }
}
TEST_CASE( "float" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Scalar::gpb::ReqFloat >( Test::Scalar::ReqFloat{ .value = 42.0 } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Scalar::gpb::OptFloat >( Test::Scalar::OptFloat{ .value = 42.3 } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Scalar::gpb::RepFloat >( Test::Scalar::RepFloat{ .value = { 42.3 } } );
        gpb_compatibility< Test::Scalar::gpb::RepFloat >( Test::Scalar::RepFloat{ .value = { 42.0, 42.3 } } );
    }
}
TEST_CASE( "double" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Scalar::gpb::ReqDouble >( Test::Scalar::ReqDouble{ .value = 42.0 } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Scalar::gpb::OptDouble >( Test::Scalar::OptDouble{ .value = 42.3 } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Scalar::gpb::RepDouble >( Test::Scalar::RepDouble{ .value = { 42.3 } } );
        gpb_compatibility< Test::Scalar::gpb::RepDouble >( Test::Scalar::RepDouble{ .value = { 42.3, 3.0 } } );
    }
}
TEST_CASE( "bytes" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility< Test::Scalar::gpb::ReqBytes >( Test::Scalar::ReqBytes{ .value = to_bytes( "hello" ) } );
        gpb_compatibility< Test::Scalar::gpb::ReqBytes >( Test::Scalar::ReqBytes{ .value = to_bytes( "\x00\x01\x02"sv ) } );
        gpb_compatibility< Test::Scalar::gpb::ReqBytes >( Test::Scalar::ReqBytes{ .value = to_bytes( "\x00\x01\x02\x03\x04"sv ) } );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility< Test::Scalar::gpb::OptBytes >( Test::Scalar::OptBytes{ .value = to_bytes( "hello" ) } );
        gpb_compatibility< Test::Scalar::gpb::OptBytes >( Test::Scalar::OptBytes{ .value = to_bytes( "\x00\x01\x02"sv ) } );
        gpb_compatibility< Test::Scalar::gpb::OptBytes >( Test::Scalar::OptBytes{ .value = to_bytes( "\x00\x01\x02\x03\x04"sv ) } );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility< Test::Scalar::gpb::RepBytes >( Test::Scalar::RepBytes{ .value = { to_bytes( "hello" ) } } );
        gpb_compatibility< Test::Scalar::gpb::RepBytes >( Test::Scalar::RepBytes{ .value = { to_bytes( "\x00\x01\x02"sv ), to_bytes( "hello" ) } } );
        gpb_compatibility< Test::Scalar::gpb::RepBytes >( Test::Scalar::RepBytes{ .value = { to_bytes( "\x00\x01\x02\x03\x04"sv ), to_bytes( "\x00\x01\x02"sv ), to_bytes( "hello" ) } } );
    }
    SUBCASE( "fixed" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility< Test::Scalar::gpb::ReqBytesFixed >( Test::Scalar::ReqBytesFixed{ .value = to_array( "12345678" ) } );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility< Test::Scalar::gpb::OptBytesFixed >( Test::Scalar::OptBytesFixed{ .value = to_array( "12345678" ) } );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility< Test::Scalar::gpb::RepBytesFixed >( Test::Scalar::RepBytesFixed{ .value = { to_array( "12345678" ) } } );
            gpb_compatibility< Test::Scalar::gpb::RepBytesFixed >( Test::Scalar::RepBytesFixed{ .value = { to_array( "12345678" ), to_array( "87654321" ) } } );
        }
    }
}
TEST_CASE( "enum" )
{
    SUBCASE( "required" )
    {
        gpb_compatibility_enum< Test::Scalar::gpb::ReqEnum, Test::Scalar::ReqEnum, int32_t >( );
    }
    SUBCASE( "optional" )
    {
        gpb_compatibility_enum< Test::Scalar::gpb::OptEnum, Test::Scalar::OptEnum, int32_t >( );
    }
    SUBCASE( "repeated" )
    {
        gpb_compatibility_enum_array< Test::Scalar::gpb::RepEnum, Test::Scalar::RepEnum >( );
    }

    SUBCASE( "int8" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::ReqEnumInt8, Test::Scalar::ReqEnumInt8, int8_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::OptEnumInt8, Test::Scalar::OptEnumInt8, int8_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_enum_array< Test::Scalar::gpb::RepEnumInt8, Test::Scalar::RepEnumInt8 >( );
        }
    }

    SUBCASE( "uint8" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::ReqEnumUint8, Test::Scalar::ReqEnumUint8, uint8_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::OptEnumUint8, Test::Scalar::OptEnumUint8, uint8_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_enum_array< Test::Scalar::gpb::RepEnumUint8, Test::Scalar::RepEnumUint8 >( );
        }
    }

    SUBCASE( "int16" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::ReqEnumInt16, Test::Scalar::ReqEnumInt16, int16_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::OptEnumInt16, Test::Scalar::OptEnumInt16, int16_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_enum_array< Test::Scalar::gpb::RepEnumInt16, Test::Scalar::RepEnumInt16 >( );
        }
    }

    SUBCASE( "uint16" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::ReqEnumUint16, Test::Scalar::ReqEnumUint16, uint16_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::OptEnumUint16, Test::Scalar::OptEnumUint16, uint16_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_enum_array< Test::Scalar::gpb::RepEnumUint16, Test::Scalar::RepEnumUint16 >( );
        }
    }

    SUBCASE( "int32" )
    {
        SUBCASE( "required" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::ReqEnumInt32, Test::Scalar::ReqEnumInt32, int32_t >( );
        }
        SUBCASE( "optional" )
        {
            gpb_compatibility_enum< Test::Scalar::gpb::OptEnumInt32, Test::Scalar::OptEnumInt32, int32_t >( );
        }
        SUBCASE( "repeated" )
        {
            gpb_compatibility_enum_array< Test::Scalar::gpb::RepEnumInt32, Test::Scalar::RepEnumInt32 >( );
        }
    }
}
TEST_CASE( "variant" )
{
    SUBCASE( "int" )
    {
        auto gpb            = Test::gpb::Variant( );
        auto spb            = Test::Variant{ .oneof_field = 0x42U };
        auto spb_serialized = spb::pb::serialize( spb );

        REQUIRE( gpb.ParseFromString( spb_serialized ) );
        REQUIRE( gpb.var_int( ) == std::get< 0 >( spb.oneof_field ) );

        SUBCASE( "deserialize" )
        {
            auto gpb_serialized = std::string( );
            REQUIRE( gpb.SerializeToString( &gpb_serialized ) );
            REQUIRE( spb::pb::deserialize< Test::Variant >( gpb_serialized ) == spb );
        }
    }
    SUBCASE( "string" )
    {
        auto gpb            = Test::gpb::Variant( );
        auto spb            = Test::Variant{ .oneof_field = "hello" };
        auto spb_serialized = spb::pb::serialize( spb );

        REQUIRE( gpb.ParseFromString( spb_serialized ) );
        REQUIRE( gpb.var_string( ) == std::get< 1 >( spb.oneof_field ) );

        SUBCASE( "deserialize" )
        {
            auto gpb_serialized = std::string( );
            REQUIRE( gpb.SerializeToString( &gpb_serialized ) );
            REQUIRE( spb::pb::deserialize< Test::Variant >( gpb_serialized ) == spb );
        }
    }
    SUBCASE( "bytes" )
    {
        auto gpb            = Test::gpb::Variant( );
        auto spb            = Test::Variant{ .oneof_field = to_bytes( "hello" ) };
        auto spb_serialized = spb::pb::serialize( spb );

        REQUIRE( gpb.ParseFromString( spb_serialized ) );
        REQUIRE( gpb.var_bytes( ) == std::get< 2 >( spb.oneof_field ) );

        SUBCASE( "deserialize" )
        {
            auto gpb_serialized = std::string( );
            REQUIRE( gpb.SerializeToString( &gpb_serialized ) );
            REQUIRE( spb::pb::deserialize< Test::Variant >( gpb_serialized ) == spb );
        }
    }
    SUBCASE( "name" )
    {
        auto gpb            = Test::gpb::Variant( );
        auto spb            = Test::Variant{ .oneof_field = Test::Name{ .name = "John" } };
        auto spb_serialized = spb::pb::serialize( spb );

        REQUIRE( gpb.ParseFromString( spb_serialized ) );
        REQUIRE( gpb.has_name( ) );

        REQUIRE( gpb.name( ).name( ) == std::get< 3 >( spb.oneof_field ).name );

        SUBCASE( "deserialize" )
        {
            auto gpb_serialized = std::string( );
            REQUIRE( gpb.SerializeToString( &gpb_serialized ) );
            REQUIRE( spb::pb::deserialize< Test::Variant >( gpb_serialized ) == spb );
        }
    }
}
TEST_CASE( "person" )
{
    auto gpb = PhoneBook::gpb::Person( );
    auto spb = PhoneBook::Person{
        .name   = "John Doe",
        .id     = 123,
        .email  = "QXUeh@example.com",
        .phones = {
            PhoneBook::Person::PhoneNumber{
                .number = "555-4321",
                .type   = PhoneBook::Person::PhoneType::HOME,
            },
            PhoneBook::Person::PhoneNumber{
                .number = "999-1234",
                .type   = PhoneBook::Person::PhoneType::MOBILE,
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
        REQUIRE( spb::pb::deserialize< PhoneBook::Person >( gpb_serialized ) == spb );
        REQUIRE( gpb_serialized == spb_serialized );
    }
    SUBCASE( "json serialize/deserialize" )
    {
        auto spb_json = spb::json::serialize( spb );

        auto parse_options = google::protobuf::util::JsonParseOptions{ };
        REQUIRE( JsonStringToMessage( spb_json, &gpb, parse_options ).ok( ) );

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

        REQUIRE( MessageToJsonString( gpb, &json_string, print_options ).ok( ) );
        REQUIRE( spb::json::deserialize< PhoneBook::Person >( json_string ) == spb );

        print_options.preserve_proto_field_names = false;
        json_string.clear( );
        REQUIRE( MessageToJsonString( gpb, &json_string, print_options ).ok( ) );
        REQUIRE( spb::json::deserialize< PhoneBook::Person >( json_string ) == spb );
        REQUIRE( json_string == spb_json );
    }
}
