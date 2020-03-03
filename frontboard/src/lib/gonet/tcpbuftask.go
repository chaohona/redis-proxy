package gonet

import (
	"io"
	"lib/glog"
	"net"
	"runtime/debug"
	"sync"
	"sync/atomic"
	"time"
)

type ITcpBufTask interface {
	ParseMsg(data []byte, flag byte) bool
	GetEndIdx() int64 // 获取发送缓冲区中数据结束下标
	OnClose()
}

const (
	//cmd_max_size     = 128 * 1024
	//sendcmd_max_size = 64 * 1024
	//cmd_header_size  = 4 // 3字节指令长度 1字节是否压缩
	//cmd_verify_time  = 30

	interrupt_chan_len = 10
	immediat_chan_len  = 24
)

type SendBuffInfo struct {
	sendBuff    *[]byte // 发送缓冲区指针
	buffLen     int64   // 发送缓冲区大小
	startIdx    int64   // 起始发送下标
	preSendBuff []byte  // 需要提前发出去的消息
}

type TcpBufTask struct {
	closed            int32
	stopped           int32
	stoppedBuff       int32
	verified          bool
	stopedChan        chan bool
	stopedBuffChan    chan bool
	recvBuff          *ByteBuffer
	sendBuff          *ByteBuffer
	sendMutex         sync.Mutex
	Conn              net.Conn
	Derived           ITcpBufTask
	signal            chan int
	newBuffSignal     chan int
	interruptBuffChan chan []byte        // 在正常消息流中间穿插的需要发送的buff
	immediatBuffChan  chan []byte        // 需要立即发送的buff
	initSendBuff      chan *SendBuffInfo // 初始化发送缓冲区
	startIdx          int64              // 起始消息流下标
	endIdx            int64              // 结束消息流下标
	sendBuffPtr       *[]byte            // 发送缓冲区指针
	buffLen           int64              // 发送缓冲区大小
	buffValid         bool               // 是否已经初始化发送缓冲区

	interruptBuff *ByteBuffer // 穿插发送的消息缓冲
}

func NewTcpBufTask(conn net.Conn) *TcpBufTask {
	return &TcpBufTask{
		closed:            -1,
		stopped:           -1,
		stoppedBuff:       -1,
		verified:          false,
		Conn:              conn,
		stopedChan:        make(chan bool, 1),
		stopedBuffChan:    make(chan bool, 1),
		recvBuff:          NewByteBuffer(),
		sendBuff:          NewByteBuffer(),
		signal:            make(chan int, 1),
		newBuffSignal:     make(chan int, 1),
		initSendBuff:      make(chan *SendBuffInfo, 1),
		interruptBuffChan: make(chan []byte, interrupt_chan_len),
		immediatBuffChan:  make(chan []byte, immediat_chan_len),
		interruptBuff:     NewByteBuffer(), //穿插的消息流，每次不能超过4K
	}
}

func (this *TcpBufTask) SetSendBuffWithFlag(SendBuff *[]byte, buffLen, startIdx int64, preSendBuff []byte, flag byte) {
	if this.IsClosed() {
		return
	}
	tmpBuff := this.getsendbuff(flag, preSendBuff)
	this.initSendBuff <- &SendBuffInfo{
		sendBuff:    SendBuff,
		buffLen:     buffLen,
		startIdx:    startIdx,
		preSendBuff: tmpBuff,
	}
}

func (this *TcpBufTask) SetSendBuff(SendBuff *[]byte, buffLen, startIdx int64, preSendBuff []byte) {
	if this.IsClosed() {
		return
	}
	this.initSendBuff <- &SendBuffInfo{
		sendBuff:    SendBuff,
		buffLen:     buffLen,
		startIdx:    startIdx,
		preSendBuff: preSendBuff,
	}
}

func (this *TcpBufTask) Signal() {
	select {
	case this.signal <- 1:
	default:
	}
}

func (this *TcpBufTask) stopchan() {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	if this.IsStopped() {
		//glog.Info("[连接] 关闭失败 ", this.RemoteAddr())
		return
	}
	if atomic.CompareAndSwapInt32(&this.stopped, 0, 1) {
		select {
		case this.stopedChan <- true:
		default:
			glog.Info("[连接] 关闭失败 ", this.RemoteAddr())
		}
	}
	return
}

func (this *TcpBufTask) RemoteAddr() string {
	if this.Conn == nil {
		return ""
	}
	return this.Conn.RemoteAddr().String()
}

