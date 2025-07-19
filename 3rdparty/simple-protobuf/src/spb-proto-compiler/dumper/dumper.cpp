/***************************************************************************\
* Name        : CPP dumper                                                  *
* Description : generate C++ src files for de/serialization                 *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "dumper.h"
#include "header.h"
#include "pb/dumper.h"
#include "json/dumper.h"

void dump_cpp_header( const proto_file & file, std::ostream & stream )
{
    try
    {
        dump_cpp_definitions( file, stream );
        dump_json_header( file, stream );
        dump_pb_header( file, stream );
    }
    catch( const std::exception & e )
    {
        throw std::runtime_error( file.path.string( ) + ":" + e.what( ) );
    }
}

void dump_cpp( const proto_file & file, const std::filesystem::path & header_file,
               std::ostream & file_stream )
{
    try
    {
        dump_json_cpp( file, header_file, file_stream );
        dump_pb_cpp( file, header_file, file_stream );
    }
    catch( const std::exception & e )
    {
        throw std::runtime_error( file.path.string( ) + ":" + e.what( ) );
    }
}
