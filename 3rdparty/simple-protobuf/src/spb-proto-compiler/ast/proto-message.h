/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for message                                  *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec/#message_definition *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-common.h"
#include "proto-enum.h"
#include "proto-field.h"
#include "proto-map.h"
#include "proto-oneof.h"
#include <set>
#include <string_view>
#include <vector>

struct proto_message;
using proto_messages         = std::vector< proto_message >;
using forwarded_declarations = std::set< std::string_view >;

struct proto_message : public proto_base
{
    proto_fields fields;
    proto_reserved_range extensions;
    proto_messages messages;
    proto_maps maps;
    proto_oneofs oneofs;
    proto_enums enums;
    proto_reserved reserved;

    //- this will be used for type dependency checking
    //- its an "resolved order" of the message in the .proto file
    //- so we can later call std::sort on `.messages`
    size_t resolved = 0;
    //- forwarded declarations needed for type dependency checking
    forwarded_declarations forwards;
};
