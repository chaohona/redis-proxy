// MIT License
//
// Copyright (c) 2017 José Santos <henrique_1609@me.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

package fastprinter

import (
	"fmt"
	"io"
	"math"
	"reflect"
	"strings"
	"testing"
)

type devNull struct{}

func (*devNull) Write(_ []byte) (int, error) {
	return 0, nil
}

var ww io.Writer = (*devNull)(nil)

type testWriter struct {
	main  [stringBufferSize * 64]byte
	bytes []byte
}

func newTestWriter() *testWriter {
	ww := new(testWriter)
	ww.reset()
	return ww
}

func (w *testWriter) Write(b []byte) (n int, err error) {
	w.bytes = append(w.bytes, b...)
	n = len(b)
	return
}

func (w *testWriter) reset() {
	w.bytes = w.main[:0]
}

func (w *testWriter) Assert(m string) {
	if string(w.bytes) != m {
		panic(fmt.Errorf("expected value is %s got %s", m, string(w.bytes)))
	}
	w.reset()
}

func (w *testWriter) String() string {
	defer w.reset()
	return string(w.bytes)
}

var w = newTestWriter()

func TestPrintBool(t *testing.T) {

	allocsPerRun := testing.AllocsPerRun(3000, func() {
		PrintBool(w, true)
		w.Assert("true")
		PrintBool(w, false)
		w.Assert("false")
	})

	if allocsPerRun > 0 {
		t.Errorf("PrintBool is allocating %f", allocsPerRun)
	}
}

var bigString = strings.Repeat("Hello World!", stringBufferSize)

func TestPrintString(t *testing.T) {

	allocsPerRun := testing.AllocsPerRun(3000, func() {
		const value = "Hello World"
		PrintString(w, value)
		w.Assert(value)
		PrintString(w, bigString)
		w.Assert(bigString)
	})

	if allocsPerRun > 0 {
		t.Errorf("PrintString is allocating %f", allocsPerRun)
	}

}

func TestPrintFloat(t *testing.T) {

	allocsPerRun := testing.AllocsPerRun(5000, func() {
		PrintFloat(w, 44.4)
		w.Assert("44.4")
		PrintFloat(w, math.Pi)
		w.Assert("3.141592653589793")
		PrintFloatPrecision(w, math.Pi, 2)
		w.Assert("3.14")
		PrintFloatPrecision(w, math.Pi, 2)
		w.Assert("3.14")
		PrintFloatPrecision(w, -1.23, 2)
		w.Assert("-1.23")
		PrintFloatPrecision(w, 1.23, 2)
		w.Assert("1.23")
	})

	if allocsPerRun > 0 {
		t.Errorf("PrintFloat is allocating %f", allocsPerRun)
	}
}

func TestPrintBytes(t *testing.T) {

	hellobytes := []byte("Hello world!!")

	allocsPerRun := testing.AllocsPerRun(5000, func() {
		PrintValue(w, reflect.ValueOf(&hellobytes).Elem())
		w.Assert("Hello world!!")
	})

	if allocsPerRun > 0 {
		t.Errorf("PrintValue is allocating %f", allocsPerRun)
	}
}

func TestPrintInt(t *testing.T) {

	allocsPerRun := testing.AllocsPerRun(3000, func() {
		PrintInt(w, -300)
		w.Assert("-300")
		PrintUint(w, 300)
		w.Assert("300")
	})

	if allocsPerRun > 0 {
		t.Errorf("Print(u)Int is allocating %f", allocsPerRun)
	}
}

func BenchmarkPrintInt(b *testing.B) {
	for i := 0; i < b.N; i++ {
		PrintInt(ww, 9000000000)
	}
}

func BenchmarkPrintFloat(b *testing.B) {
	for i := 0; i < b.N; i++ {
		PrintFloat(ww, 9000.000000)
	}
}

func BenchmarkPrintFloatPrec(b *testing.B) {
	for i := 0; i < b.N; i++ {
		PrintFloatPrecision(ww, 9000.000000, 5)
	}
}

func BenchmarkPrintString(b *testing.B) {
	for i := 0; i < b.N; i++ {
		PrintString(ww, "-------------------------------------------------------------------------------")
	}
}

func BenchmarkPrintIntFmt(b *testing.B) {
	for i := 0; i < b.N; i++ {
		fmt.Fprint(ww, 9000000000)
	}
}

func BenchmarkPrintFloatFmt(b *testing.B) {
	for i := 0; i < b.N; i++ {
		fmt.Fprint(ww, 9000.000000)
	}
}

func BenchmarkPrintStringFmt(b *testing.B) {
	for i := 0; i < b.N; i++ {
		fmt.Fprint(ww, "-------------------------------------------------------------------------------")
	}
}
