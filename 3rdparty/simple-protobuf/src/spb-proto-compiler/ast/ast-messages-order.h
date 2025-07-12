/***************************************************************************\
* Name        : ast types resolver                                          *
* Description : resolve types in ast tree                                   *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once

#include "proto-file.h"

void resolve_messages_order( proto_file & file );
