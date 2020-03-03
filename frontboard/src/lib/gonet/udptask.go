package gonet

import (
	"errors"
	"lib/glog"
	"net"
	"runtime/debug"
	"sync"
	"sync/atomic"
	"time"
)

var (
	ErrUdpClosed = errors.New("udp is closed")
)

type IUdpTask interface {
	ParseMsg(data []byte, msgNo int)
	OnClose()
}

type UdpTask struct {
	activeTime time.Duration
	Key        string
	closed     int32
	recvBuff   *Ring
	sendBuff   *Ring
	sendMutex  sync.Mutex
	Derived    IUdpTask
	sendSignal chan struct{}
	recvSignal chan struct{}
	sendClose  chan struct{}
	recvClose  chan struct{}
}

func NewUdpTask() *UdpTask {
	return &UdpTask{
		closed:     0,
		recvBuff:   NewRing(1024),
		sendBuff:   NewRing(1024),
		recvSignal: make(chan struct{}, 1),
		sendSignal: make(chan struct{}, 1),
		sendClose:  make(chan struct{}, 1),
		recvClose:  make(chan struct{}, 1),
	}
}

func (this *UdpTask) Close() {
	if atomic.CompareAndSwapInt32(&this.closed, 0, 1) {
		glog.Info("[连接] 断开连接 ", this.Key)
		select {
		case this.sendClose <- struct{}{}:
		default:
		}
		select {
		case this.recvClose <- struct{}{}:
		default:
		}
		close(this.sendClose)
		close(this.recvClose)
		close(this.sendSignal)
		close(this.recvSignal)
		this.Derived.OnClose()
	}
}

func (this *UdpTask) IsClosed() bool {
	return atomic.LoadInt32(&this.closed) != 0
}

func (this *UdpTask) onRecv(data []byte) error {
	if this.IsClosed() || len(data) < 3 {
		return ErrUdpClosed
	}
	buf := make([]byte, len(data)-3)
	copy(buf, data[3:])

	msg, err := this.recvBuff.Set()
	if err != nil {
		return err
	}
	msg.Body = buf
	msg.Index = int(data[0]) | int(data[1])<<8
	msg.MsgNo = int(data[2])
	this.recvBuff.SetAdv()
	// notice recv msg
	select {
	case this.recvSignal <- struct{}{}:
	default:
	}
	return nil
}

func (this *UdpTask) AsyncSend(buffer []byte, msgNo int) bool {
	if this.IsClosed() {
		return false
	}
	this.sendMutex.Lock()
	msg, err := this.sendBuff.Set()
	if err == nil {
		msg.MsgNo = msgNo
		msg.Body = buffer
		this.sendBuff.SetAdv()
	}
	this.sendMutex.Unlock()
	// notic send msg
	select {
	case this.sendSignal <- struct{}{}:
	default:
	}
	return true
}

func (this *UdpTask) recvloop() {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	defer this.Close()

	var (
		err    error
		buf    *UdpMsg
		curIdx int
	)

	for {
		if this.IsClosed() {
			return
		}
		select {
		case <-this.recvSignal:
			if this.IsClosed() {
				return
			}
			for {
				buf, err = this.recvBuff.Get()
				if err != nil {
					break
				}
				this.recvBuff.GetAdv()

				if this.Derived != nil {
					if buf.Index <= curIdx {
						continue
					}
					curIdx = buf.Index
					this.Derived.ParseMsg(buf.Body, buf.MsgNo)
				}
				buf.Body = nil
			}
		case <-this.recvClose:
			return
		}
	}
}

func (this *UdpTask) sendloop(conn *net.UDPConn, address *net.UDPAddr) {
	defer func() {
		if err := recover(); err != nil {
			glog.Error("[异常] ", err, "\n", string(debug.Stack()))
		}
	}()
	defer this.Close()

	var (
		err   error
		buf   *UdpMsg
		index int
	)

	for {
		if this.IsClosed() {
			return
		}
		select {
		case <-this.sendSignal:
			if this.IsClosed() {
				return
			}
			for {
				buf, err = this.sendBuff.Get()
				if err != nil {
					break
				}
				this.sendBuff.GetAdv()
				index = (index + 1) & 0xffff
				buf.Body[0] = byte(index & 0xff)
				buf.Body[1] = byte((index >> 8) & 0xff)
				buf.Body[2] = byte(buf.MsgNo)
				_, err = conn.WriteToUDP(buf.Body, address)
				if err != nil {
					glog.Error("[连接] 发送失败 ", address.String(), ",", err)
					break
				}
			}
		case <-this.sendClose:
			return
		}
	}
}
