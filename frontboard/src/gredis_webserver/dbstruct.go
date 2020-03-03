package main

type DB_RedisInfo struct {
	Id      uint64
	Name    string
	Addr    string
	MemUsed uint64
	DBSize  uint64
	CTime   int64
}

func RedisInstance_Conv(out *RedisInstance, in *DB_RedisInfo) {
	out.Id = in.Id
	out.Name = in.Name
	out.DBSize = in.DBSize
	out.MemUsed = in.MemUsed
	out.Addr = in.Addr
	out.CTime = in.CTime
}
