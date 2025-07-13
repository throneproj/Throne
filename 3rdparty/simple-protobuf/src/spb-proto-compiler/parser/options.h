/***************************************************************************\
* Name        : options                                                     *
* Description : option names used by parser/dumper                          *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <string_view>

using namespace std::literals;

const auto option_optional_type    = "optional.type"sv;
const auto option_optional_include = "optional.include"sv;

const auto option_repeated_type    = "repeated.type"sv;
const auto option_repeated_include = "repeated.include"sv;

const auto option_pointer_type    = "pointer.type"sv;
const auto option_pointer_include = "pointer.include"sv;

const auto option_bytes_type    = "bytes.type"sv;
const auto option_bytes_include = "bytes.include"sv;

const auto option_string_type    = "string.type"sv;
const auto option_string_include = "string.include"sv;

const auto option_field_type = "field.type"sv;
const auto option_enum_type  = "enum.type"sv;
