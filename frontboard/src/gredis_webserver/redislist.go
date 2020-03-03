package main

import (
	"github.com/kataras/iris"
)

func redisList(ctx iris.Context) {
	var result RedisListRet = AdminRedis_GetMe().GetRedisList()

	ctx.JSON(result)
}
