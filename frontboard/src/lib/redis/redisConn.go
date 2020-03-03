package redis

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"lib/glog"
	"net"
	"strconv"
	"sync/atomic"

	//"container/list"
	//"reflect"
	//"time"
	"errors"
	"time"
)

var (
	STRTRUE      = "1"
	STRFALSE     = "0"
	STRLINEBREAK = "\r\n"
	STRNIL       = ""
)

var (
	print_profile_log_flag uint32
)

// 设置是否打印性能日志接口
func PrintRedisProfileLog(flag uint32) {
	atomic.StoreUint32(&print_profile_log_flag, flag)
}

const (
	CmdChanBusyWarn  = "CmdChan will be full"
	WaitChanBufyWarn = "WaitChan will be full"
)

const (
	NET_EXCEPIOTN_SIGNAL = 0
	NET_CLOSE_SIGNAL     = 1
)

const (
	Default_Warn_TT  = 10   // 10秒告警
	Default_Recon_TT = 1800 // 半小时重连
)

var (
	ErrBusy          = errors.New("Busy")
	ErrInvalidArgNum = errors.New("Invalid args number")
)

const (
	NetClosed           int32 = 0
	NetOpen             int32 = 1
	RedisConDead        int32 = 0 //调用Close关闭
	RedisConExcetion    int32 = 1 //正在处理异常流程(目前只有和redis网络断链的情况)
	RedisClosedFriendly int32 = 2 //被友好地关闭，需要处理缓冲chan中的命令完再退出
	RedisConAlive       int32 = 3 //可以正常使用，可以接受外部命令
)

const (
	defaultWaitChCap = 5000
	defaultCmdChCap  = 5000
)

const (
	ConRetryTimes  = -1
	ConRetryDelayS = 1

	TryClosedDelayMS = 1 //尝试关闭RedisCon的时候休眠周期
)

var (
	ErrRedisConClose = errors.New("RedisCon is closed")
	ErrRedisNetFatal = errors.New("The connection with redis is fatal")
)

const (
	ARGSNUMARRAY = -1
	ARGSNUM0     = 0
	ARGSNUM1     = 1
	ARGSNUM2     = 2
	ARGSNUM3     = 3
	ARGSKEY      = 4
)

type RedisCon struct {
	conn       net.Conn
	br         *bufio.Reader
	bw         *bufio.Writer
	serverIdx  int
	server     string
	lenScratch [32]byte
	numScratch [40]byte
	err        error

	LifeCycle int32

	conRetryTimes   int
	conRetryDelayS  time.Duration
	conRetriedTimes int

	NetStatus int32

	cmdChan     chan *Command   //等待发送到redis的命令的chan
	cmdsChan    chan []*Command //批量命令chan
	cmdChanCap  int
	waitChan    chan *Command //发送到redis，等待结果的命令的chan
	waitChanCap int

	waitProcessCmdNum int64 //调用Send成功，但是还没有返回的命令，只有这个值为0，异常处理协程才可以退出
	TryClosedDelayMS  time.Duration
	cmdPool           *cmdPool

	recvIndex uint64
	sendIndex uint64

	warnTT  int64
	reConTT int64

	signal chan int

	addressFunc func() string
}

func (c *RedisCon) cleanChan(ch chan *Command, err error) {
	//防止cmd.ResultChan被关闭
	defer func() {
		if e := recover(); e != nil {
			glog.Error(e)
		}
	}()
	for len(ch) > 0 {
		select {
		case cmd := <-ch:
			c.closeUserChan(cmd)
		default:
		}
	}
}

func (c *RedisCon) closeChan(ch chan *Command, err error) {
	defer func() {
		if r := recover(); r != nil {
			if r != "close of closed channel" {
				glog.Error(r)
			}
		}
	}()
	c.cleanChan(ch, err)
	close(ch)
}

func (c *RedisCon) exit() {
}

func (c *RedisCon) SendExceptionSignal(signal int) {
	select {
	case c.signal <- signal:
	default:
	}
}

