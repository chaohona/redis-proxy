package main

import (
	"common"
	"encoding/json"
	"lib/glog"

	"github.com/kataras/iris"
)

type CreateInstanceRet struct {
	RedisInstance
	Ret common.ErrInfo
}

type CreateInstance_Url struct {
	Name string `url:"name"`
}

func createInstance(ctx iris.Context) {
	var result CreateInstanceRet
	data := GetRequestPayload(ctx)

	var t CreateInstance_Url
	err := json.Unmarshal(data, &t)
	if err != nil {
		glog.Error("创建新的Redis获取参数失败 err:", err)
		result.Ret = common.GetErrInfo(common.ERR_URL_PARSE)
		ctx.JSON(result)
		return
	}
	glog.Info("收到创建新的Redis请求, name:", t.Name)
	if !AdminRedis_GetMe().CreateRedisCheck() {
		glog.Error("不能创建更多的Redis, name:", t.Name)
		result.Ret = common.GetErrInfo(common.ERR_REDIS_POOLS_EMPTY)
		ctx.JSON(result)
		return
	}

	errCode, redisInfo := AdminRedis_GetMe().CreateRedis(t.Name)
	if errCode != common.ERR_OK {
		glog.Error("创建新的Redis失败 name:", t.Name)
		result.Ret = common.GetErrInfo(errCode)
		ctx.JSON(result)
		return
	}

	RedisInstance_Conv(&(result.RedisInstance), redisInfo)
	result.Ret = common.GetErrInfo(common.ERR_OK)

	glog.Info("创建新的Redis成功, name:", t.Name, ", id:", redisInfo.Id)
	ctx.JSON(result)
}
