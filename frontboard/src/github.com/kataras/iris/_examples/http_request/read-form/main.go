// package main contains an example on how to use the ReadForm, but with the same way you can do the ReadJSON & ReadJSON
package main

import (
	"github.com/kataras/iris/v12"
)

type Visitor struct {
	Username string
	Mail     string
	Data     []string `form:"mydata"`
}

func main() {
	app := iris.New()

	// set the view html template engine
	app.RegisterView(iris.HTML("./templates", ".html").Reload(true))

	app.Get("/", func(ctx iris.Context) {
		if err := ctx.View("form.html"); err != nil {
			ctx.StatusCode(iris.StatusInternalServerError)
			ctx.WriteString(err.Error())
		}
	})

	app.Post("/form_action", func(ctx iris.Context) {
		visitor := Visitor{}
		err := ctx.ReadForm(&visitor)
		if err != nil && !iris.IsErrPath(err) /* see: https://github.com/kataras/iris/issues/1157 */ {
			ctx.StatusCode(iris.StatusInternalServerError)
			ctx.WriteString(err.Error())
		}

		ctx.Writef("Visitor: %#v", visitor)
	})

	app.Post("/post_value", func(ctx iris.Context) {
		username := ctx.PostValueDefault("Username", "iris")
		ctx.Writef("Username: %s", username)
	})

	app.Run(iris.Addr(":8080"))
}
