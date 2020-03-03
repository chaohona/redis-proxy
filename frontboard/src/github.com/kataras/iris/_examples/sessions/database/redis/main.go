package main

import (
	"errors"
	"time"

	"github.com/kataras/iris/v12"

	"github.com/kataras/iris/v12/sessions"
	"github.com/kataras/iris/v12/sessions/sessiondb/redis"
)

// tested with redis version 3.0.503.
// for windows see: https://github.com/ServiceStack/redis-windows
func main() {
	// These are the default values,
	// you can replace them based on your running redis' server settings:
	db := redis.New(redis.Config{
		Network:   "tcp",
		Addr:      "127.0.0.1:6379",
		Timeout:   time.Duration(30) * time.Second,
		MaxActive: 10,
		Password:  "",
		Database:  "",
		Prefix:    "",
		Delim:     "-",
		Driver:    redis.Redigo(), // redis.Radix() can be used instead.
	})

	// Optionally configure the underline driver:
	// driver := redis.Redigo()
	// driver.MaxIdle = ...
	// driver.IdleTimeout = ...
	// driver.Wait = ...
	// redis.Config {Driver: driver}

	// Close connection when control+C/cmd+C
	iris.RegisterOnInterrupt(func() {
		db.Close()
	})

	defer db.Close() // close the database connection if application errored.

	sess := sessions.New(sessions.Config{
		Cookie:       "sessionscookieid",
		Expires:      0, // defaults to 0: unlimited life. Another good value is: 45 * time.Minute,
		AllowReclaim: true,
	})

	//
	// IMPORTANT:
	//
	sess.UseDatabase(db)

	// the rest of the code stays the same.
	app := iris.New()
	// app.Logger().SetLevel("debug")

	app.Get("/", func(ctx iris.Context) {
		ctx.Writef("You should navigate to the /set, /get, /delete, /clear,/destroy instead")
	})
	app.Get("/set", func(ctx iris.Context) {
		s := sess.Start(ctx)
		// set session values
		s.Set("name", "iris")

		// test if set here
		ctx.Writef("All ok session value of the 'name' is: %s", s.GetString("name"))
	})

	app.Get("/set/{key}/{value}", func(ctx iris.Context) {
		key, value := ctx.Params().Get("key"), ctx.Params().Get("value")
		s := sess.Start(ctx)
		// set session values
		s.Set(key, value)

		// test if set here
		ctx.Writef("All ok session value of the '%s' is: %s", key, s.GetString(key))
	})

	app.Get("/set/int/{key}/{value}", func(ctx iris.Context) {
		key := ctx.Params().Get("key")
		value, _ := ctx.Params().GetInt("value")
		s := sess.Start(ctx)
		// set session values
		s.Set(key, value)
		valueSet := s.Get(key)
		// test if set here
		ctx.Writef("All ok session value of the '%s' is: %v", key, valueSet)
	})

	app.Get("/get/{key}", func(ctx iris.Context) {
		key := ctx.Params().Get("key")
		value := sess.Start(ctx).Get(key)

		ctx.Writef("The '%s' on the /set was: %v", key, value)
	})

	app.Get("/get", func(ctx iris.Context) {
		// get a specific key, as string, if no found returns just an empty string
		name := sess.Start(ctx).GetString("name")

		ctx.Writef("The 'name' on the /set was: %s", name)
	})

	app.Get("/get/{key}", func(ctx iris.Context) {
		// get a specific key, as string, if no found returns just an empty string
		name := sess.Start(ctx).GetString(ctx.Params().Get("key"))

		ctx.Writef("The name on the /set was: %s", name)
	})

	app.Get("/delete", func(ctx iris.Context) {
		// delete a specific key
		sess.Start(ctx).Delete("name")
	})

	app.Get("/clear", func(ctx iris.Context) {
		// removes all entries
		sess.Start(ctx).Clear()
	})

	app.Get("/destroy", func(ctx iris.Context) {
		// destroy, removes the entire session data and cookie
		sess.Destroy(ctx)
	})

	app.Get("/update", func(ctx iris.Context) {
		// updates resets the expiration based on the session's `Expires` field.
		if err := sess.ShiftExpiration(ctx); err != nil {
			if errors.Is(err, sessions.ErrNotFound) {
				ctx.StatusCode(iris.StatusNotFound)
			} else if errors.Is(err, sessions.ErrNotImplemented) {
				ctx.StatusCode(iris.StatusNotImplemented)
			} else {
				ctx.StatusCode(iris.StatusNotModified)
			}

			ctx.Writef("%v", err)
			ctx.Application().Logger().Error(err)
		}
	})

	app.Run(iris.Addr(":8080"), iris.WithoutServerError(iris.ErrServerClosed))
}
