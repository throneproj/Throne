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
#include "proto-option.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using proto_reserved_range = std::vector<std::pair<int32_t, int32_t>>;
using proto_reserved_name  = std::unordered_set<std::string_view>;

struct proto_reserved
{
    proto_reserved_range reserved_range;
    proto_reserved_name reserved_name;
};

struct cpp_ident
{
    //- points to .proto file
    std::string_view proto_name{};

    //- cpp compatible name in case that `proto_name` can't be used in cpp directly
    //- ('private' -> 'private_')
    std::string cpp_name{};

    auto get_name() const -> std::string_view
    {
        return cpp_name.empty() ? proto_name : std::string_view(cpp_name);
    }
};

/**
 * @brief base attributes for most proto types
 *
 */
struct proto_base
{
    cpp_ident name;

    //- field number
    int32_t number;

    proto_options options;
    proto_comment comment;
    proto_attributes attributes;
};

using proto_bases = std::vector<proto_base>;
