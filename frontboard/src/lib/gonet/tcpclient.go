package gonet

import (
	"lib/glog"
	"net"
	"time"
)

func TcpDial(address string) (*net.TCPConn, error) {
	tcpAddr, err := net.ResolveTCPAddr("tcp4", address)
	if err != nil {
		glog.Error("[连接] 解析失败 ", address)
		return nil, err
	}

	conn, err := net.DialTCP("tcp", nil, tcpAddr)
	if err != nil {
		glog.Error("[连接] 连接失败 ", address)
		return nil, err
	}

	conn.SetKeepAlive(true)
	conn.SetKeepAlivePeriod(1 * time.Minute)
	conn.SetNoDelay(true)
	conn.SetWriteBuffer(128 * 1024)
	conn.SetReadBuffer(128 * 1024)

	glog.Info("[连接] 连接成功 ", address)
	return conn, nil
}
