// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "3rdparty/protorpc/rpc_conn.h"

#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__APPLE__) && !defined(MSG_NOSIGNAL)
#	define MSG_NOSIGNAL 0x4000
#endif

#ifndef NI_MAXSERV
# define NI_MAXSERV 32
#endif

namespace protorpc {

// [static]
// Initialize socket services
bool InitSocket() {
	return true;
}

bool Conn::IsValid() const {
	return sock_ != 0;
}

bool Conn::DialTCP(const char* host, int port) {
	struct sockaddr_in sa;
	int status, len;

	if(IsValid()) Close();
	if((sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("protorpc.Conn.DialTCP: socket failed.\n");
		sock_ = 0;
		return false;
	}

	memset(sa.sin_zero, 0 , sizeof(sa.sin_zero));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(host);
	size_t addressSize = sizeof(sa);

	if(connect(sock_, (struct sockaddr*)&sa, addressSize) == -1) {
		printf("protorpc.Conn.DialTCP: connect failed.\n");
		Close();
		return false;
	}

	int flag = 1;
	setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	return true;
}

bool Conn::ListenTCP(int port, int backlog) {
	if(IsValid()) Close();
	if((sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("protorpc.Conn.ListenTCP: socket failed.\n");
		sock_ = 0;
		return false;
	}

	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons((u_short) port);

	if(bind(sock_, (struct sockaddr*)&saddr, sizeof(saddr)) == -1) {
		printf("protorpc.Conn.ListenTCP: bind failed.\n");
		Close();
		return false;
	}
	if(::listen(sock_, backlog) != 0) {
		printf("protorpc.Conn.ListenTCP: listen failed.\n");
		Close();
		return false;
	}

	int flag = 1;
	setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));
	return true;
}

void Conn::Close() {
	if(IsValid()) {
		::close(sock_);
		sock_ = 0;
	}
}

Conn* Conn::Accept() {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int sock = ::accept(sock_, (struct sockaddr*)&addr, &addrlen);
	if(sock == 0) {
		printf("protorpc.Conn.Accept: failed.\n");
		return NULL;
	}
	return new Conn(sock);
}

bool Conn::Read (void* buf, int len) {
	char *cbuf = (char*)buf;
	while(len > 0) {
		int sent = recv(sock_, cbuf, len, 0);
		if(sent == 0 || sent == -1) {
			printf("protorpc.Conn.Read: IO error, err = %d.\n", errno);
			return false;
		}
		cbuf += sent;
		len -= sent;
	}
	return true;
}
bool Conn::Write(void* buf, int len) {
	const char *cbuf = (char*)buf;
	int flags = MSG_NOSIGNAL;

	while(len > 0) {
	int sent = send(sock_, cbuf, len, flags);
		if(sent == -1) {
			printf("protorpc.Conn.Write: IO error, err = %d.\n", errno);
			return false;
		}
		cbuf += sent;
		len -= sent;
	}
	return true;
}

} // namespace protorpc
