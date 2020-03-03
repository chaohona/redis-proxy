package gonet

import (
	"encoding/binary"
	"errors"
	"io"
	"lib/glog"
	"net"
	"runtime/debug"
	"sync"
	"sync/atomic"
	"time"
)

var sessionClosedError = errors.New("session is closed")

type ITcpTask interface {
	ParseMsg(data []byte) bool
	OnClose()
}

const (
	recvbuff_max_size = 1 * 1024 * 1024
	cmd_max_size      = 256 * 1024
	sendcmd_max_size  = 32 * 1024
	cmd_header_size   = 4 // 3字节指令长度 1字节是否压缩
	cmd_verify_time   = 30
)

type TcpTask struct {
	closed          int32
	verified        uint32
	stopedChan      chan struct{}
	recvBuff        *ByteBuffer
	sendBuff        *ByteBuffer
	sendMutex       sync.Mutex
	Conn            net.Conn
	derived         ITcpTask
	signal          chan struct{}
	needCheckrecv   int32
	recvTimeout     int64
	keepAliveSecond int64
	Index           int64
}

func NewTcpTask(conn net.Conn, derived ITcpTask) *TcpTask {
	index := time.Now().Unix()
	return &TcpTask{
		closed:     -1,
		Conn:       conn,
		stopedChan: make(chan struct{}, 1),
		recvBuff:   NewByteBuffer(),
		sendBuff:   NewByteBuffer(),
		signal:     make(chan struct{}, 1),
		derived:    derived,
		Index:      index,
	}
}

func NewTcpTaskTimeout(conn net.Conn, derived ITcpTask, timeout int64) *TcpTask {
	return &TcpTask{
		closed:          -1,
		Conn:            conn,
		stopedChan:      make(chan struct{}, 1),
		recvBuff:        NewByteBuffer(),
		sendBuff:        NewByteBuffer(),
		signal:          make(chan struct{}, 1),
		derived:         derived,
		Index:           time.Now().Unix(),
		keepAliveSecond: timeout,
	}
}

func (this *TcpTask) Signal() {
	select {
	case this.signal <- struct{}{}:
	default:
	}
}

func (this *TcpTask) RemoteAddr() string {
	if this.Conn == nil {
		return ""
	}
	return this.Conn.RemoteAddr().String()
}

func (this *TcpTask) LocalAddr() string {
	if this.Conn == nil {
		return ""
	}
	return this.Conn.LocalAddr().String()
}

func (this *TcpTask) Stop() bool {
	if this.IsClosed() {
		glog.Info("[连接] 关闭失败 ", this.RemoteAddr())
		return false
	}
	select {
	case this.stopedChan <- struct{}{}:
	default:
		glog.Info("[连接] 关闭失败 ", this.RemoteAddr())
		return false
	}
	return true
}

func (this *TcpTask) Start() {
	if !atomic.CompareAndSwapInt32(&this.closed, -1, 0) {
		return
	}
	job := &sync.WaitGroup{}
	job.Add(1)
	go this.sendloop(job)
	go this.recvloop()
	job.Wait()
	glog.Info("[连接] 收到连接 ", this.RemoteAddr(), ",", this.Index)
}

func (this *TcpTask) Close() {
	if !atomic.CompareAndSwapInt32(&this.closed, 0, -1) {
		return
	}
	glog.Info("[连接] 断开连接 ", this.RemoteAddr())
	this.Conn.Close()
	this.recvBuff.Reset()
	this.sendBuff.Reset()
	close(this.stopedChan)
	this.derived.OnClose()
}

func (this *TcpTask) IsClosed() bool {
	return atomic.LoadInt32(&this.closed) != 0
}

func (this *TcpTask) Verify() {
	atomic.StoreUint32(&this.verified, 1)
}

func (this *TcpTask) IsVerified() bool {
	return atomic.LoadUint32(&this.verified) == 1
}

func (this *TcpTask) Terminate() {
	this.Close()
}

func (this *TcpTask) RouteRpcBuffToService(flag uint8, innerId uint64, uid int64, buffer []byte) bool {
	if this.IsClosed() {
		return false
	}
	bsize := len(buffer) + 8 + 8 + 1 // innerid 大小(8) + flag(1)
	this.sendMutex.Lock()
	this.sendBuff.Append(byte(bsize), byte(bsize>>8), byte(bsize>>16), byte(bsize>>24), flag)
	this.sendBuff.AppendUint64(innerId)
	this.sendBuff.AppendUint64(uint64(uid))
	this.sendBuff.Append(buffer...)
	this.sendMutex.Unlock()
	this.Signal()
	return true
}