func (this *TcpBufTask) stopbuff() {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	if this.IsStoppedBuff() {
		//glog.Info("[连接] 关闭失败 ", this.RemoteAddr())
		return
	}

	if atomic.CompareAndSwapInt32(&this.stoppedBuff, 0, 1) {
		select {
		case this.stopedBuffChan <- true:
		default:
			glog.Info("[连接] 关闭失败 ", this.RemoteAddr())
		}
	}

	return
}

func (this *TcpBufTask) Stop() bool {
	this.stopchan()
	this.stopbuff()
	return true
}

func (this *TcpBufTask) Start(PoolSend bool) {
	if atomic.CompareAndSwapInt32(&this.closed, -1, 0) {
		atomic.StoreInt32(&this.stopped, 0)
		atomic.StoreInt32(&this.stoppedBuff, 0)
		glog.Info("[连接] 收到连接 ", this.RemoteAddr())
		go this.recvloop()
		if !PoolSend {
			go this.sendloop()
		} else {
			go this.poolsendloop()
		}
	}
}

func (this *TcpBufTask) Close() {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	if atomic.CompareAndSwapInt32(&this.closed, 0, 1) {
		this.Stop()
		atomic.StoreInt32(&this.stopped, 1)
		atomic.StoreInt32(&this.stoppedBuff, 1)
		glog.Info("[连接] 断开连接 ", this.RemoteAddr())
		this.sendBuffPtr = nil
		this.interruptBuff = nil
		this.Conn.Close()
		close(this.interruptBuffChan)
		close(this.immediatBuffChan)
		close(this.initSendBuff)
		this.Derived.OnClose()
	}
}

func (this *TcpBufTask) Reset() bool {
	if atomic.LoadInt32(&this.closed) != 1 {
		return false
	}
	if !this.IsVerified() {
		return false
	}
	this.closed = -1
	this.stopped = -1
	this.stoppedBuff = -1
	this.verified = false
	this.stopedChan = make(chan bool)
	this.stopedBuffChan = make(chan bool)
	glog.Info("[连接] 重置连接 ", this.RemoteAddr())
	return true
}

func (this *TcpBufTask) IsStoppedBuff() bool {
	return atomic.LoadInt32(&this.stoppedBuff) != 0
}

func (this *TcpBufTask) IsStopped() bool {
	return atomic.LoadInt32(&this.stopped) != 0
}

func (this *TcpBufTask) IsClosed() bool {
	return atomic.LoadInt32(&this.closed) != 0
}

func (this *TcpBufTask) Verify() {
	this.verified = true
}

func (this *TcpBufTask) IsVerified() bool {
	return this.verified
}

func (this *TcpBufTask) Terminate() {
	this.Close()
}

func (this *TcpBufTask) AsyncSend(buffer []byte, flag byte) bool {
	if this.IsClosed() {
		return false
	}
	bsize := len(buffer)
	this.sendMutex.Lock()
	this.sendBuff.Append(byte(bsize), byte(bsize>>8), byte(bsize>>16), flag)
	this.sendBuff.Append(buffer...)
	this.sendMutex.Unlock()
	this.Signal()
	return true
}

func (this *TcpBufTask) AsyncSendWithHead(head []byte, buffer []byte, flag byte) bool {
	if this.IsClosed() {
		return false
	}
	bsize := len(buffer) + len(head)
	this.sendMutex.Lock()
	this.sendBuff.Append(byte(bsize), byte(bsize>>8), byte(bsize>>16), flag)
	this.sendBuff.Append(head...)
	this.sendBuff.Append(buffer...)
	this.sendMutex.Unlock()
	this.Signal()
	return true
}

