// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#ifndef PROTORPC_CLIENT_H__
#define PROTORPC_CLIENT_H__

#include "3rdparty/protorpc/rpc_conn.h"
#include "3rdparty/protorpc/rpc_error.h"

#include <stdint.h>
#include <string>

namespace protorpc {

class Client {
public:
	Client(const char* host, int port);
	~Client();

	const ::protorpc::Error CallMethod(
		const std::string& method,
		const ::std::string* request,
		::std::string* response
	);

	// Close the connection
	void Close();

private:
	const ::protorpc::Error callMethod(
		const std::string& method,
		const ::std::string* request,
		::std::string* response
	);

	bool checkMothdValid(
		const std::string& method,
		const ::std::string* request,
		::std::string* response
	) const;

	std::string host_;
	int port_;
	Conn conn_;
	uint64_t seq_;
};

} // namespace protorpc

#endif // PROTORPC_CLIENT_H__
