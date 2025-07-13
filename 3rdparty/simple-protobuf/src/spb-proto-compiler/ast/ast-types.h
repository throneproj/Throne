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

#include "ast/proto-field.h"
#include "proto-file.h"

[[nodiscard]] auto is_scalar( const proto_field::Type & type ) -> bool;
[[nodiscard]] auto is_packed_array( const proto_file & file, const proto_field & field ) -> bool;

/**
 * @brief resolve types in a proto file
 *
 */
void resolve_types( proto_file & file );
