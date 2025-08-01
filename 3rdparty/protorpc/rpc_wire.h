// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#ifndef PROTORPC_WIRE_H__
#define PROTORPC_WIRE_H__

#include <stddef.h>
#include <stdint.h>

#include "3rdparty/protorpc/wire.pb/wire.pb.h"
#include "3rdparty/protorpc/rpc_error.h"
#include "3rdparty/protorpc/rpc_conn.h"

namespace protorpc {
namespace wire {

Error SendRequest(Conn* conn,
	uint64_t id, const std::string& serviceMethod,
	const ::std::string* request
);
Error RecvRequestHeader(Conn* conn,
	RequestHeader* header
);
Error RecvRequestBody(Conn* conn,
	const RequestHeader* header,
	::std::string* request
);

Error SendResponse(Conn* conn,
	uint64_t id, const std::string& error,
	const ::std::string* response
);
Error RecvResponseHeader(Conn* conn,
	ResponseHeader* header
);
Error RecvResponseBody(Conn* conn,
	const ResponseHeader* header,
	::std::string* request
);

} // namespace wire
} // namespace protorpc

#endif // PROTORPC_WIRE_H__

