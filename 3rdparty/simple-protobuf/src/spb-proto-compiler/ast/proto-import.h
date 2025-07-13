/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for imports                                  *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec/#import_statement *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-comment.h"
#include <string_view>

struct proto_import
{
    //- points to .proto file
    std::string_view file_name;

    proto_comment comments;
};

using proto_imports = std::vector< proto_import >;