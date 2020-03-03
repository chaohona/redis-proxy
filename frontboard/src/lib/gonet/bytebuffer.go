package gonet

import (
	"encoding/binary"
)

const (
	presize  = 0
	initsize = 128
)

type ByteBuffer struct {
	_buffer      []byte
	_prependSize int
	_readerIndex int
	_writerIndex int
}

func NewByteBuffer() *ByteBuffer {
	return &ByteBuffer{
		_buffer:      make([]byte, presize+initsize),
		_prependSize: presize,
		_readerIndex: presize,
		_writerIndex: presize,
	}
}

func (this *ByteBuffer) AppendUint16(num uint16) {
	this.WrGrow(2)
	binary.LittleEndian.PutUint16(this._buffer[this._writerIndex:], num)
	this.WrFlip(2)
}

func (this *ByteBuffer) AppendUint32(num uint32) {
	this.WrGrow(4)
	binary.LittleEndian.PutUint32(this._buffer[this._writerIndex:], num)
	this.WrFlip(4)
}

func (this *ByteBuffer) AppendUint64(num uint64) {
	this.WrGrow(8)
	binary.LittleEndian.PutUint64(this._buffer[this._writerIndex:], num)
	this.WrFlip(8)
}

func (this *ByteBuffer) Append(buff ...byte) {
	size := len(buff)
	if size == 0 {
		return
	}
	this.WrGrow(size)
	copy(this._buffer[this._writerIndex:], buff)
	this.WrFlip(size)
}

func (this *ByteBuffer) WrBuf() []byte {
	if this._writerIndex >= len(this._buffer) {
		return nil
	}
	return this._buffer[this._writerIndex:]
}

func (this *ByteBuffer) WrSize() int {
	return len(this._buffer) - this._writerIndex
}

func (this *ByteBuffer) WrFlip(size int) {
	this._writerIndex += size
}

func (this *ByteBuffer) WrGrow(size int) {
	if size > this.WrSize() {
		this.wrreserve(size)
	}
}

func (this *ByteBuffer) RdBuf() []byte {
	if this._readerIndex >= len(this._buffer) {
		return nil
	}
	return this._buffer[this._readerIndex:]
}

func (this *ByteBuffer) RdReady() bool {
	return this._writerIndex > this._readerIndex
}

func (this *ByteBuffer) RdSize() int {
	return this._writerIndex - this._readerIndex
}

func (this *ByteBuffer) RdFlip(size int) {
	if size < this.RdSize() {
		this._readerIndex += size
	} else {
		this.Reset()
	}
}

func (this *ByteBuffer) Reset() {
	this._readerIndex = this._prependSize
	this._writerIndex = this._prependSize
}

func (this *ByteBuffer) MaxSize() int {
	return len(this._buffer)
}

func (this *ByteBuffer) wrreserve(size int) {
	if this.WrSize()+this._readerIndex < size+this._prependSize {
		tmpbuff := make([]byte, this._writerIndex+size)
		copy(tmpbuff, this._buffer)
		this._buffer = tmpbuff
	} else {
		readable := this.RdSize()
		copy(this._buffer[this._prependSize:], this._buffer[this._readerIndex:this._writerIndex])
		this._readerIndex = this._prependSize
		this._writerIndex = this._readerIndex + readable
	}
}

func (this *ByteBuffer) Prepend(buff []byte) bool {
	size := len(buff)
	if this._readerIndex < size {
		return false
	}
	this._readerIndex -= size
	copy(this._buffer[this._readerIndex:], buff)
	return true
}
