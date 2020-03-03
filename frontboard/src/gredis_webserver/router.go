package main

import (
	"github.com/kataras/iris"
)

func GetRequestPayload(ctx iris.Context) []byte {
	buf := make([]byte, 2048)
	n, _ := ctx.Request().Body.Read(buf)
	return buf[0:n]
}

func Router_Init(router *iris.Application) {
	router.HandleDir("/redis/api/v1/", "./dist", iris.DirOptions{
		IndexName: "/index.html",
		Gzip:      false,
		ShowList:  false,
	})
	router.HandleDir("/", "./dist", iris.DirOptions{
		IndexName: "/index.html",
		Gzip:      false,
		ShowList:  false,
	})

	router.RegisterView(iris.HTML("./dist", ".html"))
	router.Get("/", index)
	router.Get("/redis", index)

	//redislist
	router.Get("/redis/api/v1/redislist", redisList) // 获取线上redis列表,返回RedisListRet
	//getinstance?id=1
	router.Get("/redis/api/v1/getinstance", getInstance) // 返回RedisInstance
	//findinstance?name=123 实例名 返回RedisInstance
	router.Get("/redis/api/v1/findinstance", findInstance) // 查找线上Redis实例
	//delinstance?id=123 实例编号 返回GRedisRet
	router.Delete("/redis/api/v1/delinstance", delInstance) // 删除线上实例
	//createinstance?name=123 返回RedisInstance
	router.Post("/redis/api/v1/createinstance", createInstance) // 创建新的实例
	//clearinstance?id=123
	router.Post("/redis/api/v1/clearinstance", clearInstance)
}

func index(ctx iris.Context) {
	ctx.View("index.html")
}
