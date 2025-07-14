/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for an comment                               *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include <string_view>
#include <vector>

struct proto_comment
{
    //- points to .proto file
    std::vector< std::string_view > comments;
};