func exceptionProcess(redisCon *RedisCon) {
	for {
		<-redisCon.signal
		if !redisCon.cycleIsAlive() && redisCon.getProcessingCmdNum() <= 0 { //生命周期结束
			redisCon.closeChan(redisCon.cmdChan, ErrRedisConClose)
			redisCon.closeChan(redisCon.waitChan, ErrRedisConClose)
			glog.Error("[Redis] 手动关闭redis服务,", redisCon.server)
			break
		}
		if redisCon.cycleIsDead() { //处理外部强制关闭情况
			for {
				redisCon.cleanChan(redisCon.cmdChan, ErrRedisConClose)
				redisCon.cleanChan(redisCon.waitChan, ErrRedisConClose)
				if redisCon.getProcessingCmdNum() <= 0 { //缓冲区中的命令处理完了退出
					break
				}
			}
		} else if redisCon.cycleIsClosedFriendly() { //处理外部想尝试关闭链接的情况
			for {
				if redisCon.IsNetClose() { //网络层出问题处理不了了，退出
					break
				}
				if redisCon.getProcessingCmdNum() <= 0 { //缓冲区中的命令处理完了退出
					redisCon.Close()
					break
				}
				time.Sleep(time.Millisecond * redisCon.TryClosedDelayMS)
			}
		}

		//处理网络异常情况
		for redisCon.IsNetClose() && redisCon.cycleIsAlive() {
			glog.Error("[Redis] 检查到异常，开始重新连接,", redisCon.server)
			redisCon.tryChangeLifeCycle(RedisConAlive, RedisConExcetion) //将状态置为异常状态
			//清理已经发送到redis但是没有得到结果的chan缓冲区，这部分再不会得到实际的执行结果了
			for {
				redisCon.cleanChan(redisCon.waitChan, ErrRedisNetFatal)
				if redisCon.getProcessingCmdNum() <= int64(len(redisCon.cmdChan)) { //只有cmdChan才有没有处理的命令，其它命令都处理完了
					redisCon.closeChan(redisCon.waitChan, ErrRedisNetFatal) //这个地方关闭，防止有外部再往waitChan里面写命令。重连成功之后再开启waitChan
					break
				}
			}
			if redisCon.cycleIsAlive() && (redisCon.conRetryTimes < 0 || redisCon.conRetriedTimes < redisCon.conRetryTimes) {
				time.Sleep(time.Second * ConRetryDelayS)
				err := redisCon.resetNet()
				if err == nil {
					break
				} else {
					glog.Error("[Redis] 检查到异常，重新连接失败,", redisCon.server, ",", err)
				}
				if redisCon.conRetriedTimes > 1 { //如果重连redis一次失败，则清空cmdChan中的命令
					redisCon.cleanChan(redisCon.cmdChan, err)
				}
			} else { //如果超过重连次数还不成功则关闭
				glog.Error("[Redis] 检查到异常，超过重连次数还不成功则关闭,", redisCon.server)
				redisCon.Close()
				break
			}
		}
		//time.Sleep(time.Second * ConRetryDelayS)
	}
}

func processReceiveRedis(redisCon *RedisCon) {
	defer func() {
		//处理ResultChan被关闭的情况
		if r := recover(); r != nil {
			glog.Error("[Redis] ResultChan被关闭的情况,", redisCon.server)
			//			if !redisCon.cycleIsDead() {
			//				processReceiveRedis(redisCon)
			//			}
		}
	}()
	if redisCon == nil {
		glog.Error("[Redis] redisCon is nil")
		return
	}
	var (
		maxTime   int64 // 最大耗时
		minTime   int64 // 最小耗时
		cmdCnt    int64 // cmd个数
		nowNano   int64 // 当前时间
		startTime int64 // 统计开始时间
		totalTime int64 // 所有cmd所用时间总和
		thisTime  int64 // 当前cmd所用时间
		cmdName   string
		_         = cmdName
	)
	for {
		if redisCon.IsNetClose() {
			if redisCon.cycleIsDead() { //只有声明周期处于Dead状态才退出
				break
			}
			//glog.Error("Net connect to redis failed, will try connect to it")
			time.Sleep(time.Second * redisCon.conRetryDelayS)
			continue
		}
		data, err := redisCon.receive()
		cmd := <-redisCon.waitChan
		result := &Result{
			Data: data,
			Err:  err,
		}
		if atomic.LoadUint32(&print_profile_log_flag) != 0 {
			nowNano = time.Now().UnixNano()
			if startTime == 0 {
				startTime = cmd.recvTime
			}
			cmdCnt += 1
			thisTime = nowNano - cmd.recvTime
			if maxTime == 0 || thisTime > maxTime {
				maxTime = thisTime
				cmdName = *cmd.Cmd
			}
			if minTime == 0 || minTime > thisTime {
				minTime = thisTime
			}
			totalTime += thisTime
			if nowNano-startTime > 300000000000 { // 5分钟做一次性能统计
				glog.Info("[redis] 性能统计 ", maxTime, ",", minTime, ",", totalTime/cmdCnt, ",", cmdCnt, ",", cmdName, ",", redisCon.server)
				startTime = 0
				maxTime = 0
				minTime = 0
				cmdCnt = 0
				totalTime = 0
			}
		}

		redisCon.sendResultToUser(result, cmd)
	}
}

