// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "3rdparty/protorpc/rpc_wire.h"

namespace protorpc {
namespace wire {

Error SendRequest(Conn* conn,
	uint64_t id, const std::string& serviceMethod,
	const ::std::string* request
) {
	// marshal request
	std::string pbRequest;
	if(request != NULL) {
		pbRequest = *request;
	}

	// generate header
	RequestHeader header;

	header.id = id;
	header.method = serviceMethod;

	header.raw_request_len = pbRequest.size();

	// check header size
	std::string pbHeader = spb::pb::serialize<std::string>(header);
	if(pbHeader.size() > (unsigned int)protorpc::wire::Const::MAX_REQUEST_HEADER_LEN) {
		return Error::New("protorpc.SendRequest: header larger than max_header_len.");
	}

	// send header
	if(!conn->SendFrame(&pbHeader)) {
		return Error::New("protorpc.SendRequest: SendFrame header failed.");
	}

	// send body
	if(!conn->SendFrame(&pbRequest)) {
		return Error::New("protorpc.SendRequest: SendFrame body failed.");
	}

	return Error::Nil();
}

Error RecvRequestHeader(Conn* conn,
	RequestHeader* header
) {
	// recv header
	std::string pbHeader;
	if(!conn->RecvFrame(&pbHeader)) {
		return Error::New("protorpc.RecvRequestHeader: RecvFrame failed.");
	}
	if(pbHeader.size() > (unsigned int)protorpc::wire::Const::MAX_REQUEST_HEADER_LEN) {
		return Error::New("protorpc.RecvRequestHeader: RecvFrame larger than max_header_len.");
	}

	// Marshal Header
	*header = spb::pb::deserialize<RequestHeader>(pbHeader);

	return Error::Nil();
}

Error RecvRequestBody(Conn* conn,
	const RequestHeader* header,
	::std::string* request
) {
	// recv body
	std::string pbRequest;
	if(!conn->RecvFrame(&pbRequest)) {
		return Error::New("protorpc.RecvRequestBody: RecvFrame failed.");
	}

	// check wire header: rawMsgLen
	if(pbRequest.size() != header->raw_request_len) {
		return Error::New("protorpc.RecvRequestBody: Unexcpeted raw msg len.");
	}

	// marshal request
	*request = pbRequest;

	return Error::Nil();
}

Error SendResponse(Conn* conn,
	uint64_t id, const std::string& error,
	const ::std::string* response
) {
	// marshal response
	std::string pbResponse;
	if(response != NULL) {
		pbResponse = *response;
	}

	// generate header
	ResponseHeader header;

	header.id = id;
	header.error = error;

	header.raw_response_len = pbResponse.size();

	// check header size
	std::string pbHeader = spb::pb::serialize<std::string>(header);


	// send header
	if(!conn->SendFrame(&pbHeader)) {
		return Error::New("protorpc.SendResponse: SendFrame header failed.");
	}

	// send body
	if(!conn->SendFrame(&pbResponse)) {
		return Error::New("protorpc.SendResponse: SendFrame body failed.");
	}

	return Error::Nil();
}

Error RecvResponseHeader(Conn* conn,
	ResponseHeader* header
) {
	// recv header
	std::string pbHeader;
	if(!conn->RecvFrame(&pbHeader)) {
		return Error::New("protorpc.RecvResponseHeader: RecvFrame failed.");
	}

	// Marshal Header
	*header = spb::pb::deserialize<ResponseHeader>(pbHeader);

	return Error::Nil();
}

Error RecvResponseBody(Conn* conn,
	const ResponseHeader* header,
	::std::string* response
) {
	// recv body
	std::string pbResponse;
	if(!conn->RecvFrame(&pbResponse)) {
		return Error::New("protorpc.RecvResponseBody: RecvFrame failed.");
	}

	// check wire header: rawMsgLen
	if(pbResponse.size() != header->raw_response_len) {
		return Error::New("protorpc.RecvResponseBody: Unexcpeted raw msg len.");
	}

	// marshal response
	*response = pbResponse;

	return Error::Nil();
}

} // namespace wire
} // namespace protorpc
