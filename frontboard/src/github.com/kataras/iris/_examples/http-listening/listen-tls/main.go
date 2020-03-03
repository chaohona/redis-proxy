package main

import (
	"net/url"

	"github.com/kataras/iris/v12"

	"github.com/kataras/iris/v12/core/host"
)

func main() {
	app := iris.New()

	app.Get("/", func(ctx iris.Context) {
		ctx.Writef("Hello from the SECURE server")
	})

	app.Get("/mypath", func(ctx iris.Context) {
		ctx.Writef("Hello from the SECURE server on path /mypath")
	})

	// to start a new server listening at :80 and redirects
	// to the secure address, then:
	target, _ := url.Parse("https://127.0.0.1:443")
	go host.NewRedirection("127.0.0.1:80", target, iris.StatusMovedPermanently).ListenAndServe()

	// start the server (HTTPS) on port 443, this is a blocking func
	app.Run(iris.TLS("127.0.0.1:443", "mycert.cert", "mykey.key"))
}
