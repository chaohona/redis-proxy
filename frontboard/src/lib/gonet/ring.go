package gonet

import (
	"errors"
)

type UdpMsg struct {
	MsgNo int
	Index int
	Body  []byte
}

var (
	// ring
	ErrRingEmpty = errors.New("ring buffer empty")
	ErrRingFull  = errors.New("ring buffer full")
)

const (
	// signal command
	SignalNum   = 1
	ProtoFinish = 0
	ProtoReady  = 1
)

type Ring struct {
	// read
	rp   uint64
	num  uint64
	mask uint64
	// write
	wp   uint64
	data []UdpMsg
}

func NewRing(num int) *Ring {
	r := new(Ring)
	r.init(uint64(num))
	return r
}

func (r *Ring) Init(num int) {
	r.init(uint64(num))
}

func (r *Ring) init(num uint64) {
	// 2^N
	if num&(num-1) != 0 {
		for num&(num-1) != 0 {
			num &= (num - 1)
		}
		num = num << 1
	}
	r.data = make([]UdpMsg, num)
	r.num = num
	r.mask = r.num - 1
}

func (r *Ring) Get() (proto *UdpMsg, err error) {
	if r.rp == r.wp {
		return nil, ErrRingEmpty
	}
	proto = &r.data[r.rp&r.mask]
	return
}

func (r *Ring) GetAdv() {
	r.rp++
}

func (r *Ring) Set() (proto *UdpMsg, err error) {
	if r.wp-r.rp >= r.num {
		return nil, ErrRingFull
	}
	proto = &r.data[r.wp&r.mask]
	return
}

func (r *Ring) SetAdv() {
	r.wp++
}

func (r *Ring) Reset() {
	r.rp = 0
	r.wp = 0
}
