package service

import (
	"lib/glog"
	"math/rand"
	"os"
	"os/signal"
	"runtime"
	"runtime/debug"
	"syscall"
	"time"

	"github.com/goinbox/pidfile"
)

type IService interface {
	Init() bool
	MainLoop()
	Reload()
	Final() bool
}

type Service struct {
	terminate bool
	Derived   IService
}

func (this *Service) Terminate() {
	this.terminate = true
}

func (this *Service) isTerminate() bool {
	return this.terminate
}

func (this *Service) SetCpuNum(num int) {
	if num > 0 {
		runtime.GOMAXPROCS(num)
	} else if num == -1 {
		runtime.GOMAXPROCS(runtime.NumCPU())
	}
}

func (this *Service) Main(pidfilepath string) bool {
	var pidFiles *pidfile.PidFile
	defer func() {
		this.Derived.Final()
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
		if pidFiles != nil {
			err := pidFiles.Clear()
			if err != nil {
				glog.Error("[关闭] 删除pid文件出错 ", pidfilepath)
			}
		}
		glog.Info("关闭服务器完成")
		glog.Flush()
	}()
	if pidfilepath != "" {
		var err error
		pidFiles, err = pidfile.CreatePidFile(pidfilepath)
		if err != nil || pidFiles == nil {
			glog.Error("[启动] 创建pid文件出错 ", pidfilepath, ",", err)
			return false
		}
	}
	rand.Seed(time.Now().Unix() ^ int64(os.Getpid()))
	if this.Derived == nil {
		glog.Error("[启动] 没有设置驱动程序,需要给接口Derived赋值")
		return false
	}

	ch := make(chan os.Signal, 1)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGQUIT, syscall.SIGABRT, syscall.SIGTERM, syscall.SIGPIPE, syscall.SIGHUP)
	go func() {
		for sig := range ch {
			switch sig {
			case syscall.SIGHUP:
				glog.Info("[服务] 收到重新加载服务信号")
				this.Derived.Reload()
			case syscall.SIGPIPE:
			default:
				this.Terminate()
			}
			glog.Info("[服务] 收到信号 ", sig)
		}
	}()

	runtime.GOMAXPROCS(runtime.NumCPU())

	glog.Info("[启动] 开始初始化")
	if !this.Derived.Init() {
		glog.Error("[启动] 初始化失败")
		return false
	}
	glog.Info("[启动] 初始化成功")

	for !this.isTerminate() {
		this.Derived.MainLoop()
	}

	return true
}
