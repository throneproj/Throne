/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for the whole proto file                     *
* Link        : https://protobuf.dev/reference/protobuf/proto3-spec         *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-comment.h"
#include "proto-common.h"
#include "proto-extend.h"
#include "proto-message.h"
#include "proto-option.h"
#include "proto-service.h"
#include "proto-syntax.h"
#include <filesystem>
#include <string>
#include <vector>

struct proto_file;

using proto_files = std::vector<proto_file>;

struct proto_file
{
    std::filesystem::path path;
    std::string content;
    proto_syntax syntax;
    proto_comment comment;
    proto_files imports;
    proto_message package;
    proto_extends extends;
    proto_options options;
    proto_attributes attributes;

    proto_services services;
};
