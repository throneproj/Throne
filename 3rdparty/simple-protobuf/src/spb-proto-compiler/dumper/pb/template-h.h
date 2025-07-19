/***************************************************************************\
* Name        : pb dumper                                                   *
* Description : C++ template used for pb de/serialization                   *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <string_view>

constexpr std::string_view file_pb_header_template = R"(
void serialize( ostream & stream, const $ & value );
void deserialize_value( istream & stream, $ & value, uint32_t tag );

)";
