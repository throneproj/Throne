/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for an enum                                  *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec/#enum_definition *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-common.h"
#include <vector>

struct proto_enum : public proto_base
{
    proto_bases fields;
    proto_reserved reserved;
};

using proto_enums = std::vector< proto_enum >;