func processReceiveUser(redisCon *RedisCon) {
	defer func() {
		if r := recover(); r != nil {
			glog.Error("[Redis] processReceiveUser panic,", redisCon.server)
			//			if !redisCon.cycleIsDead() {
			//				processReceiveUser(redisCon)
			//			}
		}
	}()
	if redisCon == nil {
		glog.Error("redisCon is nil")
		return
	}
	for {
		if redisCon.IsNetClose() {
			if redisCon.cycleIsDead() {
				break
			}
			//glog.Error("Net connect to redis failed, will try connect to it")
			time.Sleep(time.Second * redisCon.conRetryDelayS)
			continue
		}
		select {
		case cmd := <-redisCon.cmdChan:
			if cmd == nil {
				break
			}
			if err := redisCon.sendToRedis(cmd); err != nil {
				continue
			}
			if err := redisCon.Flush(); err != nil {
				redisCon.closeUserChan(cmd)
				glog.Info("[Redis] Failed send cmd to redis", redisCon.server)
				continue
			}
			continue
		}
	}
}

func processTimeOut(redisCon *RedisCon) {
	defer func() {
		if e := recover(); e != nil {
			glog.Error(e)
		}
	}()
	var lastRecvId uint64
	var index, ttTimes, ttTime int64
	for {
		time.Sleep(1 * time.Second)
		index += 1
		rId := atomic.LoadUint64(&redisCon.recvIndex) // 收到结果的下标
		sId := atomic.LoadUint64(&redisCon.sendIndex) // 发送请求的下标
		if rId != lastRecvId {                        // 收到新消息重置超时
			index = 0
			lastRecvId = rId
			//lastSendId = sId
		} else {
			if index%redisCon.warnTT == 0 && sId-rId > 100 { // 报警处理
				glog.Error("[redis] redis出现超时情况 ", ",", redisCon.warnTT, ",", redisCon.conn.RemoteAddr())
				now := time.Now().Unix()
				if now-ttTime > 60 {
					ttTime = now
					ttTimes = 0
				}
				ttTimes += 1
				if ttTimes > 5 {
					glog.Error("[redis] redis出现超时次数太多重置链接 ", redisCon.conn.RemoteAddr())
					redisCon.resetNet()
					ttTime = 0
					ttTimes = 0
				}
			}
			if index%redisCon.reConTT == 0 && sId-rId > 100 { // 超时处理
				glog.Error("[redis] redis出现严重超时情况 ", redisCon.reConTT, ",", redisCon.conn.RemoteAddr())
			}
		}
	}
}

func NewRedisConWithTimeOut(server string, passwd string, cmdChanCap int, waitChanCap int, warnTT int64, reconTT int64) (redisCon *RedisCon, err error) {
	con, err := NewRedisCon(server, cmdChanCap, waitChanCap)
	if err != nil {
		return con, err
	}
	con.warnTT = warnTT
	con.reConTT = reconTT
	return con, err
}

//和redis建立连接的同事创建两个goroution，一个用于像redis发送命令，一个用于接收处理结果
func NewRedisCon(server string, cmdChanCap int, waitChanCap int) (redisCon *RedisCon, err error) {
	svr, pwd, _ := ParseRedisKey(server)
	if len(svr) == 0 {
		glog.Error("[Redis] Invalid-redis-server address:", server)
		return nil, errors.New("Invalid redis server address")
	}
	redisCon, err = conRedis(svr)
	if err != nil {
		glog.Error("[Redis] Con to redis failed:" + svr)
		return
	}

	if cmdChanCap <= 0 {
		cmdChanCap = defaultCmdChCap
	}
	if waitChanCap <= 0 {
		waitChanCap = defaultWaitChCap
	}
	redisCon.cmdChanCap = cmdChanCap
	redisCon.waitChanCap = waitChanCap
	redisCon.conRetryTimes = ConRetryTimes
	redisCon.conRetryDelayS = ConRetryDelayS
	redisCon.conRetriedTimes = 0
	redisCon.TryClosedDelayMS = TryClosedDelayMS
	redisCon.serverIdx = 0
	redisCon.server = svr

	redisCon.signal = make(chan int, 10)

	err = redisCon.init()
	if err != nil {
		return
	}

	//创建从cmdChan中获取命令发送到redis的goroution
	go func() {
		for {
			processReceiveUser(redisCon)
			if redisCon.cycleIsDead() {
				break
			}
		}
	}()

	//创建从redis获取执行结果的协程
	go func() {
		for {
			processReceiveRedis(redisCon)
			if redisCon.cycleIsDead() {
				break
			}
		}
	}()

	//处理和redis链接异常，正常关闭流程
	go exceptionProcess(redisCon)
	go processTimeOut(redisCon)

	_ = pwd
	if len(pwd) > 0 {
		ch := make(chan *Result)
		args := Args{}.Add(pwd)
		redisCon.SendToConn(ch, "AUTH", (*([]interface{}))(&args))
		result := <-ch
		ret, rerr := String(result.Data, result.Err)
		if rerr != nil {
			glog.Error("[Redis] Ger auth result failed,", svr)
			redisCon.fatal(rerr)
			return nil, rerr
		}
		if ret != "OK" {
			glog.Error("[Redis] Invalid password,", svr)
			rerr = errors.New("invalid passwd")
			redisCon.fatal(rerr)
			return nil, rerr
		}
	}

	return redisCon, nil
}

