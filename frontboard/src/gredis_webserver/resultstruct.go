package main

import (
	"common"
)

type RedisInstance struct {
	Id      uint64 // 实例编号
	Name    string // 实例名
	Addr    string // 地址
	MemUsed uint64 // 使用内存大小
	DBSize  uint64 // dbsize
	CTime   int64  // 创建时间
}

type RedisListRet struct {
	ListInfo []RedisInstance // 实例列表
	TotalCnt uint64          // 总共可以创建的Redis个数
	UsedCnt  uint64          // 已经创建的Redis个数
	Ret      common.ErrInfo
}
