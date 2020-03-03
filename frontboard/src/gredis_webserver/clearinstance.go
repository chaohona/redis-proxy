package main

import (
	"common"
	"encoding/json"
	"lib/env"
	"lib/glog"
	"os/exec"
	"strings"

	"github.com/kataras/iris"
)

type ClearInstance_Url struct {
	Id uint64 `url:"id"`
}

func clearInstance(ctx iris.Context) {
	var result common.ErrInfo
	data := GetRequestPayload(ctx)
	var t ClearInstance_Url
	err := json.Unmarshal(data, &t)
	if err != nil {
		glog.Error("创建新的Redis获取参数失败 err:", err)
		result = common.GetErrInfo(common.ERR_URL_PARSE)
		ctx.JSON(result)
		return
	}
	glog.Info("收到清除Redis请求, id:", t.Id)
	// 获取Redis信息,ip:port
	retCode, redisInfo := AdminRedis_GetMe().GetRedisInfo(t.Id)
	if retCode != common.ERR_OK {
		glog.Error("清除Redis数据，获取信息出错 id:", t.Id)
		result = common.GetErrInfo(common.ERR_INVALID_REDIS)
		ctx.JSON(result)
		return
	}

	vDatas := strings.Split(redisInfo.Addr, ":")
	if len(vDatas) != 2 {
		glog.Error("清除Redis数据，获取地址出错 id:", t.Id, ", name:", redisInfo.Name, ", addr:", redisInfo.Addr)
		result = common.GetErrInfo(common.ERR_INVALID_REDIS)
		ctx.JSON(result)
		return
	}
	// 连接Redis,发送清除命令
	cmd := exec.Command(env.Get(g_process_name, "basepath")+"/redis/bin/redis-cli", "-h", string(vDatas[0]), "-p", string(vDatas[1]), "_reset_")
	err = cmd.Run()
	if err != nil {
		glog.Error("清除Redis数据，执行命令出错 id:", t.Id, ", name:", redisInfo.Name, ", addr:", redisInfo.Addr, ", err:", err)
		result = common.GetErrInfo(common.ERR_INVALID_REDIS)
		ctx.JSON(result)
		return
	}
	ctx.JSON(result)
}
