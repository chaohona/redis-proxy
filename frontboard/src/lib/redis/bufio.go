package redis

import (
	"bytes"
	"fmt"
	"io"
	"strconv"
)

const (
	packSize          = 64
	maxpackSize       = 1024 * 1024 * 16
	defaultBufSize    = 4096
	minReadBufferSize = 16
)

var (
	endflag   = [2]byte{'\r', '\n'}
	trueFlag  = "1"
	falseFlag = "0"
	nilFlag   = ""
)

type Packet struct {
	buf  []byte
	a    [10]byte
	r, w int // buf read and write positions
}

func NewPacketSize(size int) *Packet {
	if size < minReadBufferSize {
		size = minReadBufferSize
	}
	r := new(Packet)
	r.buf = make([]byte, size)
	return r
}

func NewPacket() *Packet {
	return NewPacketSize(defaultBufSize)
}

func (b *Packet) Reset() {
	b.r, b.w = 0, 0
}

func (b *Packet) Pos(r int) {
	if r >= 0 && r < b.w {
		b.r = r
	}
}

func (b *Packet) fill(r io.Reader) {
	n, _ := r.Read(b.buf[b.w:])
	if n < 0 {
		return
	}
	b.w += n
}

func (b *Packet) ReadBytes(r io.Reader, bt []byte, ismove bool) (err error) {
	need := len(bt) - (b.w - b.r)
	if need > 0 {
		nn := 0
		nn, err = io.ReadAtLeast(r, b.buf[b.w:], need)
		if nn > 0 {
			b.w += nn
		}
		if err != nil {
			return
		}
	}
	copy(bt, b.buf[b.r:b.r+len(bt)])
	if ismove {
		b.r += len((bt))
	}
	return
}

func (b *Packet) ReadByte(r io.Reader) (c byte, err error) {
	if b.w-b.r < 1 {
		nn := 0
		nn, err = io.ReadAtLeast(r, b.buf[b.w:], 1)
		if nn > 0 {
			b.w += nn
		}
		if err != nil {
			return
		}
	}
	c = b.buf[b.r]
	b.r++
	return
}

func (b *Packet) ReadLine(r io.Reader) (line []byte, err error) {
	var n = 0
	for {
		if i := bytes.IndexByte(b.buf[b.r:b.w], '\n'); i >= 0 {
			line = b.buf[b.r : b.r+i+1]
			b.r += i + 1
			if b.r == b.w {
				b.r, b.w = 0, 0
			}
			break
		}

		if len(b.buf)-b.w < 4096 {
			b.grow(4096)
		}
		if n, err = r.Read(b.buf[b.w:]); err != nil {
			fmt.Println("[Conn] conn read error:", err, ", w:", b.w, ", len:", len(b.buf))
			return
		}
		b.w = b.w + n
	}
	n = len(line) - 2
	if n < 0 || line[n] != '\r' {
		return nil, errNegativeInt
	}
	//fmt.Println("get line:", string(line[:n]))
	return line[:n], nil
}

func (b *Packet) Bytes(bt []byte) (err error) {
	if bt == nil || len(bt) == 0 {
		fmt.Println(bt)
		return ErrNil
	}
	b.grow(len(bt))
	copy(b.buf[b.w:], bt)
	b.w = b.w + len(bt)
	return
}

func (b *Packet) WriteBytes(s *[]byte) {
	b.WriteLen('$', len(*s))
	b.grow(len(*s) + 2)
	pos := b.w
	copy(b.buf[pos:], *s)
	pos = pos + len(*s)
	b.buf[pos] = '\r'
	b.buf[pos+1] = '\n'
	b.w = pos + 2
}

func (b *Packet) WriteString(s *string) {
	b.WriteLen('$', len(*s))
	b.grow(len(*s) + 2)
	pos := b.w
	copy(b.buf[pos:], *s)
	pos = pos + len(*s)
	b.buf[pos] = '\r'
	b.buf[pos+1] = '\n'
	b.w = pos + 2
}

