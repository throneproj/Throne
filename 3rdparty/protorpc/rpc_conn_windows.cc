// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "3rdparty/protorpc/rpc_conn.h"

#include <string.h>

#ifdef _MSC_VER
#	include <ws2tcpip.h> /* send,recv,socklen_t etc */
#	include <wspiapi.h>  /* addrinfo */
#	pragma comment(lib, "ws2_32.lib")
#else
#	include <ws2tcpip.h> /* send,recv,socklen_t etc */
#	include <winsock2.h>
typedef int socklen_t;
#endif

#ifndef NI_MAXSERV
# define NI_MAXSERV 32
#endif

namespace protorpc {

// [static]
// Initialize socket services
bool InitSocket() {
	WSADATA wsaData;
	WORD wVers;
	static bool called_once = false;
	static bool retval = false;

	if(called_once) return retval;

	called_once = true;
	wVers = MAKEWORD(1, 1);
	retval = (WSAStartup(wVers, &wsaData) == 0);

	return retval;
}

bool Conn::IsValid() const {
	return sock_ != 0;
}

bool Conn::DialTCP(const char* host, int port) {
	if(IsValid()) Close();
	if((sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("protorpc.Conn.DialTCP: socket failed.\n");
		sock_ = 0;
		return false;
	}

	struct sockaddr_in sa;
	socklen_t addressSize;
	memset(sa.sin_zero, 0 , sizeof(sa.sin_zero));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(host);
	addressSize = sizeof(sa);

	if(connect(sock_, ( struct sockaddr*)&sa, addressSize) == -1 ) {
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
		::closesocket(sock_);
		sock_ = 0;
	}
}

Conn* Conn::Accept() {
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
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
			printf("protorpc.Conn.Read: IO error, err = %d.\n", WSAGetLastError());
			return false;
		}
		cbuf += sent;
		len -= sent;
	}
	return true;
}
bool Conn::Write(void* buf, int len) {
	const char *cbuf = (char*)buf;
	int flags = 0;

	while(len > 0) {
		int sent = send(sock_, cbuf, len, flags );
		if(sent == -1) {
			printf("protorpc.Conn.Write: IO error, err = %d.\n", WSAGetLastError());
			return false;
		}
		cbuf += sent;
		len -= sent;
	}
	return true;
}

} // namespace protorpc
