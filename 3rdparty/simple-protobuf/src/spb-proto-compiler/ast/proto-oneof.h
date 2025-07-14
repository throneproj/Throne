/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for oneof syntax                             *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec/#oneof_and_oneof_field *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-common.h"
#include "proto-field.h"
#include <vector>

struct proto_oneof : public proto_base
{
    proto_fields fields;
};

using proto_oneofs = std::vector< proto_oneof >;