func (this *TcpBufTask) recvloop() {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	defer this.Close()

	var (
		neednum   int
		readnum   int
		err       error
		totalsize int
		datasize  int
		msgbuff   []byte
	)

	for {
		totalsize = this.recvBuff.RdSize()

		if totalsize < cmd_header_size {

			neednum = cmd_header_size - totalsize
			if this.recvBuff.WrSize() < neednum {
				this.recvBuff.WrGrow(neednum)
			}

			readnum, err = io.ReadAtLeast(this.Conn, this.recvBuff.WrBuf(), neednum)
			if err != nil {
				//glog.Error("[连接] 接收失败 ", this.Conn.RemoteAddr(), ",", err)
				return
			}

			this.recvBuff.WrFlip(readnum)
			totalsize = this.recvBuff.RdSize()
		}

		msgbuff = this.recvBuff.RdBuf()

		datasize = int(msgbuff[0]) | int(msgbuff[1])<<8 | int(msgbuff[2])<<16
		if datasize > cmd_max_size {
			glog.Error("[连接] 数据超过最大值 ", this.RemoteAddr(), ",", datasize)
			return
		}

		if totalsize < cmd_header_size+datasize {

			neednum = cmd_header_size + datasize - totalsize
			if this.recvBuff.WrSize() < neednum {
				this.recvBuff.WrGrow(neednum)
			}

			readnum, err = io.ReadAtLeast(this.Conn, this.recvBuff.WrBuf(), neednum)
			if err != nil {
				glog.Error("[连接] 接收失败 ", this.RemoteAddr(), ",", err)
				return
			}

			this.recvBuff.WrFlip(readnum)
			msgbuff = this.recvBuff.RdBuf()
		}

		this.Derived.ParseMsg(msgbuff[cmd_header_size:cmd_header_size+datasize], msgbuff[3])
		this.recvBuff.RdFlip(cmd_header_size + datasize)
	}
}

func (this *TcpBufTask) SyncSendBuff(buff []byte, flag byte) bool {
	var start int = 0
	for {
		writenum, err := this.Conn.Write(buff[start:])
		if err != nil {
			glog.Error("[连接] 发送失败 ", this.RemoteAddr(), ",", err)
			return false
		}
		start += writenum
	}
	return true
}

func (this *TcpBufTask) getsendbuff(flag byte, buff []byte) []byte {
	var tmpBuff []byte
	bsize := len(buff)
	tmpBuff = make([]byte, bsize+4)
	tmpBuff[0] = byte(bsize)
	tmpBuff[1] = byte(bsize >> 8)
	tmpBuff[2] = byte(bsize >> 16)
	tmpBuff[3] = byte(flag)
	copy(tmpBuff[4:], buff)
	return tmpBuff
}

func (this *TcpBufTask) SendImmediateBuffSafe(buff []byte, flag byte) bool {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] SendImmediateBuffSafe ", err, "\n", string(debug.Stack()))
			this.Close()
		}
	}()
	tmpBuff := this.getsendbuff(flag, buff)
	if this.IsClosed() {
		return false
	}
	this.immediatBuffChan <- tmpBuff
	return true
}

func (this *TcpBufTask) SendImmediateBuff(buff []byte, flag byte) bool {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] SendImmediateBuff ", err, "\n", string(debug.Stack()))
			this.Close()
		}
	}()
	tmpBuff := this.getsendbuff(flag, buff)
	if this.IsClosed() {
		return false
	}
	select {
	case this.immediatBuffChan <- tmpBuff:
	default:
		glog.Error("[连接] 网络繁忙, immediatBuffChan已满 ", this.RemoteAddr())
	}
	return true
}

func (this *TcpBufTask) AsyncSendBuffSafe(buff []byte, flag byte) bool {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] AsyncSendBuff ", err, "\n", string(debug.Stack()))
			this.Close()
		}
	}()
	tmpBuff := this.getsendbuff(flag, buff)
	if this.IsClosed() {
		return false
	}
	this.interruptBuffChan <- tmpBuff
	return true
}

func (this *TcpBufTask) AsyncSendBuff(buff []byte, flag byte) bool {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] AsyncSendBuff ", err, "\n", string(debug.Stack()))
			this.Close()
		}
	}()
	tmpBuff := this.getsendbuff(flag, buff)
	if this.IsClosed() {
		return false
	}
	select {
	case this.interruptBuffChan <- tmpBuff:
	default:
		glog.Error("[连接] 网络繁忙, interruptBuff已满 ", this.RemoteAddr())
	}
	return true
}

func (this *TcpBufTask) NewBuffNotice() {
	if this.IsClosed() {
		return
	}
	select {
	case this.newBuffSignal <- 1:
	default:
		return
	}
}

