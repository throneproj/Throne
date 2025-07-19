/***************************************************************************\
* Name        : generic reader and writer                                   *
* Description : user specific input/output used for de/serialization        *
* Author      : antonin.kriz@gmail.com                                      *
* ------------------------------------------------------------------------- *
* This is free software; you can redistribute it and/or modify it under the *
* terms of the MIT license. A copy of the license can be found in the file  *
* "LICENSE" at the root of this distribution.                               *
\***************************************************************************/

#pragma once
#include "function_ref.hpp"
#include <cstdlib>

namespace spb::io
{
/**
 * @brief generic writer used to write exactly `size` number of bytes from `p_data`
 *
 * @param[in] p_data input buffer
 * @param[in] size input buffer size
 * @throws any exception thrown will stop the `serialize` process and will be propagated to the
 *         caller of `spb::pb::serialize` or `spb::json::serialize`
 */
using writer = spb::detail::function_ref< void( const void * p_data, size_t size ) >;

//-
/**
 * @brief generic reader used to read up to `size` number of bytes into `p_data`
 *
 * @param p_data output buffer (this is never nullptr)
 * @param[in] size number of bytes to read (this is always > 0)
 * @return number of bytes copied into `p_data`, could be less than `size`. 0 indicates end-of-file
 * @throws any exception thrown will stop the `deserialize` process and will be propagated to the
 *         caller of `spb::pb::deserialize` or `spb::json::deserialize`
 */
using reader = spb::detail::function_ref< size_t( void * p_data, size_t size ) >;

}// namespace spb::io
