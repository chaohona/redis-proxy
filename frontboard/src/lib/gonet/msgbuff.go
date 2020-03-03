package gonet

import (
	"sync"
	"sync/atomic"
)

var (
	poolmgr      *msgPoolMgr
	poolmgrMutex sync.RWMutex
)

const (
	default_poollen = 1024 * 1024
)

type msgPoolMgr struct {
	poolMgr *sync.Pool
	poolLen int
}

func msgPoolMgr_GetMe() *msgPoolMgr {
	poolmgrMutex.RLock()
	if poolmgr == nil {
		poolmgrMutex.RUnlock()
		poolmgrMutex.Lock()
		if poolmgr == nil {
			poolmgr = &msgPoolMgr{
				poolLen: default_poollen,
				poolMgr: &sync.Pool{
					New: func() interface{} {
						return &msgPool{
							pool:    make([]byte, default_poollen),
							poolLen: default_poollen,
							tmpPool: make([]byte, cmd_max_size),
						}
					},
				},
			}
		}
		poolmgrMutex.Unlock()
		return poolmgr
	}
	poolmgrMutex.RUnlock()
	return poolmgr
}

func (this *msgPoolMgr) getMsgPool() *msgPool {
	return this.poolMgr.Get().(*msgPool)
}

func (this *msgPoolMgr) releaseMsgPool(pool *msgPool) {
	if pool == nil || pool.poolLen != this.poolLen {
		return
	}
	pool.reset()
	this.poolMgr.Put(pool)
	return
}

type msgPool struct {
	poolLen int    // pool总长度
	pool    []byte // 发送接收数据缓冲池子
	start   uint64 // 环开始下标
	end     uint64 // 环结束下标
	tmpPool []byte // 临时数据存储
	mutex   sync.Mutex
	index   int
	flag    byte
	msgLen  uint64
}

func (this *msgPool) reset() {
	this.start = 0
	this.end = 0
	this.index = 0
	this.msgLen = 0
	this.flag = 0
}

func (this *msgPool) addSendIndex(sendLen int) {
	atomic.AddUint64(&this.start, uint64(sendLen))
}

// 返回需要发送的数据与buff
func (this *msgPool) getSendMsgBuff() ([]byte, bool) {
	end := atomic.LoadUint64(&this.end)
	start := atomic.LoadUint64(&this.start)
	if start == end {
		return nil, false
	}
	if end-start > uint64(this.poolLen) {
		return nil, true
	}
	sIndex := int(start % uint64(this.poolLen))
	eIndex := int(end % uint64(this.poolLen))
	if sIndex > eIndex {
		return this.pool[sIndex:], false
	} else {
		return this.pool[sIndex:eIndex], false
	}
	return nil, false
}

func (this *msgPool) addSendMsg(buffer []byte, flag byte) bool {
	buffLen := len(buffer)
	if buffLen > cmd_max_size {
		return false
	}
	this.mutex.Lock()
	this.index = int(this.end % uint64(this.poolLen))
	if this.index <= this.poolLen-4 {
		this.pool[this.index] = byte(buffLen)
		this.pool[this.index+1] = byte(buffLen >> 8)
		this.pool[this.index+2] = byte(buffLen >> 16)
		this.pool[this.index+3] = byte(flag)
		this.index += 4
		if this.index == this.poolLen {
			this.index = 0
		}
	} else if this.index+1 == this.poolLen {
		this.pool[this.poolLen-1] = byte(buffLen)
		this.pool[0] = byte(buffLen >> 8)
		this.pool[1] = byte(buffLen >> 16)
		this.pool[2] = byte(flag)
		this.index = 3
	} else if this.index+2 == this.poolLen {
		this.pool[this.poolLen-2] = byte(buffLen)
		this.pool[this.poolLen-1] = byte(buffLen >> 8)
		this.pool[0] = byte(buffLen >> 16)
		this.pool[1] = byte(flag)
		this.index = 2
	} else if this.index+3 == this.poolLen {
		this.pool[this.poolLen-3] = byte(buffLen)
		this.pool[this.poolLen-2] = byte(buffLen >> 8)
		this.pool[this.poolLen-1] = byte(buffLen >> 16)
		this.pool[0] = byte(flag)
		this.index = 1
	}

	// 将buff拷如发送队列中
	if buffLen > this.poolLen-this.index { // 需要从池子头开始拷贝
		copy(this.pool[this.index:], buffer[:this.poolLen-this.index])
		copy(this.pool, buffer[this.poolLen-this.index:])
	} else {
		copy(this.pool[this.index:], buffer)
	}
	atomic.AddUint64(&this.end, uint64(buffLen+4))
	this.mutex.Unlock()
	return true
}

