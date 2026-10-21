/***************************************************************************\
* Name        : proto ast                                                   *
* Description : data structure for an extend                                *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-field.h"
#include <unordered_map>

using proto_extends = std::unordered_map<std::string_view, proto_fields>;