func (this *TcpTask) SendInnerMsgToClient(flag, mainCmd byte, subCmd uint16, rpcId uint32, buffer []byte) bool {
	if this.IsClosed() {
		return false
	}
	bsize := len(buffer) + 8
	this.sendMutex.Lock()
	this.sendBuff.Append(byte(bsize), byte(bsize>>8), byte(bsize>>16), byte(bsize>>24), flag, mainCmd)
	this.sendBuff.AppendUint16(subCmd)
	this.sendBuff.AppendUint32(rpcId)
	this.sendBuff.Append(buffer...)
	this.sendMutex.Unlock()
	this.Signal()
	return true
}

func (this *TcpTask) AsyncRawSend(buffer []byte) bool {
	if this.IsClosed() {
		return false
	}
	this.sendMutex.Lock()
	this.sendBuff.Append(buffer...)
	this.sendMutex.Unlock()
	this.Signal()
	return true
}

func (this *TcpTask) AsyncSend(buffer []byte) bool {
	if this.IsClosed() {
		return false
	}
	bsize := len(buffer)
	this.sendMutex.Lock()
	this.sendBuff.Append(byte(bsize), byte(bsize>>8), byte(bsize>>16), byte(bsize>>24))
	this.sendBuff.Append(buffer...)
	this.sendMutex.Unlock()
	this.Signal()
	return true
}

func (this *TcpTask) AsyncSendWithHead(head []byte, buffer []byte) bool {
	if this.IsClosed() {
		return false
	}
	bsize := len(buffer) + len(head) + cmd_header_size
	this.sendMutex.Lock()
	this.sendBuff.Append(byte(bsize), byte(bsize>>8), byte(bsize>>16), byte(bsize>>24))
	this.sendBuff.Append(head...)
	this.sendBuff.Append(buffer...)
	this.sendMutex.Unlock()
	this.Signal()
	return true
}

func (this *TcpTask) readAtLeast(buff *ByteBuffer, neednum int) error {
	buff.WrGrow(neednum)
	n, err := io.ReadAtLeast(this.Conn, buff.WrBuf(), neednum)
	buff.WrFlip(n)
	return err
}

func (this *TcpTask) recvloop() {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", this.RemoteAddr(), ",", err, "\n", string(debug.Stack()))
		}
	}()
	defer this.Close()

	var (
		neednum   int
		err       error
		totalsize int
		datasize  int
		msgbuff   []byte
	)

	for {
		totalsize = this.recvBuff.RdSize()

		if totalsize < cmd_header_size {
			neednum = cmd_header_size - totalsize
			err = this.readAtLeast(this.recvBuff, neednum)
			if err != nil {
				if !this.IsClosed() {
					glog.Error("[连接] 接收失败 ", this.RemoteAddr(), ",", err)
				}
				return
			}
			totalsize = this.recvBuff.RdSize()
		}

		msgbuff = this.recvBuff.RdBuf()

		datasize = int(binary.LittleEndian.Uint32(msgbuff))
		if datasize <= 0 || datasize > cmd_max_size {
			glog.Error("[连接] 数据超过最大值 ", this.RemoteAddr(), ",", datasize)
			return
		}

		if totalsize < datasize+cmd_header_size {
			neednum = datasize + cmd_header_size - totalsize
			err = this.readAtLeast(this.recvBuff, neednum)
			if err != nil {
				glog.Error("[连接] 接收失败 ", this.RemoteAddr(), ",", err)
				return
			}
			msgbuff = this.recvBuff.RdBuf()
		}

		this.derived.ParseMsg(msgbuff[cmd_header_size : cmd_header_size+datasize])
		this.recvBuff.RdFlip(datasize + cmd_header_size)
	}
}

func (this *TcpTask) sendloop(job *sync.WaitGroup) {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	defer this.Close()

	var (
		tmpByte  = NewByteBuffer()
		timeout  *time.Timer
		writenum int
		err      error
	)
	if this.keepAliveSecond != 0 {
		timeout = time.NewTimer(time.Second * time.Duration(this.keepAliveSecond))
	} else {
		timeout = time.NewTimer(time.Second * cmd_verify_time)
	}

	defer timeout.Stop()

	job.Done()

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
			return
		case <-timeout.C:
			if !this.IsVerified() {
				glog.Error("[连接] 验证超时 ", this.RemoteAddr(), ",", this.Index)
				return
			}
		}
	}
}
