/***************************************************************************\
* Name        : proto option                                                *
* Description : data structure for an single option                         *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <vector>

struct proto_option;
using proto_option_name  = std::string_view;
using proto_option_value = std::string_view;

using proto_options = std::map<proto_option_name, proto_option>;

struct proto_option
{
    std::vector<proto_option_value> value;
    std::vector<proto_options> options;
};

struct proto_attributes
{
    // default field value
    std::string_view default_;

    // maximum size for `string` or `bytes`
    // de/serialize will return false if `real_size > max_size`
    std::optional<uint32_t> max_size;

    // maximum elements in an repeated field
    // de/serialize will return false if `real_count > max_count`
    std::optional<uint32_t> max_count;

    // field's type: one of int8, uint8, int16, uint16, int32, uint32, int64, uint64
    // can be combined with an bit field: `int8:1`, `uint8:2`, ...
    // warning: due to compatibility with GPB, always use int types with the same sign, like `int32`
    // and `int8`, combinations like `int32` and `uint8` or `float and `int32` are invalid.
    std::string_view type;

    // enum type, one of int8, uint8, int16, uint16, int32, uint32, int64, uint64
    // default: "int32"
    std::string_view enum_;

    // extra files to include in generated `.pb.h`
    std::set<std::string_view> include;

    // files to exclude from includes in generated `.pb.h`
    std::set<std::string_view> exclude;

    // container type for optional label
    // default: "std::optional<$>"
    std::string_view optional;

    // container type for optional label if there is cyclic dependency between messages (A -> B, B
    // -> A) default: "std::unique_ptr<$>"
    std::string_view pointer;

    // container type for repeated label
    // default: "std::vector<$>", `$` will be replaced by a field's type
    std::string_view repeated;

    // container type for bytes type
    // default: "std::vector<$>", `$` will be replaced with `std::byte`
    std::string_view bytes;

    // container type for string type
    // default: "std::string"
    std::string_view string;

    // container type for map
    // default: "std::map<$, @>", `$` will be replaced by a map's key and `@` by a map's value
    std::string_view map;

    // field name used for json de/serializer
    // default: <none>
    std::string_view json_name;

    // [[deprecated]] attribute
    bool deprecated = false;

    // packed attribute for an array or message
    std::optional<bool> packed;
};
