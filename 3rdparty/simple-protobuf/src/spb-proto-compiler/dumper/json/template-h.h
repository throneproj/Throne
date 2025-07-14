/***************************************************************************\
* Name        : json dumper                                                 *
* Description : C++ template used for json de/serialization                 *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <string_view>

constexpr std::string_view file_json_header_template = R"(
void serialize_value( ostream & stream, const $ & value );
void deserialize_value( istream & stream, $ & value );

)";
