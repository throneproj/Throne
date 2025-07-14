/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for proto syntax                             *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec/#syntax *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-comment.h"
#include <cstdint>

struct proto_syntax
{
    uint32_t version = 2;
    proto_comment comments;
};
