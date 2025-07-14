/***************************************************************************\
* Name        : ast render                                                  *
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

/**
 * @brief return scalar type or no type
 *        https://protobuf.dev/programming-guides/proto3/#scalar
 *
 * @param type_name
 * @return scalar type or no type
 */
[[nodiscard]] auto get_encoder( const proto_field & field ) -> proto_field::Type;

/**
 * @brief resolve types and sort all messages in a proto file
 *        so if message A depends on message B
 *        then message B must be defined before message A
 *
 */
void resolve_messages( proto_file & file );
