package main

import (
	"github.com/kataras/iris/v12"

	"github.com/casbin/casbin/v2"
	cm "github.com/iris-contrib/middleware/casbin"
)

// $ go get github.com/casbin/casbin
// $ go run main.go

// Enforcer maps the model and the policy for the casbin service, we use this variable on the main_test too.
var Enforcer, _ = casbin.NewEnforcer("casbinmodel.conf", "casbinpolicy.csv")

func newApp() *iris.Application {
	casbinMiddleware := cm.New(Enforcer)

	app := iris.New()
	app.WrapRouter(casbinMiddleware.Wrapper())

	app.Get("/", hi)

	app.Any("/dataset1/{p:path}", hi) // p, dataset1_admin, /dataset1/*, * && p, alice, /dataset1/*, GET

	app.Post("/dataset1/resource1", hi)

	app.Get("/dataset2/resource2", hi)
	app.Post("/dataset2/folder1/{p:path}", hi)

	app.Any("/dataset2/resource1", hi)

	return app
}

func main() {
	app := newApp()
	app.Run(iris.Addr(":8080"))
}

func hi(ctx iris.Context) {
	ctx.Writef("Hello %s", cm.Username(ctx.Request()))
}