// 返回接收消息的buff与最少应该接收的字节数
func (this *msgPool) getRecvBuff() ([]byte, int) {
	if this.end == 0 { // 初始的时候最少要接收4个字节的
		return this.pool, cmd_header_size
	}
	if this.msgLen == 0 { // 获取消息长度
		if this.end-this.start >= cmd_header_size {
			this.msgLen = uint64(this.pool[int(this.start%uint64(this.poolLen))]) | uint64(this.pool[int((this.start+1)%uint64(this.poolLen))])<<8 | uint64(this.pool[int((this.start+2)%uint64(this.poolLen))])<<16
			if this.msgLen > cmd_max_size {

				return nil, 0
			}
		}
	}

	if this.msgLen != 0 && this.end-this.start >= this.msgLen+cmd_header_size {
		// 已经接收到一个可以处理的消息
		return nil, 0
	}
	msgLen := this.msgLen + cmd_header_size

	sIndex := this.start % uint64(this.poolLen)
	if sIndex+msgLen <= uint64(this.poolLen) {
		return this.pool[int(this.end%uint64(this.poolLen)):], int(msgLen - this.end + this.start)
	} else { // 出现拐弯现象了
		eIndex := this.end % uint64(this.poolLen)
		leftLen := msgLen - this.end + this.start
		if eIndex+leftLen >= uint64(this.poolLen) {
			return this.pool[eIndex:], int(uint64(this.poolLen) - eIndex)
		} else {
			return this.pool[eIndex:], int(leftLen)
		}
	}

	// 不应该执行到这儿
	return nil, 0
}

// 获取需要被处理的消息
func (this *msgPool) getRecvMsg() ([]byte, byte) {
	if this.end-this.start < this.msgLen+cmd_header_size || this.start > this.end {
		return nil, 0
	}
	if this.msgLen == 0 && this.end < this.start+cmd_header_size {
		return nil, 0
	}
	if this.msgLen == 0 {
		return nil, 0
	}
	if this.msgLen > cmd_max_size {
		return nil, 0
	}
	startIndex := int(this.start % uint64(this.poolLen))
	endIndex := int((this.start + this.msgLen + cmd_header_size) % uint64(this.poolLen))
	if startIndex+cmd_header_size > endIndex {
		if startIndex+cmd_header_size <= this.poolLen {
			this.flag = this.pool[startIndex+cmd_header_size-1]
			copy(this.tmpPool, this.pool[startIndex+cmd_header_size:])
			copy(this.tmpPool[this.poolLen-startIndex-cmd_header_size:], this.pool[:int(this.msgLen)-(this.poolLen-startIndex-cmd_header_size)])
			this.start += this.msgLen + cmd_header_size
			this.msgLen = 0
			return this.tmpPool[:this.msgLen], this.flag
		} else {
			this.flag = this.pool[cmd_header_size-(this.poolLen-startIndex)-1]
			this.start += this.msgLen + cmd_header_size
			this.msgLen = 0

			return this.pool[cmd_header_size-(this.poolLen-startIndex) : endIndex], this.flag
		}
	} else {
		this.start += this.msgLen + cmd_header_size
		this.msgLen = 0

		return this.pool[startIndex+cmd_header_size : endIndex], this.pool[startIndex+cmd_header_size-1]
	}
	return nil, 0
}

// 增加结束下标,计算下一个消息的长度
func (this *msgPool) addRecvIndex(num int) bool {
	this.end += uint64(num)
	if this.end-this.start > uint64(this.poolLen) {
		return false
	}
	if this.start == 0 { // 接收第一条消息
		if this.end < cmd_header_size {
			return false
		}
		this.msgLen = uint64(this.pool[0]) | uint64(this.pool[1])<<8 | uint64(this.pool[2])<<16
	}
	if this.msgLen > cmd_max_size {
		return false
	}
	return true
}
