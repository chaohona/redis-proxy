package main

import (
	"github.com/kataras/iris"
)

func findInstance(ctx iris.Context) {
	var result = RedisInstance{
		Id:      1,
		Name:    "name1",
		MemUsed: 1000,
		DBSize:  1000,
		CTime:   1582686617,
	}

	ctx.JSON(result)
}
