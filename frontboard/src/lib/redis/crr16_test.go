package redis

//import (
//	"hash/crc32"
//	"testing"
//)

//func BenchmarkCrc16(b *testing.B) {
//	b.StopTimer()
//	var s []byte = []byte("fdsafdsaf")
//	b.StartTimer()
//	for i := 0; i < b.N; i++ {
//		KeyHashSlot(s)
//	}
//}

//func BenchmarkCrc32(b *testing.B) {
//	b.StopTimer()
//	var s []byte = []byte("fdsafsf")
//	b.StartTimer()
//	for i := 0; i < b.N; i++ {
//		crc32.ChecksumIEEE(s)
//	}
//}
