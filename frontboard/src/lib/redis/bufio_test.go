package redis

import (
	"bufio"
	"bytes"
	"testing"
)

var (
	str        = "test send mhget commond for 中国"
	p          = NewPacketSize(64)
	lenScratch [32]byte
	endStr     = "\r\n"
)

//func TestBuf(t *testing.T) {
//	//	str := []byte("test send mhget commond for 中国")
//	str := "test send mhget commond for 中国"
//	p := NewPacketSize(1024)
//	//p.WriteBytes(str)
//	p.WriteString(&str)
//	//p.WriteLen(len(str))
//	t.Log("bytes :", string(p.buf[:p.w]))

//	bb := make([]byte, 1024)
//	l := formatInt(bb, len(str))
//	l = formatInt(bb[l:], len(str))
//	l = formatInt(bb[2*l:], len(str))
//	t.Log("int2string:", l, string(bb[:l*3]))
//}

func BenchmarkCmd(b *testing.B) {
	for i := 0; i < b.N; i++ {
		p.Reset()
		testHSet("gamedata:12321", "ps", "12321")
	}
}

func BenchmarkSetInt(b *testing.B) {
	for i := 0; i < b.N; i++ {
		p.Reset()
		HSetInt("gamedata:12321", "ps", 12321)
	}
}
func BenchmarkSetString(b *testing.B) {
	for i := 0; i < b.N; i++ {
		p.Reset()
		HSet("gamedata:12321", "ps", "12321")
	}
}

func BenchmarkSetval(b *testing.B) {
	for i := 0; i < b.N; i++ {
		p.Reset()
		HSetVal("gamedata:12321", "ps", "12321")
	}
}

func HSet(key string, field string, val string) {
	p.WriteCmd(&flagHset, 4)
	p.WriteString(&key)
	p.WriteString(&key)
	p.WriteString(&val)
}

func HSetInt(key string, field string, val int64) {
	p.WriteCmd(&flagHset, 4)
	p.WriteString(&key)
	p.WriteString(&key)
	p.WriteInt64(val)
}
func HSetVal(key string, field string, val interface{}) {
	p.WriteCmd(&flagHset, 4)
	p.WriteString(&key)
	p.WriteString(&key)
	p.WriteArg(val)
}

func BenchmarkCmd2(b *testing.B) {
	for i := 0; i < b.N; i++ {
		p.Reset()
		testDo("set", "gamedata:12321", "ps", "12321")
	}
}

func testHSet(key string, field string, val string) {
	p.WriteCmd(&flagHset, 4)
	p.WriteString(&key)
	p.WriteString(&field)
	p.WriteString(&val)
}

func testDo(cmd string, args ...interface{}) {
	//func testDo(cmd string, k1 string, k2 interface{}, k3 interface{}) {
	p.WriteCommond(&cmd, &args)
}

//func BenchmarkStrconv(b *testing.B) {
//	b.StopTimer()
//	//	count := 35
//	bb := make([]byte, 1024)
//	b.StartTimer()
//	for i := 0; i < b.N; i++ {
//		strconv.AppendInt(bb, 35, 10)
//	}
//}

//func BenchmarkStrconv2(b *testing.B) {
//	b.StopTimer()
//	//	count := 35
//	bb := make([]byte, 1024)
//	b.StartTimer()
//	for i := 0; i < b.N; i++ {
//		formatInt(bb, 35)
//	}
//}

func formatInt(b []byte, u int) int {
	var a [10]byte
	var i = 10
	for u >= 10 {
		i--
		q := u / 10
		a[i] = byte(u - q*10 + '0')
		u = q
	}
	i--
	a[i] = byte(u + '0')
	copy(b, a[i:])
	return 10 - i
}

func BenchmarkBufio(b *testing.B) {
	b.StopTimer()
	//	str := []byte("test send mhget commond for 中国")
	str := "test send mhget commond for 中国"
	bf := bytes.NewBuffer(make([]byte, 64))
	p := bufio.NewWriterSize(bf, 64)
	//	p := NewPacketSize(1024)

	b.StartTimer()
	for i := 0; i < b.N; i++ {
		p.Reset(bf)
		//p.Write(str)
		writeString(p, &str)
		//		p.WriteString(str)
	}
}

func writeString(buf *bufio.Writer, s *string) {
	writeLen(buf, '$', len(*s))
	buf.WriteString(*s)
	buf.WriteString("\r\n")
}

func writeLen(buf *bufio.Writer, prefix byte, n int) {
	lenScratch[len(lenScratch)-1] = '\n'
	lenScratch[len(lenScratch)-2] = '\r'
	i := len(lenScratch) - 3
	for {
		lenScratch[i] = byte('0' + n%10)
		i -= 1
		n = n / 10
		if n == 0 {
			break
		}
	}
	lenScratch[i] = prefix
	buf.Write(lenScratch[i:])
}