func (b *Packet) grow(size int) {
	need := size - len(b.buf) + b.w
	if need > 0 {
		if need < packSize {
			need = packSize
		}
		//fmt.Println(len(b.buf), ", size:", size, ", w:", b.w, ", need:", need)
		grow := make([]byte, len(b.buf)+need)
		if b.w > 0 {
			copy(grow, b.buf[0:b.w])
		}
		b.buf = grow
	}
}

func (b *Packet) WriteLen(prefix byte, n int) {
	a := b.a
	i, q, pos := 10, 0, b.w
	for n >= 10 {
		i--
		q = n / 10
		a[i] = byte(n - q*10 + '0')
		n = q
	}
	i--
	a[i] = byte(n + '0')
	// check need size
	b.grow(13 - i)
	// write to buf
	b.buf[pos] = prefix
	copy(b.buf[pos+1:], a[i:])
	pos = pos + 11 - i
	b.buf[pos] = '\r'
	b.buf[pos+1] = '\n'
	b.w = pos + 2
}

func (b *Packet) WriteInt64(n int64) {
	a := b.a
	var q int64
	i, pos := 10, b.w
	for n >= 10 {
		i--
		q = n / 10
		a[i] = byte(n - q*10 + '0')
		n = q
	}
	i--
	a[i] = byte(n + '0')
	// check need size
	b.grow(12 - i)
	// write to buf
	copy(b.buf[pos:], a[i:])
	pos = pos + 10 - i
	b.buf[pos] = '\r'
	b.buf[pos+1] = '\n'
	b.w = pos + 2
}

func (b *Packet) WriteFloat64(n *float64) {
	p := strconv.FormatFloat(*n, 'g', -1, 64)
	b.WriteString(&p)
}

func (b *Packet) WriteArg(arg interface{}) {
	switch arg := arg.(type) {
	case string:
		b.WriteString(&arg)
	case []byte:
		b.WriteBytes(&arg)
	case int:
		b.WriteInt64(int64(arg))
	case int64:
		b.WriteInt64(arg)
	case float64:
		b.WriteFloat64(&arg)
	case bool:
		if arg {
			b.WriteString(&trueFlag)
		} else {
			b.WriteString(&falseFlag)
		}
	case nil:
		b.WriteString(&nilFlag)
	default:
		var buf bytes.Buffer
		fmt.Fprint(&buf, arg)
		bpf := buf.Bytes()
		b.WriteBytes(&bpf)
	}
}
func (b *Packet) WriteCmd(cmd *string, size int) {
	b.WriteLen('*', size)
	b.WriteString(cmd)
}

func (b *Packet) WriteCommond(cmd *string, args *[]interface{}) {
	b.WriteLen('*', 1+len(*args))
	b.WriteString(cmd)
	for _, arg := range *args {
		switch arg := arg.(type) {
		case string:
			b.WriteString(&arg)
		case []byte:
			b.WriteBytes(&arg)
		case int:
			b.WriteInt64(int64(arg))
		case int64:
			b.WriteInt64(arg)
		case float64:
			b.WriteFloat64(&arg)
		case bool:
			if arg {
				b.WriteString(&trueFlag)
			} else {
				b.WriteString(&falseFlag)
			}
		case nil:
			b.WriteString(&nilFlag)
		default:
			var buf bytes.Buffer
			fmt.Fprint(&buf, arg)
			bpf := buf.Bytes()
			b.WriteBytes(&bpf)
		}
	}
}

func (b *Packet) Flush(w io.Writer) (err error) {
	if b.w == 0 {
		return
	}
	//	fmt.Println(len(b.buf), ",w:", b.w)
	n, nn := 0, 0
	for {
		nn, err = w.Write(b.buf[n:b.w])
		if nn < b.w-n && err == nil {
			n = n + nn
		} else {
			break
		}
	}
	b.Reset()
	return
}
