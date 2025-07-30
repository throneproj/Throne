// Copyright 2017 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a Apache
// license that can be found in the LICENSE file.

package plugin

import (
	"io/ioutil"
	"log"
	"os"
	"strings"

	"github.com/golang/protobuf/proto"
	"github.com/golang/protobuf/protoc-gen-go/generator"
)

func Main() {
	mainPlugin := new(mainPlugin)
	generator.RegisterPlugin(mainPlugin)

	// Begin by allocating a generator. The request and response structures are stored there
	// so we can do error handling easily - the response structure contains the field to
	// report failure.
	g := generator.New()
	if len(getAllCodeGenerator()) == 0 {
		g.Fail("no code generator plugin")
	}

	pkgReadRequetFromStdin(g)
	pkgGenerateAllFiles(g, mainPlugin)
	pkgWriteResponseToStdout(g)
}

func pkgGenerateAllFiles(g *generator.Generator, plugin *mainPlugin) {
	// set default plugins
	// protoc --xxx_out=. x.proto
	plugin.InitService(pkgGetUserPlugin(g))

	// parse command line parameters
	g.CommandLineParameters("plugins=" + plugin.Name())

	// Create a wrapped version of the Descriptors and EnumDescriptors that
	// point to the file that defines them.
	g.WrapTypes()

	g.SetPackageNames()
	g.BuildTypeNameMap()

	g.GenerateAllFiles()

	// skip non *.pb.xxx.go
	respFileList := g.Response.File[:0]
	for _, file := range g.Response.File {
		fileName := file.GetName()
		extName := plugin.FileNameExt()

		if strings.HasSuffix(fileName, extName) {
			respFileList = append(respFileList, file)
		}
	}
	g.Response.File = respFileList
}

func pkgReadRequetFromStdin(g *generator.Generator) {
	data, err := ioutil.ReadAll(os.Stdin)
	if err != nil {
		g.Error(err, "reading input")
	}

	if err := proto.Unmarshal(data, g.Request); err != nil {
		g.Error(err, "parsing input proto")
	}

	if len(g.Request.FileToGenerate) == 0 {
		g.Fail("no files to generate")
	}
}

func pkgWriteResponseToStdout(g *generator.Generator) {
	data, err := proto.Marshal(g.Response)
	if err != nil {
		g.Error(err, "failed to marshal output proto")
	}
	_, err = os.Stdout.Write(data)
	if err != nil {
		g.Error(err, "failed to write output proto")
	}
}

func pkgGetUserPlugin(g *generator.Generator) CodeGenerator {
	args := g.Request.GetParameter()
	userPluginName := pkgGetParameterValue(args, "plugin")
	if userPluginName == "" {
		userPluginName = getFirstServiceGeneratorName()
	}

	userPlugin := getCodeGenerator(userPluginName)
	if userPlugin == nil {
		log.Print("protoc-gen-plugin: registor plugins:", getAllServiceGeneratorNames())
		g.Fail("invalid plugin option:", userPluginName)
	}

	return userPlugin
}

func pkgGetParameterValue(parameter, key string) string {
	for _, p := range strings.Split(parameter, ",") {
		if i := strings.Index(p, "="); i > 0 {
			if p[0:i] == key {
				return p[i+1:]
			}
		}
	}
	return ""
}
