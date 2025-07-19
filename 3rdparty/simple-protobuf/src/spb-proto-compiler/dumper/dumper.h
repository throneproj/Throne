/***************************************************************************\
* Name        : CPP dumper                                                  *
* Description : generate C++ src files for de/serialization                 *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "ast/proto-file.h"
#include <filesystem>

/**
 * @brief dump C++ header file for parsed proto
 *
 * @param file parsed proto
 * @param header_file output file
 * @return C++ header file for a ast
 */
void dump_cpp_header( const proto_file & file, std::ostream & header_file );

/**
 * @brief dump C++ file for parsed proto
 *
 * @param file parsed proto
 * @param header_file generated C++ header file (ex: my.pb.h)
 * @param file_stream output file name (ex: my.pb.cpp)
 */
void dump_cpp( const proto_file & file, const std::filesystem::path & header_file,
               std::ostream & file_stream );