func (c *RedisCon) SetWarnTimeOut(t int64) {
	c.warnTT = t
}

func (c *RedisCon) SetReconTimeOut(t int64) {
	c.reConTT = t
}

func (c *RedisCon) getProcessingCmdNum() int64 {
	return atomic.LoadInt64(&c.waitProcessCmdNum)
}

func (c *RedisCon) getLifeCycle() int32 {
	return atomic.LoadInt32(&c.LifeCycle)
}

func (c *RedisCon) cycleIsAlive() bool {
	return atomic.LoadInt32(&c.LifeCycle) == RedisConAlive || c.cycleIsExcetion()
}

func (c *RedisCon) cycleIsDead() bool {
	return atomic.LoadInt32(&c.LifeCycle) == RedisConDead
}

func (c *RedisCon) cycleIsExcetion() bool {
	return atomic.LoadInt32(&c.LifeCycle) == RedisConExcetion
}

func (c *RedisCon) cycleIsClosedFriendly() bool {
	return atomic.LoadInt32(&c.LifeCycle) == RedisClosedFriendly
}

func (c *RedisCon) changeLifeCycle(newLifeCycle int32) int32 {
	if c.getLifeCycle() != newLifeCycle {
		c.LifeCycle = newLifeCycle
	}
	return c.LifeCycle
}

func (c *RedisCon) tryChangeLifeCycle(oldLifeCycle int32, newLifeCycle int32) bool {
	return atomic.CompareAndSwapInt32(&c.LifeCycle, oldLifeCycle, newLifeCycle)
}

func (c *RedisCon) Do(cmd string, args ...interface{}) (interface{}, error) {
	ch := make(chan *Result, 1)
	err := c.SendToConn(ch, cmd, &args)
	if err == nil {
		r := <-ch
		return r.Data, r.Err
	}
	return nil, err
}

func (c *RedisCon) DoKeyArgs(cmd string, key interface{}, args *[]interface{}) (interface{}, error) {
	ch := make(chan *Result, 1)
	err := c.sendToCmdChanArgsKey(ch, &cmd, key, args)
	if err == nil {
		r := <-ch
		return r.Data, r.Err
	}
	return nil, err
}

func (c *RedisCon) DoKey(cmd string, key interface{}, args ...interface{}) (interface{}, error) {
	ch := make(chan *Result, 1)
	err := c.sendToCmdChanArgsKey(ch, &cmd, key, &args)
	if err == nil {
		r := <-ch
		return r.Data, r.Err
	}
	return nil, err
}

func (c *RedisCon) DoArg1(cmd string, arg interface{}) (interface{}, error) {
	ch := make(chan *Result, 1)
	err := c.sendToCmdChanArgsNum1(ch, &cmd, arg)
	if err == nil {
		r := <-ch
		return r.Data, r.Err
	}
	return nil, err
}

func (c *RedisCon) DoArg2(cmd string, arg1 interface{}, arg2 interface{}) (interface{}, error) {
	ch := make(chan *Result, 1)
	err := c.sendToCmdChanArgsNum2(ch, &cmd, arg1, arg2)
	if err == nil {
		r := <-ch
		return r.Data, r.Err
	}
	return nil, err
}

func (c *RedisCon) DoArg3(cmd string, arg1 interface{}, arg2 interface{}, arg3 interface{}) (interface{}, error) {
	ch := make(chan *Result, 1)
	err := c.sendToCmdChanArgsNum3(ch, &cmd, arg1, arg2, arg3)
	if err == nil {
		r := <-ch
		return r.Data, r.Err
	}
	return nil, err
}

//send做的只是把命令放到cmdChan中
//只有再RedisCon没有被主动关闭，并且和redis链接没问题的时候才能发送成功
func (c *RedisCon) Send(cmd string, args ...interface{}) error {
	return c.SendToConn(nil, cmd, &args)
}

