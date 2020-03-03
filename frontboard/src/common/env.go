package common

import (
	"flag"
	"lib/env"
	"lib/glog"
)

// 进程环境初始化
// svrName为配置文件中本模块的名字
func InitEnv(svrName string) bool {
	var (
		logfile = flag.String("logfile", "", "Log file name")
		config  = flag.String("config", "config.json", "config path")
	)
	flag.Parse()

	if !env.Load(*config) {
		return false
	}

	loglevel := env.Get("global", "loglevel")
	if loglevel != "" {
		flag.Lookup("stderrthreshold").Value.Set(loglevel)
		glog.SetLogLevel(loglevel)
	}

	logtostderr := env.Get("global", "logtostderr")
	if logtostderr != "" {
		flag.Lookup("logtostderr").Value.Set(logtostderr)
	}

	if *logfile != "" {
		glog.SetLogFile(*logfile)
	} else {
		glog.SetLogFile(env.Get(svrName, "log"))
	}
	return true
}
