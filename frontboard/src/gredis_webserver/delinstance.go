package main

import (
	"common"
	"lib/glog"

	"encoding/json"

	"github.com/kataras/iris"
)

type DelInstance_Url struct {
	Id uint64 `url:"id"`
}

func delInstance(ctx iris.Context) {
	var result common.ErrInfo

	var t DelInstance_Url
	data := GetRequestPayload(ctx)
	err := json.Unmarshal(data, &t)

	if err != nil {
		glog.Error("删除Redis获取参数失败 err:", err)
		result = common.GetErrInfo(common.ERR_URL_PARSE)
		ctx.JSON(result)
		return
	}

	glog.Info("收到回收Redis请求, id:", t.Id)
	if !AdminRedis_GetMe().InvalidRedis(t.Id) {
		glog.Error("删除Redis不存在, id:", t.Id)
		result = common.GetErrInfo(common.ERR_INVALID_REDIS)
		ctx.JSON(result)
		return
	}

	errCode, redisInfo := AdminRedis_GetMe().GetRedisInfo(t.Id)
	if errCode != common.ERR_OK {
		glog.Error("删除Redis获取信息失败, id:", t.Id)
		result = common.GetErrInfo(common.ERR_INVALID_REDIS)
		ctx.JSON(result)
		return
	}

	if !AdminRedis_GetMe().RecycleRedis(redisInfo) {
		glog.Error("删除Redis操作数据库失败, id:", t.Id, ", name:", redisInfo.Name)
		result = common.GetErrInfo(common.ERR_DB)
		ctx.JSON(result)
		return
	}

	glog.Info("回收Redis成功, id:", t.Id, ", name:", redisInfo.Name)
	ctx.JSON(result)
}
