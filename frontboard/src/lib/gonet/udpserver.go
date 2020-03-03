package gonet

import (
	"errors"
	"lib/glog"
	"net"
	"runtime/debug"
	"strings"
	"sync"
)

const (
	maxUdpSize = 1024
)

type HandshakeFunc func(data []byte, listener *net.UDPConn, addr *net.UDPAddr) *UdpTask

type UdpServer struct {
	maxPacketSize int
	Handshake     HandshakeFunc
	listeners     []*net.UDPConn
}

func NewUdpServer(handler HandshakeFunc, maxPacketSize int) *UdpServer {
	return &UdpServer{
		Handshake:     handler,
		maxPacketSize: maxPacketSize,
	}
}

func (this *UdpServer) BindAccept(address string) error {
	addrs := strings.Split(address, "/")
	if len(addrs) == 0 {
		return errors.New("[UDP] listen address is nil")
	}
	for _, addr := range addrs {
		listener, err := udpBind(addr)
		if err != nil {
			return err
		}
		udpHandle(listener, this.maxPacketSize, this.Handshake)
		this.listeners = append(this.listeners, listener)
	}
	return nil
}

func (this *UdpServer) Close() {
	for _, listener := range this.listeners {
		listener.Close()
	}
}

func udpBind(address string) (listener *net.UDPConn, err error) {
	udpAddr, err := net.ResolveUDPAddr("udp4", address)
	if err != nil {
		glog.Error("[UDP] 解析失败 ", address, ",", err)
		return nil, err
	}

	listener, err = net.ListenUDP("udp4", udpAddr)
	if err != nil {
		glog.Error("[UDP] 解析失败 ", address)
		return
	}
	glog.Error("[UDP] 绑定成功 ", address)
	return
}

func udpHandle(listener *net.UDPConn, maxPacketSize int, handShake HandshakeFunc) {
	clients := make(map[string]*UdpTask)
	go func() {
		defer func() {
			if err := recover(); err != nil {
				glog.Error("[异常] ", err, "\n", string(debug.Stack()))
			}
		}()
		var (
			mutex sync.RWMutex
			b     = make([]byte, maxPacketSize)
			c     *UdpTask
			addr  *net.UDPAddr
			key   string
			n     int
			ok    bool
			err   error
		)
		for {
			n, addr, err = listener.ReadFromUDP(b[:])
			if err != nil || n >= maxPacketSize {
				continue
			}
			key = addr.String()

			mutex.RLock()
			c, ok = clients[key]
			mutex.RUnlock()
			if ok {
				c.onRecv(b[:n])
			} else if c = handShake(b[:n], listener, addr); c != nil {
				c.Key = key
				mutex.Lock()
				clients[key] = c
				mutex.Unlock()

				go c.recvloop()
				go func(task *UdpTask, udpaddr *net.UDPAddr) {
					task.sendloop(listener, udpaddr)
					mutex.Lock()
					delete(clients, task.Key)
					mutex.Unlock()
				}(c, addr)
			}
		}
	}()
}
