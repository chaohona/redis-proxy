package redis

import (
	"fmt"
	"sync"
	"testing"
	"time"
)

var (
	curIdx = 1
)

func init() {
	//glog.SetLogFile("tset.txt")
}

func TestPoolMGet(t *testing.T) {
	//t.Log()
	pool, err := NewRedisPool("@tcp(192.168.124.138:6381)/0", 5, 1024)
	if err != nil {
		return
	}
	doHMGET(pool, "Money", "EMoney", "Level")
}

func doHMGET(pool *RedisPool, args ...interface{}) {
	gamedata := "gamedata:1"
	strs, err := Strings(pool.Do("HMGET", Args{}.Add(gamedata).AddFlat(args)...))
	if err != nil {
		fmt.Println("err:", err)
		return
	}
	fmt.Println("strs:", strs)
}

func testPool(t *testing.T) {
	//t.Log()

	for i := 0; i < 20; i++ {
		pool, err := NewRedisPool("@tcp(192.168.124.138:6381)/0", 5, 1024)
		if err != nil {
			return
		}
		count := 200
		var w sync.WaitGroup
		fmt.Println(time.Now().Format("2006-01-02 15:04:05.999999999"))

		for i := 0; i < count; i++ {
			w.Add(1)
			go testCount(&w, pool, 5000, i)
		}
		time.Sleep(time.Second * 1)
		pool.Destory()
		w.Wait()
		fmt.Println(time.Now().Format("2006-01-02 15:04:05.999999999"))
	}
	//	rel, err := String(pool.Do("set", "db", "fdafdsa"))
	//	t.Log(rel)

	//	get, err := String(pool.Do("get", "db"))

	//t.Log(err)
	//t.Log(get)
}

func testCount(w *sync.WaitGroup, pool *RedisPool, size int, idx int) {
	for i := 0; i < size; i++ {
		pool.Do("set", fmt.Sprintf("db_%d_%d", idx, i), "fdafdsa")
	}
	w.Done()
	//String(pool.Do("set", "db", "fdafdsa"))
}

//func BenchmarkPool(b *testing.B) {
//	b.StopTimer()
//	pool, err := NewRedisPool("@tcp(192.168.124.130:6379)/0", 5, 1024)
//	if err != nil {
//		return
//	}
//	cmds := pool.GetSend()
//	b.StartTimer()
//	for i := 0; i < b.N; i++ {
//		//pool.Do("set", fmt.Sprintf("db_%d", curIdx), "1")
//		for j := 0; j < 10; j++ {
//			pool.SendSync(&cmds, "set", "db", "aab")
//		}
//		pool.Flush(cmds)
//	}
//}
