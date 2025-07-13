#include <algorithm>
#include <cctype>
#include <spb/json/deserialize.hpp>
#include <spb/json/serialize.hpp>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <spb/json/base64.h>

namespace
{
auto base64_encode_size( std::string_view value ) -> size_t
{
    auto stream = spb::json::detail::ostream( nullptr );
    spb::json::detail::base64_encode( stream, { reinterpret_cast< const std::byte * >( value.data( ) ), value.size( ) } );
    return stream.size( );
}

auto base64_encode( std::string_view value ) -> std::string
{
    auto encode_size = base64_encode_size( value );
    auto result      = std::string( encode_size, '\0' );
    auto writer      = [ ptr = result.data( ) ]( const void * data, size_t size ) mutable
    {
        memcpy( ptr, data, size );
        ptr += size;
    };
    auto stream = spb::json::detail::ostream( writer );
    spb::json::detail::base64_encode( stream, { reinterpret_cast< const std::byte * >( value.data( ) ), value.size( ) } );
    return result;
}

auto generate_random_bytes( size_t size ) -> std::string
{
    auto result = std::string( size, 0 );
    for( auto & c : result )
    {
        c = ( char ) rand( );
    }
    return result;
}

auto base64_decode( std::string_view value ) -> std::string
{
    auto reader = [ ptr = value.data( ), end = value.data( ) + value.size( ) ]( void * data, size_t size ) mutable -> size_t
    {
        size_t bytes_left = end - ptr;

        size = std::min( size, bytes_left );
        memcpy( data, ptr, size );
        ptr += size;
        return size;
    };

    auto stream = spb::json::detail::istream( reader );
    auto result = std::vector< std::byte >( );

    spb::json::detail::base64_decode_string( result, stream );
    REQUIRE_THROWS( ( void ) stream.view( 1 ) );

    return { std::string( reinterpret_cast< const char * >( result.data( ) ), reinterpret_cast< const char * >( result.data( ) + result.size( ) ) ) };
}

template < size_t N >
auto base64_decode_fixed( std::string_view value ) -> std::string
{
    auto reader = [ ptr = value.data( ), end = value.data( ) + value.size( ) ]( void * data, size_t size ) mutable -> size_t
    {
        size_t bytes_left = end - ptr;

        size = std::min( size, bytes_left );
        memcpy( data, ptr, size );
        ptr += size;
        return size;
    };

    auto stream = spb::json::detail::istream( reader );
    auto result = std::array< std::byte, N >( );

    spb::json::detail::base64_decode_string( result, stream );
    REQUIRE_THROWS( ( void ) stream.view( 1 ) );

    return { std::string( reinterpret_cast< const char * >( result.data( ) ), reinterpret_cast< const char * >( result.data( ) + result.size( ) ) ) };
}

}// namespace

TEST_CASE( "base64" )
{
    SUBCASE( "base64_encode" )
    {
        CHECK( base64_encode( "hello world" ) == "aGVsbG8gd29ybGQ=" );

        CHECK( base64_encode( "" ) == "" );
        CHECK( base64_encode( "f" ) == "Zg==" );
        CHECK( base64_encode( "fo" ) == "Zm8=" );
        CHECK( base64_encode( "foo" ) == "Zm9v" );
        CHECK( base64_encode( "foob" ) == "Zm9vYg==" );
        CHECK( base64_encode( "fooba" ) == "Zm9vYmE=" );
        CHECK( base64_encode( "foobar" ) == "Zm9vYmFy" );
    }
    SUBCASE( "base64_decode" )
    {
        SUBCASE( "invalid" )
        {
            CHECK_THROWS( base64_decode( R"(")" ) );
            CHECK_THROWS( base64_decode( R"("Zg=")" ) );
            CHECK_THROWS( base64_decode( R"("Zg")" ) );
            CHECK_THROWS( base64_decode( R"("Z")" ) );
            CHECK_THROWS( base64_decode( R"("Zg==)" ) );
        }

        CHECK( base64_decode( R"("")" ) == "" );
        CHECK( base64_decode( R"("Zg==")" ) == "f" );
        CHECK( base64_decode( R"("Zm8=")" ) == "fo" );
        CHECK( base64_decode( R"("Zm9v")" ) == "foo" );
        CHECK( base64_decode( R"("Zm9vYg==")" ) == "foob" );
        CHECK( base64_decode( R"("Zm9vYmE=")" ) == "fooba" );
        CHECK( base64_decode( R"("Zm9vYmFy")" ) == "foobar" );
        CHECK( base64_decode_fixed< 6 >( R"("Zm9vYmFy")" ) == "foobar" );
        CHECK_THROWS( base64_decode_fixed< 5 >( R"("Zm9vYmFy")" ) );
        CHECK_THROWS( base64_decode_fixed< 7 >( R"("Zm9vYmFy")" ) );
        CHECK_THROWS( base64_decode_fixed< 3 >( R"("Zm9vYmFy")" ) );
        CHECK_THROWS( base64_decode( R"("Zm9vY!Fy")" ) );
        CHECK_THROWS( base64_decode( R"("Zm9vY^Fy")" ) );
        CHECK_THROWS( base64_decode( R"("!m9vYmFy")" ) );
        CHECK_THROWS( base64_decode( R"("&m9vYmFy")" ) );
    }
    SUBCASE( "encode/decode" )
    {
        const auto buffer_max_size = SPB_READ_BUFFER_SIZE * 10;
        for( auto i = 8U; i <= buffer_max_size; i++ )
        {
            srand( i );

            auto bytes   = generate_random_bytes( i );
            auto encoded = base64_encode( bytes );
            CHECK( ( encoded.size( ) % 4 ) == 0 );
            CHECK( std::all_of( encoded.begin( ), encoded.end( ), isprint ) );
            {
                auto decoded = base64_decode( '"' + encoded + '"' );
                CHECK( decoded == bytes );
            }
            if( i == 8 )
            {
                auto decoded = base64_decode_fixed< 8 >( '"' + encoded + '"' );
                CHECK( decoded == bytes );
            }
            if( i == buffer_max_size )
            {
                auto decoded = base64_decode_fixed< buffer_max_size >( '"' + encoded + '"' );
                CHECK( decoded == bytes );
            }
        }
    }
}