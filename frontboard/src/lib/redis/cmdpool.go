package redis

import (
	"errors"
	"lib/glog"
	"sync/atomic"
)

const (
	CmdUsed = 0
	CmdFree = 1
)

var globalIndex uint64
var finishIndex uint64

type Command struct {
	Cmd    *string //"set"等redis命令
	CmdNum int     //参数个数-1为不定个数用Args
	Arg1   interface{}
	Arg2   interface{}
	Arg3   interface{}
	Args   *[]interface{} //Cmd命令的参数

	isUsed   int32
	bPoolCmd bool
	useChan  chan int

	ResultChan chan *Result //通过这个chan把结果传递出去
	cmdIndex   uint64
	recvTime   int64
}

func (c *Command) release(con *RedisCon) {
	if c.bPoolCmd && atomic.CompareAndSwapInt32(&c.isUsed, CmdUsed, CmdFree) {
		c.useChan <- 1
		atomic.StoreUint64(&con.recvIndex, c.cmdIndex)
	}
}

type cmdPool struct {
	size      uint32
	getIndex  uint32
	freeIndex uint32
	cmds      []*Command
}

func NewCmdPool(poolSize uint32) (_cmdPool *cmdPool, err error) {
	poolSize = poolSize * 10
	defer func() {
		if r := recover(); r != nil {
			glog.Error(r)
			err = errors.New("Make cmd pool failed")
		}
	}()
	_cmdPool = new(cmdPool)
	_cmdPool.size = poolSize
	_cmdPool.getIndex = 0
	_cmdPool.cmds = make([]*Command, poolSize)
	for i := 0; i < int(poolSize); i++ {
		_cmdPool.cmds[i] = new(Command)
		_cmdPool.cmds[i].useChan = make(chan int, 1)
		_cmdPool.cmds[i].useChan <- 1
		_cmdPool.cmds[i].bPoolCmd = true
		_cmdPool.cmds[i].isUsed = CmdFree
	}
	return
}

func (p *cmdPool) Get(con *RedisCon) *Command {
	tmp := atomic.AddUint32(&p.getIndex, 1)
	index := int(tmp % p.size)
	//for !atomic.CompareAndSwapUint32(&(p.cmds[index].isUsed), CmdFree, CmdUsed){
	//
	//}
	<-p.cmds[index].useChan
	p.cmds[index].isUsed = CmdUsed
	p.cmds[index].cmdIndex = atomic.AddUint64(&con.sendIndex, 1)
	return p.cmds[index]
}
