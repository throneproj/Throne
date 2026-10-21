/***************************************************************************\
* Name        : serialize library for protobuf                              *
* Description : field attributes                                            *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <cstddef>
#include <stdexcept>

namespace spb::json::detail
{
struct field_attributes
{
    size_t max_count = 0;
    size_t max_size  = 0;
};

inline void check_size(size_t size, size_t max_size)
{
    if (size > max_size) [[unlikely]]
        throw std::length_error("field is too large");
}

} // namespace spb::json::detail
