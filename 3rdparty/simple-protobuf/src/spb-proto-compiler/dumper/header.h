/***************************************************************************\
* Name        : C++ header dumper                                           *
* Description : generate C++ header with all structs and enums              *
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
 *        dump all structs and enums
 *
 * @param file parsed proto
 * @param stream output stream
 */
void dump_cpp_definitions( const proto_file & file, std::ostream & stream );

/**
 * Replaces all occurrences of a substring in a given string with another substring.
 *
 * @param input the original string
 * @param what the substring to be replaced
 * @param with the substring to replace 'what' with
 *
 * @return the modified string after replacements
 */
auto replace( std::string_view input, std::string_view what, std::string_view with ) -> std::string;

/**
 * @brief throw parse error 'at' offset
 *
 * @param file parsed file
 * @param at error offset
 * @param message message to the user
 */
[[noreturn]] void throw_parse_error( const proto_file & file, std::string_view at,
                                     std::string_view message );
