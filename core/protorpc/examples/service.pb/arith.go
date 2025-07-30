// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package service

import (
	"errors"
)

type Arith int

func (t *Arith) Add(args *ArithRequest, reply *ArithResponse) error {
	reply.C = args.A + args.B
	return nil
}

func (t *Arith) Mul(args *ArithRequest, reply *ArithResponse) error {
	reply.C = args.A * args.B
	return nil
}

func (t *Arith) Div(args *ArithRequest, reply *ArithResponse) error {
	if args.B == 0 {
		return errors.New("divide by zero")
	}
	reply.C = args.A / args.B
	return nil
}

func (t *Arith) Error(args *ArithRequest, reply *ArithResponse) error {
	return errors.New("ArithError")
}
