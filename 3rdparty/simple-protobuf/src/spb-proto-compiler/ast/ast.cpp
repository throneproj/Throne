/***************************************************************************\
* Name        : ast render                                                  *
* Description : resolve types in ast tree                                   *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#include "ast.h"
#include "ast-types.h"
#include "ast-messages-order.h"

void resolve_messages( proto_file & file )
{
    resolve_types( file );
    resolve_messages_order( file );
}
