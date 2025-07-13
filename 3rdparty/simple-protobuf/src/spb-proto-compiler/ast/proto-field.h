/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for a field                                  *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec/#normal_field *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-common.h"
#include <string_view>
#include <vector>
#include <spb/pb/wire-types.h>

struct proto_field : public proto_base
{
    enum Type
    {
        NONE,
        BOOL,
        BYTES,
        DOUBLE,
        ENUM,
        FLOAT,
        FIXED32,
        FIXED64,
        INT32,
        INT64,
        MESSAGE,
        SFIXED32,
        SFIXED64,
        SINT32,
        SINT64,
        STRING,
        UINT32,
        UINT64,
    };

    enum class BitType
    {
        NONE,
        INT8,
        INT16,
        INT32,
        INT64,
        UINT8,
        UINT16,
        UINT32,
        UINT64,
    };

    enum class Label
    {
        //- no type modifier, only `type`
        NONE = 0,
        //- std::optional< type >
        OPTIONAL = 1,
        //- std::vector< type >
        REPEATED = 2,
        //- std::unique_ptr< type >, used to break up circular references
        PTR = 3,
    };

    Type type   = Type::NONE;
    Label label = Label::OPTIONAL;

    //- points to .proto file
    std::string_view type_name;

    BitType bit_type = BitType::NONE;
    std::string_view bit_field;
};

using proto_fields = std::vector< proto_field >;