func (c *RedisCon) SendArg0(cmd string) error {
	return c.sendToCmdChanArgsNum0(nil, &cmd)
}
func (c *RedisCon) SendArg1(cmd string, arg1 interface{}) error {
	return c.sendToCmdChanArgsNum1(nil, &cmd, arg1)
}
func (c *RedisCon) SendArg2(cmd string, arg1 interface{}, arg2 interface{}) error {
	return c.sendToCmdChanArgsNum2(nil, &cmd, arg1, arg2)
}
func (c *RedisCon) SendArg3(cmd string, arg1 interface{}, arg2 interface{}, arg3 interface{}) error {
	return c.sendToCmdChanArgsNum3(nil, &cmd, arg1, arg2, arg3)
}

//send做的只是把命令放到cmdChan中
func (c *RedisCon) SendToConn(ch chan *Result, cmd string, args *[]interface{}) error {
	if !c.IsNetClose() {
		return c.SendToCmdChan(ch, &cmd, args)
	} else {
		return ErrRedisConClose
	}
}

//TODO用于打印资源使用情况
func (c *RedisCon) usagePrint() {
}

func (c *RedisCon) packageRedisCmd(cmd *string, argNum int, arg1 interface{}, arg2 interface{}, arg3 interface{}, args *[]interface{}) (command *Command) {
	command = c.cmdPool.Get(c)
	command.recvTime = time.Now().UnixNano()
	switch argNum {
	case ARGSNUMARRAY:
		command.CmdNum = argNum
		command.Cmd = cmd
		command.Args = args
	case ARGSNUM0:
		command.CmdNum = argNum
		command.Cmd = cmd
	case ARGSNUM1:
		command.CmdNum = argNum
		command.Cmd = cmd
		command.Arg1 = arg1
	case ARGSNUM2:
		command.CmdNum = argNum
		command.Cmd = cmd
		command.Arg1 = arg1
		command.Arg2 = arg2
	case ARGSNUM3:
		command.CmdNum = argNum
		command.Cmd = cmd
		command.Arg1 = arg1
		command.Arg2 = arg2
		command.Arg3 = arg3
	case ARGSKEY:
		command.CmdNum = argNum
		command.Cmd = cmd
		command.Arg1 = arg1
		command.Args = args
	default:
		command.release(c)
		glog.Error("Invalid argNum:", argNum)
		return nil
	}
	return
}

func (c *RedisCon) sendToCmdChanArgsKey(ch chan *Result, cmd *string, key interface{}, args *[]interface{}) error {
	return c.sendToCmdChan(ch, cmd, ARGSKEY, key, nil, nil, args)
}

func (c *RedisCon) sendToCmdChanArgsNum0(ch chan *Result, cmd *string) error {
	return c.sendToCmdChan(ch, cmd, ARGSNUM0, nil, nil, nil, nil)
}

func (c *RedisCon) sendToCmdChanArgsNum1(ch chan *Result, cmd *string, arg interface{}) error {
	return c.sendToCmdChan(ch, cmd, ARGSNUM1, arg, nil, nil, nil)
}

func (c *RedisCon) sendToCmdChanArgsNum2(ch chan *Result, cmd *string, arg1 interface{}, arg2 interface{}) error {
	return c.sendToCmdChan(ch, cmd, ARGSNUM2, arg1, arg2, nil, nil)
}

func (c *RedisCon) sendToCmdChanArgsNum3(ch chan *Result, cmd *string, arg1 interface{}, arg2 interface{}, arg3 interface{}) error {
	return c.sendToCmdChan(ch, cmd, ARGSNUM3, arg1, arg2, arg3, nil)
}

func (c *RedisCon) sendToCmdChan(ch chan *Result, cmd *string, argNum int, arg1 interface{}, arg2 interface{}, arg3 interface{}, args *[]interface{}) (err error) {
	var sCmd *Command
	if cmd == nil {
		sCmd = new(Command)
		sCmd.ResultChan = ch
		c.closeUserChan(sCmd)
		return ErrInvalidArgNum
	}

	defer func() {
		if r := recover(); r != nil { //处理cmdChan写异常
			glog.Error(*cmd, ",", r)
			if sCmd == nil {
				sCmd = new(Command)
			}
			sCmd.ResultChan = ch
			c.closeUserChan(sCmd)
		}
	}()
	if *cmd != "EVALSHA" && *cmd != "EVAL" {
		switch argNum {
		case ARGSNUMARRAY:
			if args == nil || len(*args) < 1 {
				panic(ErrInvalidArgNum)
			}
			switch arg := (*args)[0].(type) {
			case string:
				if len(arg) > MAX_KEY_LEN {
					glog.Error("[redis] 传入的key长度过长:", arg)
					//					err = errors.New("key的长度过长")
					//					panic("key的长度过长")
				}
			}
		case ARGSNUM0:
			//			panic("参数个数不对")
		case ARGSNUM1, ARGSKEY:
			switch arg := arg1.(type) {
			case string:
				if len(arg) > MAX_KEY_LEN {
					glog.Error("[redis] 传入的key长度过长:", arg)
					//					err = errors.New("key的长度过长")
					//					panic("key的长度过长")
				}
			}
		case ARGSNUM2:
			switch arg := arg1.(type) {
			case string:
				if len(arg) > MAX_KEY_LEN {
					glog.Error("[redis] 传入的key长度过长:", arg)
					//					err = errors.New("key的长度过长")
					//					panic("key的长度过长")
				}
			}
		case ARGSNUM3:
			switch arg := arg1.(type) {
			case string:
				if len(arg) > MAX_KEY_LEN {
					glog.Error("[redis] 传入的key长度过长:", arg)
					//					err = errors.New("key的长度过长")
					//					panic("key的长度过长")
				}
			}
		}
	}

	sCmd = c.packageRedisCmd(cmd, argNum, arg1, arg2, arg3, args)
	if sCmd == nil {
		return ErrInvalidArgNum
	}
	sCmd.ResultChan = ch
	atomic.AddInt64(&c.waitProcessCmdNum, 1)
	c.cmdChan <- sCmd
	return nil
}

