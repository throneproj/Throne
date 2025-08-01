// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "3rdparty/protorpc/rpc_conn.h"

#if (defined(_WIN32) || defined(_WIN64))
#	include "./rpc_conn_windows.cc"
#else
#	include "./rpc_conn_posix.cc"
#endif

namespace protorpc {

// MaxVarintLenN is the maximum length of a varint-encoded N-bit integer.
static const int maxVarintLen16 = 3;
static const int maxVarintLen32 = 5;
static const int maxVarintLen64 = 10;

// PutUvarint encodes a uint64 into buf and returns the number of bytes written.
// If the buffer is too small, PutUvarint will panic.
static int putUvarint(uint8_t buf[], uint64_t x) {
	auto i = 0;
	while(x >= 0x80) {
		buf[i] = uint8_t(x) | 0x80;
		x >>= 7;
		i++;
	}
	buf[i] = uint8_t(x);
	return i + 1;
}

// ReadUvarint reads an encoded unsigned integer from r and returns it as a uint64.
bool Conn::ReadUvarint(uint64_t* rx) {
	uint64_t x;
	uint8_t s, b;

	*rx = 0;
	x = 0;
	s = 0;

	for(int i = 0; ; i++) {
		if(!Read(&b, 1)) {
			return false;
		}
		if(b < 0x80) {
			if(i > 9 || i == 9 && b > 1){
				printf("protorpc.Conn.ReadUvarint: varint overflows a 64-bit integer\n");
				return false;
			}
			*rx = (x | uint64_t(b)<<s);
			return true;
		}
		x |= (uint64_t(b&0x7f) << s);
		s += 7;
	}
	printf("not reachable!\n");
	return false;
}

// PutUvarint encodes a uint64 into buf and returns the number of bytes written.
// If the buffer is too small, PutUvarint will panic.
bool Conn::WriteUvarint(uint64_t x) {
	uint8_t buf[maxVarintLen64];
	int n = putUvarint(buf, x);
	return Write(buf, n);
}

bool Conn::RecvFrame(::std::string* data) {
	uint64_t size;
	if(!ReadUvarint(&size)) {
		return false;
	}
	if(size != 0) {
		data->resize(size_t(size));
		if(!Read((void*)data->data(), int(size))) {
			data->clear();
			return false;
		}
	}
	return true;
}

bool Conn::SendFrame(const ::std::string* data) {
	if(data == NULL) {
		return WriteUvarint(uint64_t(0));
	}

	if(!WriteUvarint(uint64_t(data->size()))) {
		return false;
	}
	if(!Write((void*)data->data(), data->size())) {
		return false;
	}
	return true;
}

} // namespace protorpc
