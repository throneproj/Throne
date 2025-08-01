// Copyright 2017 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a Apache
// license that can be found in the LICENSE file.

package plugin

import (
	"github.com/golang/protobuf/protoc-gen-go/descriptor"
	"github.com/golang/protobuf/protoc-gen-go/generator"
)

var pkgCodeGeneratorList []CodeGenerator

type CodeGenerator interface {
	Name() string
	FileNameExt() string

	HeaderCode(g *generator.Generator, file *generator.FileDescriptor) string
	ServiceCode(g *generator.Generator, file *generator.FileDescriptor, svc *descriptor.ServiceDescriptorProto) string
	MessageCode(g *generator.Generator, file *generator.FileDescriptor, msg *descriptor.DescriptorProto) string
}

func RegisterCodeGenerator(g CodeGenerator) {
	pkgCodeGeneratorList = append(pkgCodeGeneratorList, g)
}

func getAllCodeGenerator() []CodeGenerator {
	return pkgCodeGeneratorList
}

func getAllServiceGeneratorNames() (names []string) {
	for _, g := range pkgCodeGeneratorList {
		names = append(names, g.Name())
	}
	return
}

func getFirstServiceGeneratorName() string {
	if len(pkgCodeGeneratorList) > 0 {
		return pkgCodeGeneratorList[0].Name()
	}
	return ""
}

func getCodeGenerator(name string) CodeGenerator {
	for _, g := range pkgCodeGeneratorList {
		if g.Name() == name {
			return g
		}
	}
	return nil
}
