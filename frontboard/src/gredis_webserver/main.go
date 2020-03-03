package main

import (
	"common"
	"lib/env"
	"lib/glog"
	"lib/service"
	"time"

	"github.com/kataras/iris"
)

var g_process_name string = "gredis_webserver"

type FrontBoard struct {
	service.Service
}

var g_front_board *FrontBoard

func FrontBoard_GetMe() *FrontBoard {
	if g_front_board == nil {
		g_front_board = &FrontBoard{}
		g_front_board.Derived = g_front_board
	}
	return g_front_board
}

var urlPre string = "http://"

var startURL = urlPre + env.Get(g_process_name, "listen")

func (this *FrontBoard) Init() bool {
	if !AdminRedis_GetMe().Init(env.Get(g_process_name, "admindb")) {
		return false
	}
	glog.Info("[init] 和管理数据库连接成功 ", env.Get(g_process_name, "admindb"))

	app := iris.New()
	Router_Init(app)
	app.Run(iris.Addr(env.Get(g_process_name, "listen")), iris.WithSitemap(startURL))
	glog.Info("[init] 启动成功")
	return true
}

func (this *FrontBoard) MainLoop() {
	time.Sleep(1 * time.Second)
}

func (this *FrontBoard) Reload() {

}

func (this *FrontBoard) Final() bool {
	return true
}

func (this *FrontBoard) ServerStopped() bool {
	return true
}

func main() {
	if !common.InitEnv(g_process_name) {
		return
	}

	FrontBoard_GetMe().Main(env.Get(g_process_name, "pid"))
}
