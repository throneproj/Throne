// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#ifndef PROTORPC_ERROR_H__
#define PROTORPC_ERROR_H__

namespace protorpc {

// Error
class Error {
public:
	Error(){}
	Error(const std::string& err): err_text_(err) {}
	Error(const Error& err): err_text_(err.err_text_) {}
	~Error(){}

	static Error Nil() { return Error(); }
	static Error New(const std::string& err) { return Error(err); }

	bool IsNil()const { return err_text_.empty(); }
	const std::string& String()const { return err_text_; }

private:
	std::string err_text_;
};

} // namespace protorpc

#endif // PROTORPC_ERROR_H__
