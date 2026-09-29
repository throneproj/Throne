// Run through script/gen_schema.sh so the build tags match the shipped core; the GUI bundles the output as res/schema/sing-box.json.
package main

import (
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"log"
	"os"
	"reflect"

	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/schema"
)

func main() {
	output := flag.String("o", "", "write to this file instead of stdout")
	indent := flag.Bool("indent", false, "keep the generated indentation")
	flag.Parse()

	content, err := schema.Generate(include.Context(context.Background()), reflect.TypeFor[option.Options]())
	if err != nil {
		log.Fatalln("generate schema:", err)
	}

	if !*indent {
		var compact bytes.Buffer
		if err = json.Compact(&compact, content); err != nil {
			log.Fatalln("compact schema:", err)
		}
		content = compact.Bytes()
	}

	if *output == "" {
		if _, err = os.Stdout.Write(content); err != nil {
			log.Fatalln("write schema:", err)
		}
		return
	}
	if err = os.WriteFile(*output, content, 0o644); err != nil {
		log.Fatalln("write schema:", err)
	}
}
