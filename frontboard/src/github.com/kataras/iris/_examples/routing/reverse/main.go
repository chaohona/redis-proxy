package main

import (
	"github.com/kataras/iris/v12"
	"github.com/kataras/iris/v12/core/router"
)

func main() {
	app := iris.New()
	// need for manually reverse routing when needed outside of view engine.
	// you normally don't need it because of the {{ urlpath "routename" "path" "values" "here"}}
	rv := router.NewRoutePathReverser(app)

	myroute := app.Get("/anything/{anythingparameter:path}", func(ctx iris.Context) {
		paramValue := ctx.Params().Get("anythingparameter")
		ctx.Writef("The path after /anything is: %s", paramValue)
	})

	myroute.Name = "myroute"

	// useful for links, although iris' view engine has the {{ urlpath "routename" "path values"}} already.
	app.Get("/reverse_myroute", func(ctx iris.Context) {
		myrouteRequestPath := rv.Path(myroute.Name, "any/path")
		ctx.HTML("Should be <b>/anything/any/path</b>: " + myrouteRequestPath)
	})

	// execute a route, similar to redirect but without redirect :)
	app.Get("/execute_myroute", func(ctx iris.Context) {
		ctx.Exec("GET", "/anything/any/path") // like it was called by the client.
	})

	// http://localhost:8080/reverse_myroute
	// http://localhost:8080/execute_myroute
	// http://localhost:8080/anything/any/path/here
	//
	// See view/template_html_4 example for more reverse routing examples
	// using the reverse router component and the {{url}} and {{urlpath}} template functions.
	app.Run(iris.Addr(":8080"))
}