//TODO 可以增加超时机制
func (c *RedisCon) SendToCmdChan(ch chan *Result, cmd *string, args *[]interface{}) (err error) {
	return c.sendToCmdChan(ch, cmd, ARGSNUMARRAY, nil, nil, nil, args)
}

func (c *RedisCon) sendToCmdsChan(cmds []*Command) {
	c.cmdsChan <- cmds
}

func (c *RedisCon) sendToRedis(cmd *Command) (err error) {
	defer func() {
		if r := recover(); r != nil || err != nil {
			c.closeUserChan(cmd)
			c.netClose()
			err = ErrRedisNetFatal
		}
	}()
	err = c.writeCommand(cmd.CmdNum, cmd.Cmd, cmd.Arg1, cmd.Arg2, cmd.Arg3, cmd.Args)
	if err == nil {
		c.waitChan <- cmd
	} else {
		glog.Error(err)
		c.netClose()
	}
	return
}

func (c *RedisCon) Flush() (err error) {
	defer func() {
		if r := recover(); r != nil {
			glog.Error(r)
			err = ErrRedisNetFatal
			c.netClose()
		}
	}()
	err = c.bw.Flush()
	if err != nil {
		c.netClose()
	}
	return
}

func (c *RedisCon) sendResultToUser(result *Result, cmd *Command) {
	defer func() {
		cmd.release(c)
		atomic.AddInt64(&c.waitProcessCmdNum, -1)
		if r := recover(); r != nil {
			glog.Error(r)
		}
	}()
	if cmd != nil && cmd.ResultChan != nil {
		cmd.ResultChan <- result
	}
}

func (c *RedisCon) closeUserChan(p *Command) {
	result := &Result{
		Data: nil,
		Err:  ErrRedisConClose,
	}
	c.sendResultToUser(result, p)
}

//只有向Redis发送命令的协程调用这个
func (c *RedisCon) fatal(err error) error {
	c.err = err
	return err
}

func (c *RedisCon) resetNet() error {
	if len(c.server) == 0 {
		glog.Error("[Redis] 获取不到redis连接地址")
		return errors.New("获取不到redis连接地址")
	}
	//	c.serverIdx += 1
	//	if c.serverIdx >= len(c.server) {
	//		c.serverIdx = 0
	//	}
	glog.Error("[Redis] Begin-to-reset-Net ", c.server)

	if c.addressFunc != nil {
		c.server = c.addressFunc()
	}

	c.conRetriedTimes += 1
netconnect:
	netDialer := net.Dialer{Timeout: 5 * time.Second, KeepAlive: time.Minute}
	netConn, err := netDialer.Dial("tcp", c.server)
	if err != nil {
		glog.Error("[Redis] Dial failed", c.server)
		if c.addressFunc != nil {
			c.server = c.addressFunc()
			time.Sleep(time.Second * 2)
			goto netconnect
		}
		return err
	}

	c.br = bufio.NewReader(netConn)
	c.bw = bufio.NewWriter(netConn)
	c.waitChan = make(chan *Command, c.waitChanCap) //此句一定要放在c.NetStatus = NetOpen之前
	c.NetStatus = NetOpen
	//c.LifeCycle = RedisConAlive
	c.conRetriedTimes = 0
	return nil
}

//强行关闭和redis的网络链接，并置标志位
func (c *RedisCon) Close() {
	for {
		if c.changeLifeCycle(RedisConDead) == RedisConDead {
			break
		}
	}
	for {
		if c.netClose() {
			break
		}
	}

}

