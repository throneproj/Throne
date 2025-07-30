// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package service

type Echo int

func (t *Echo) Echo(args *EchoRequest, reply *EchoResponse) error {
	reply.Msg = args.Msg
	return nil
}

func (t *Echo) EchoTwice(args *EchoRequest, reply *EchoResponse) error {
	reply.Msg = args.Msg + args.Msg
	return nil
}
