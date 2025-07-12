/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for map                                      *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec/#map_field *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "ast/proto-field.h"
#include "proto-common.h"

struct proto_map : public proto_base
{
    proto_field key;
    proto_field value;
};

using proto_maps = std::vector< proto_map >;