//置标志位
func (c *RedisCon) CloseFriendly() {
	for {
		if c.changeLifeCycle(RedisClosedFriendly) == RedisClosedFriendly {
			break
		}
	}
}

func (c *RedisCon) IsNetClose() bool {
	return atomic.LoadInt32(&c.NetStatus) == NetClosed
}

func (c *RedisCon) netClose() bool {
	if atomic.CompareAndSwapInt32(&c.NetStatus, NetOpen, NetClosed) {
		err := c.conn.Close()
		if err != nil {
			glog.Error("[Redis] Close connction 2 redis failed,", c.server)
		}
	}
	c.SendExceptionSignal(NET_CLOSE_SIGNAL)
	return c.IsNetClose()
}

func (c *RedisCon) Receive(retChan *chan *Result) (interface{}, error) {
	return nil, nil
}

func (c *RedisCon) receive() (Data interface{}, Err error) {
	if Data, Err = c.readReply(); Err != nil {
		glog.Error("[Redis] Get-result-from-redis-failed:", Err, c.server)
		c.netClose()
		//panic("read result from redis failed")
		return
	}
	if e, ok := Data.(Error); ok {
		Err = e
	}

	return
}

func (c *RedisCon) writeLen(prefix byte, n int) error {
	c.lenScratch[len(c.lenScratch)-1] = '\n'
	c.lenScratch[len(c.lenScratch)-2] = '\r'
	i := len(c.lenScratch) - 3
	for {
		c.lenScratch[i] = byte('0' + n%10)
		i -= 1
		n = n / 10
		if n == 0 {
			break
		}
	}
	c.lenScratch[i] = prefix
	_, err := c.bw.Write(c.lenScratch[i:])
	return err
}

func (c *RedisCon) writeString(s *string) error {
	c.writeLen('$', len(*s))
	c.bw.WriteString(*s)
	_, err := c.bw.WriteString(STRLINEBREAK)
	return err
}

func (c *RedisCon) writeBytes(p []byte) error {
	c.writeLen('$', len(p))
	c.bw.Write(p)
	_, err := c.bw.WriteString(STRLINEBREAK)
	return err
}

func (c *RedisCon) writeInt64(n int64) error {
	return c.writeBytes(strconv.AppendInt(c.numScratch[:0], n, 10))
}

func (c *RedisCon) writeFloat64(n float64) error {
	return c.writeBytes(strconv.AppendFloat(c.numScratch[:0], n, 'g', -1, 64))
}

func (c *RedisCon) writeCommand(argNum int, cmd *string, arg1 interface{}, arg2 interface{}, arg3 interface{}, args *[]interface{}) (err error) {
	defer func() {
		if r := recover(); r != nil { //防止网络层抛异常
			err = ErrRedisNetFatal
		}
	}()

	if argNum == ARGSKEY {
		err = c.writeLen('*', 2+len(*args))
	} else if argNum == ARGSNUMARRAY {
		err = c.writeLen('*', 1+len(*args))
	} else {
		err = c.writeLen('*', 1+argNum)
	}
	if err != nil {
		return
	}
	err = c.writeString(cmd)
	if err != nil {
		return
	}
	switch argNum {
	case ARGSNUMARRAY:
		if args != nil {
			for _, arg := range *args {
				if err != nil {
					break
				}
				err = c.writeInterface(arg)
				if err != nil {
					return
				}
			}
		}
	case ARGSNUM0:
		return
	case ARGSNUM1:
		err = c.writeInterface(arg1)
	case ARGSNUM2:
		err = c.writeInterface(arg1)
		if err != nil {
			return
		}
		err = c.writeInterface(arg2)
	case ARGSNUM3:
		err = c.writeInterface(arg1)
		if err != nil {
			return
		}
		err = c.writeInterface(arg2)
		if err != nil {
			return
		}
		err = c.writeInterface(arg3)
	case ARGSKEY:
		err = c.writeInterface(arg1)
		if err != nil {
			return
		}
		if args != nil {
			for _, arg := range *args {
				if err != nil {
					break
				}
				err = c.writeInterface(arg)
				if err != nil {
					return
				}
			}
		}
	default:
		return ErrInvalidArgNum
	}
	return
}

func (c *RedisCon) writeInterface(arg interface{}) (err error) {
	switch arg := arg.(type) {
	case string:
		err = c.writeString(&arg)
	case []byte:
		err = c.writeBytes(arg)
	case int:
		err = c.writeInt64(int64(arg))
	case int64:
		err = c.writeInt64(arg)
	case float64:
		err = c.writeFloat64(arg)
	case bool:
		if arg {
			err = c.writeString(&STRTRUE)
		} else {
			err = c.writeString(&STRFALSE)
		}
	case nil:
		err = c.writeString(&STRNIL)
	default:
		var buf bytes.Buffer
		fmt.Fprint(&buf, arg)
		err = c.writeBytes(buf.Bytes())
	}
	return
}

