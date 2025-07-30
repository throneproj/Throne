// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#ifndef PROTORPC_CONN_H__
#define PROTORPC_CONN_H__

#include <stdarg.h>
#include <stdint.h>
#include <string>

namespace protorpc {

// Initialize socket services
bool InitSocket();

// Stream-oriented network connection.
class Conn {
public:
	Conn(int fd=0): sock_(fd) { InitSocket(); }
	~Conn() {}

	bool IsValid() const;
	bool DialTCP(const char* host, int port);
	bool ListenTCP(int port, int backlog=5);
	void Close();

	Conn* Accept();

	bool Read(void* buf, int len);
	bool Write(void* buf, int len);

	bool ReadUvarint(uint64_t* x);
	bool WriteUvarint(uint64_t x);

	bool RecvFrame(::std::string* data);
	bool SendFrame(const ::std::string* data);

private:

	int sock_;
};

}  // namespace protorpc

#endif // PROTORPC_CONN_H__

