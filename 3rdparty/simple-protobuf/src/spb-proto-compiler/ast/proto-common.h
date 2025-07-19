/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for common definitions                       *
* Link        : https://protobuf.dev/programming-guides/proto3/             *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-comment.h"
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using proto_reserved_range = std::vector< std::pair< int32_t, int32_t > >;
using proto_reserved_name  = std::unordered_set< std::string_view >;
using proto_options        = std::unordered_map< std::string_view, std::string_view >;

struct proto_reserved
{
    proto_reserved_range reserved_range;
    proto_reserved_name reserved_name;
};

/**
 * @brief base attributes for most proto types
 *
 */
struct proto_base
{
    //- points to .proto file
    std::string_view name;

    //- field number
    int32_t number;

    proto_options options;
    proto_comment comment;
};

using proto_bases = std::vector< proto_base >;