func (c *RedisCon) init() (err error) {
	c.cmdChan = make(chan *Command, c.cmdChanCap)
	c.cmdsChan = make(chan []*Command, c.cmdChanCap)
	c.waitChan = make(chan *Command, c.waitChanCap)
	c.NetStatus = NetOpen
	c.LifeCycle = RedisConAlive
	c.cmdPool, err = NewCmdPool(uint32(c.waitChanCap + c.cmdChanCap))
	return
}

func conRedis(server string) (*RedisCon, error) {
	netDialer := net.Dialer{Timeout: 5 * time.Second}
	netDialer.KeepAlive = time.Minute
	netConn, err := netDialer.Dial("tcp", server)
	if err != nil {
		glog.Error("[redis] Dial-failed")
		return nil, err
	}

	return &RedisCon{
		conn:    netConn,
		br:      bufio.NewReader(netConn),
		bw:      bufio.NewWriter(netConn),
		warnTT:  Default_Warn_TT,
		reConTT: Default_Recon_TT,
	}, nil
}

type protocolError string

func (pe protocolError) Error() string {
	return fmt.Sprintf("dbserver redis: %s (possible server error or unsupported concurrent read by application)", string(pe))
}

func (c *RedisCon) readLine() ([]byte, error) {
	p, err := c.br.ReadSlice('\n')
	if err == bufio.ErrBufferFull {
		return nil, protocolError("long response line")
	}
	if err != nil {
		return nil, err
	}
	i := len(p) - 2
	if i < 0 || p[i] != '\r' {
		return nil, protocolError("bad response line terminator")
	}
	return p[:i], nil
}

// parseLen parses bulk string and array lengths.
func parseLen(p []byte) (int, error) {
	if len(p) == 0 {
		return -1, protocolError("malformed length")
	}

	if p[0] == '-' && len(p) == 2 && p[1] == '1' {
		// handle $-1 and $-1 null replies.
		return -1, nil
	}

	var n int
	for _, b := range p {
		n *= 10
		if b < '0' || b > '9' {
			return -1, protocolError("illegal bytes in length")
		}
		n += int(b - '0')
	}

	return n, nil
}

// parseInt parses an integer reply.
func parseInt(p []byte) (int64, error) {
	if len(p) == 0 {
		return 0, protocolError("malformed integer")
	}

	var negate bool
	if p[0] == '-' {
		negate = true
		p = p[1:]
		if len(p) == 0 {
			return 0, protocolError("malformed integer")
		}
	}

	var n int64
	for _, b := range p {
		n *= 10
		if b < '0' || b > '9' {
			return 0, protocolError("illegal bytes in length")
		}
		n += int64(b - '0')
	}

	if negate {
		n = -n
	}
	return n, nil
}

var (
	okReply   interface{} = "OK"
	pongReply interface{} = "PONG"
)

func (c *RedisCon) readReply() (r interface{}, err error) {
	defer func() {
		if r := recover(); r != nil {
			glog.Error("[Redis] ", r)
			err = ErrRedisNetFatal
		}
	}()
	line, err := c.readLine()
	if err != nil {
		return nil, err
	}
	if len(line) == 0 {
		return nil, protocolError("short response line")
	}
	switch line[0] {
	case '+':
		switch {
		case len(line) == 3 && line[1] == 'O' && line[2] == 'K':
			// Avoid allocation for frequent "+OK" response.
			return okReply, nil
		case len(line) == 5 && line[1] == 'P' && line[2] == 'O' && line[3] == 'N' && line[4] == 'G':
			// Avoid allocation in PING command benchmarks :)
			return pongReply, nil
		default:
			return string(line[1:]), nil
		}
	case '-':
		return Error(string(line[1:])), nil
	case ':':
		return parseInt(line[1:])
	case '$':
		n, err := parseLen(line[1:])
		if n < 0 || err != nil {
			return nil, err
		}
		p := make([]byte, n)
		_, err = io.ReadFull(c.br, p)
		if err != nil {
			return nil, err
		}
		if line, err := c.readLine(); err != nil {
			return nil, err
		} else if len(line) != 0 {
			return nil, protocolError("bad bulk string format")
		}
		return p, nil
	case '*':
		n, err := parseLen(line[1:])
		if n < 0 || err != nil {
			return nil, err
		}
		r := make([]interface{}, n)
		for i := range r {
			r[i], err = c.readReply()
			if err != nil {
				return nil, err
			}
		}
		return r, nil
	}
	return nil, protocolError("unexpected response line")
}
