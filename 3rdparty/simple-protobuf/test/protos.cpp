#include "mesos.pb.h"
#include <cstdio>
#include <filesystem>
#include <type_traits>

static auto load_file( const std::filesystem::path & file_path ) -> std::string
{
    const auto file_size = std::filesystem::file_size( file_path );
    auto file_content    = std::string( file_size, '\0' );

    if( auto * p_file = fopen( file_path.string( ).c_str( ), "rb" ); p_file )
    {
        const auto read = fread( file_content.data( ), 1, file_content.size( ), p_file );
        fclose( p_file );
        file_content.resize( read );
        return file_content;
    }
    perror( file_path.string( ).c_str( ) );
    throw std::system_error( std::make_error_code( std::errc( errno ) ) );
}

static void save_file( const std::filesystem::path & file_path, std::string_view file_content )
{
    if( auto * p_file = fopen( file_path.string( ).c_str( ), "wb" ); p_file )
    {
        const auto written = fwrite( file_content.data( ), 1, file_content.size( ), p_file );
        fclose( p_file );
        if( written == file_content.size( ) )
        {
            return;
        }
    }
    perror( file_path.string( ).c_str( ) );
    throw std::system_error( std::make_error_code( std::errc( errno ) ) );
}

auto main( int argc, char * argv[] ) -> int
{
    if( argc != 3 )
    {
        fprintf( stderr, "usage %s, <in.file.json> <out.file.json>\n", argv[ 0 ] );
        return 1;
    }

    auto volume = spb::json::deserialize< mesos::Volume >( load_file( argv[ 1 ] ) );
    auto json   = spb::json::serialize( volume );
    save_file( argv[ 2 ], json );
    return 0;
}