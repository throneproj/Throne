/***************************************************************************\
* Name        : spb-protoc                                                  *
* Description : proto file compiler                                         *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper/dumper.h"
#include "parser/parser.h"
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
using namespace std::literals;
namespace fs = std::filesystem;

constexpr auto opt_version = "--version"sv;
constexpr auto opt_v       = "-v"sv;

constexpr auto opt_help = "--help"sv;
constexpr auto opt_h    = "-h"sv;

constexpr auto opt_cpp_out = "--cpp_out="sv;

constexpr auto opt_proto_path = "--proto_path="sv;
constexpr auto opt_ipath      = "-IPATH="sv;

void print_usage( )
{
    std::cout
        << "Usage: spb-protoc [OPTION] PROTO_FILES\n"
        << "Parse PROTO_FILES and generate C++ source files.\n"
        << "  -IPATH=, --proto_path=PATH  Specify the directory in which to search for imports.\n"
        << "May be specified multiple times. directories will be searched in order.\n"
        << "  -v, --version               Show version info and exit.\n"
        << "  -h, --help                  Show this text and exit.\n"
        << "  --cpp_out=OUT_DIR           Generate C++ header and source.\n\n";
}

void process_file( const fs::path & input_file, std::span< const fs::path > import_paths,
                   const fs::path & output_dir )
{
    const auto parsed_file       = parse_proto_file( input_file, import_paths );
    const auto output_cpp_header = cpp_file_name_from_proto( input_file, ".pb.h" );
    const auto output_cpp        = cpp_file_name_from_proto( input_file, ".pb.cc" );

    auto cpp_header_stream = std::ofstream( output_dir / output_cpp_header );
    dump_cpp_header( parsed_file, cpp_header_stream );

    auto cpp_stream = std::ofstream( output_dir / output_cpp );
    dump_cpp( parsed_file, output_cpp_header, cpp_stream );
}

}// namespace

auto main( int argc, char * argv[] ) -> int
{
    if( argc < 2 )
    {
        print_usage( );
        return 1;
    }

    auto output_dir   = fs::path( );
    auto import_paths = std::vector< fs::path >{ fs::current_path( ) };

    for( ; argc > 1 && argv[ 1 ][ 0 ] == '-'; argc--, argv++ )
    {
        if( opt_help == argv[ 1 ] || opt_h == argv[ 1 ] )
        {
            print_usage( );
            return 0;
        }

        if( opt_version == argv[ 1 ] || opt_v == argv[ 1 ] )
        {
            std::cout << "spb-protoc version 0.1.0\n";
            return 0;
        }

        if( std::string_view( argv[ 1 ] ).starts_with( opt_proto_path ) )
        {
            import_paths.emplace_back( argv[ 1 ] + opt_proto_path.size( ) );
        }
        else if( std::string_view( argv[ 1 ] ).starts_with( opt_ipath ) )
        {
            import_paths.emplace_back( argv[ 1 ] + opt_ipath.size( ) );
        }
        else if( std::string_view( argv[ 1 ] ).starts_with( opt_cpp_out ) )
        {
            output_dir = argv[ 1 ] + opt_cpp_out.size( );
        }
        else
        {
            std::cerr << "Unknown option: " << argv[ 1 ] << ", use -h or --help\n";
            return 1;
        }
    }

    auto input_files = std::vector< fs::path >( );
    for( auto i = 1; i < argc; i++ )
    {
        input_files.emplace_back( argv[ i ] );
        import_paths.emplace_back( input_files.back( ).parent_path( ) );
    }

    if( output_dir.empty( ) )
    {
        std::cerr << "Missing output directory, use --cpp_out=OUT_DIR:\n";
        return 1;
    }

    if( input_files.empty( ) )
    {
        std::cerr << "Missing input files, use PROTO_FILES:\n";
        return 1;
    }

    try
    {
        for( const auto & input_file : input_files )
        {
            process_file( input_file, import_paths, output_dir );
        }
    }
    catch( const std::exception & e )
    {
        std::cerr << e.what( ) << '\n';
        return 1;
    }

    return 0;
}