func (this *TcpBufTask) poolsendloop() {
	defer func() {
		atomic.StoreInt32(&this.stoppedBuff, 1)
		close(this.stopedBuffChan)
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	defer this.Close()
	defer this.stopchan()

	var (
		timeout = time.NewTimer(time.Second * cmd_verify_time)
	)

	defer timeout.Stop()

	for {
		select {
		case buff := <-this.immediatBuffChan:
			{
				l := len(buff)
				if l == 0 {
					continue
				}
				var start int
				for {
					writenum, err := this.Conn.Write(buff[start:l])
					if err != nil {
						glog.Error("[连接] 发送失败 ", this.RemoteAddr(), ",", err)
						return
					}
					start += writenum
					if start == l {
						break
					}
				}
			}
		case buff := <-this.interruptBuffChan: // 需要把正常消息发送完，然后再把buff发送完才能返回
			{
				if this.interruptBuff == nil {
					continue
				}
				this.interruptBuff.Append(buff...)
			}
		case <-this.newBuffSignal:
			if !this.buffValid {
				continue
			}
			this.endIdx = this.Derived.GetEndIdx()
			if this.sendBuffPtr == nil || this.endIdx-this.startIdx > int64(len(*this.sendBuffPtr)) { // 消息发送太慢了,已经出现环路了
				glog.Error("[连接] 客户端消息消耗太慢,已经出现回路了 ", this.RemoteAddr())
				return
			}

			for {
				this.endIdx = this.Derived.GetEndIdx()
				if this.startIdx == this.endIdx { // 没有需要发送的数据
					break
				}
				startIdx, endIdx := this.getBuffSection()
				if this.sendBuffPtr == nil {
					break
				}
				writenum, err := this.Conn.Write((*this.sendBuffPtr)[startIdx:endIdx])
				if err != nil {
					glog.Error("[连接] 发送失败 ", this.RemoteAddr(), ",", err)
					return
				}
				this.startIdx += int64(writenum)
			}
			for {
				if this.interruptBuff == nil || !this.interruptBuff.RdReady() {
					break
				}

				writenum, err := this.Conn.Write(this.interruptBuff.RdBuf()[:this.interruptBuff.RdSize()])
				if err != nil {
					glog.Error("[连接] 发送失败 ", this.RemoteAddr(), ",", err)
					return
				}
				this.interruptBuff.RdFlip(writenum)
			}
		case <-this.stopedBuffChan:
			return
		case <-timeout.C:
			if !this.IsVerified() {
				glog.Error("[连接] 验证超时 ", this.RemoteAddr())
				return
			}
		case sbi := <-this.initSendBuff:
			{
				if sbi == nil || sbi.buffLen == 0 || sbi.sendBuff == nil {
					// 如果连接关闭(调用Close函数)则initSendBuff会一并被关闭，此时获得的sbi就是nil
					// glog.Error("[连接] invlid sbi value ", this.RemoteAddr())
					return
				}
				if len(sbi.preSendBuff) > 0 {
					start := 0
					for {
						writenum, err := this.Conn.Write(sbi.preSendBuff[start:])
						if err != nil {
							glog.Error("[连接] 发送失败 ", this.RemoteAddr(), ",", err)
							return
						}
						start += writenum
						if start == len(sbi.preSendBuff) {
							break
						}
					}
				}
				this.startIdx = sbi.startIdx
				this.endIdx = sbi.startIdx
				this.buffLen = sbi.buffLen
				this.sendBuffPtr = sbi.sendBuff
				this.buffValid = true
			}
		}
	}
}

func (this *TcpBufTask) getBuffSection() (int, int) {
	if this.sendBuffPtr == nil {
		return 0, 0
	}
	startIdx := this.startIdx % this.buffLen
	endIdx := this.endIdx % this.buffLen
	if endIdx > startIdx {
		return int(startIdx), int(endIdx)
	}
	return int(startIdx), int(this.buffLen)
}

func (this *TcpBufTask) sendloop() {
	defer func() {
		atomic.StoreInt32(&this.stopped, 1)
		close(this.stopedChan)
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()

	var (
		tmpByte  = NewByteBuffer()
		timeout  = time.NewTimer(time.Second * cmd_verify_time)
		writenum int
		err      error
	)

	defer this.Close()
	defer this.stopbuff()

	defer timeout.Stop()

	for {
		select {
		case <-this.signal:
			for {
				this.sendMutex.Lock()
				if this.sendBuff.RdReady() {
					tmpByte.Append(this.sendBuff.RdBuf()[:this.sendBuff.RdSize()]...)
					this.sendBuff.Reset()
				}
				this.sendMutex.Unlock()

				if !tmpByte.RdReady() {
					break
				}

				writenum, err = this.Conn.Write(tmpByte.RdBuf()[:tmpByte.RdSize()])
				if err != nil {
					glog.Error("[连接] 发送失败 ", this.RemoteAddr(), ",", err)
					return
				}
				tmpByte.RdFlip(writenum)
			}
		case <-this.stopedChan:
			{
				return
			}
		case <-timeout.C:
			if !this.IsVerified() {
				glog.Error("[连接] 验证超时 ", this.RemoteAddr())
				return
			}
		}
	}
}
