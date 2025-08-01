#pragma once

#include <cstddef>
#include <cstdint>
#include <spb/json.hpp>
#include <spb/pb.hpp>
#include <string>

// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
//	protorpc wire format wrapper
//
//	0. Frame Format
//	len : uvarint64
//	data: byte[len]
//
//	1. Client Send Request
//	Send RequestHeader: sendFrame(zsock, hdr, len(hdr))
//	Send Request: sendFrame(zsock, body, hdr.snappy_compressed_request_len)
//
//	2. Server Recv Request
//	Recv RequestHeader: recvFrame(zsock, hdr, max_hdr_len, 0)
//	Recv Request: recvFrame(zsock, body, hdr.snappy_compressed_request_len, 0)
//
//	3. Server Send Response
//	Send ResponseHeader: sendFrame(zsock, hdr, len(hdr))
//	Send Response: sendFrame(zsock, body, hdr.snappy_compressed_response_len)
//
//	4. Client Recv Response
//	Recv ResponseHeader: recvFrame(zsock, hdr, max_hdr_len, 0)
//	Recv Response: recvFrame(zsock, body, hdr.snappy_compressed_response_len, 0)
//
namespace protorpc::wire
{
enum class Const : int32_t
{
ZERO = 0,
MAX_REQUEST_HEADER_LEN = 1024,
};
struct RequestHeader
{
uint64_t id;
std::string method;
uint32_t raw_request_len;
};
struct ResponseHeader
{
uint64_t id;
std::string error;
uint32_t raw_response_len;
};
}// namespace protorpc::wire

namespace spb::json::detail
{
struct ostream;
struct istream;

void serialize_value( ostream & stream, const protorpc::wire::RequestHeader & value );
void deserialize_value( istream & stream, protorpc::wire::RequestHeader & value );


void serialize_value( ostream & stream, const protorpc::wire::ResponseHeader & value );
void deserialize_value( istream & stream, protorpc::wire::ResponseHeader & value );


void serialize_value( ostream & stream, const protorpc::wire::Const & value );
void deserialize_value( istream & stream, protorpc::wire::Const & value );

} // namespace spb::json::detail
namespace spb::pb::detail
{
struct ostream;
struct istream;

void serialize( ostream & stream, const protorpc::wire::RequestHeader & value );
void deserialize_value( istream & stream, protorpc::wire::RequestHeader & value, uint32_t tag );


void serialize( ostream & stream, const protorpc::wire::ResponseHeader & value );
void deserialize_value( istream & stream, protorpc::wire::ResponseHeader & value, uint32_t tag );

} // namespace spb::pb::detail
