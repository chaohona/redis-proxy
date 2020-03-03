package main

import (
	"common"
	"lib/glog"
	"lib/redis"
	"strconv"
	"time"
)

const (
	Key_RedisIdIndex string = "redis_id_index"
	Key_RedisPools   string = "redis_pools"
	Key_RedisList    string = "redis_list"
	Key_RedisInfo    string = "redis_info:"
)

// db 操作接口

type AdminRedis struct {
	Conn *redis.RedisOp
}

var g_admin_redis_instance *AdminRedis

func AdminRedis_GetMe() *AdminRedis {
	if g_admin_redis_instance == nil {
		g_admin_redis_instance = &AdminRedis{}
	}

	return g_admin_redis_instance
}

func (this *AdminRedis) Init(addr string) bool {
	conn, err := redis.NewRedisCon(addr, 100, 100)
	if err != nil {
		glog.Error("初始化数据库错误, addr:", addr, ", err:", err)
		return false
	}
	this.Conn = redis.NewRedisOp(conn)
	return true
}

func (this *AdminRedis) InvalidRedis(id uint64) bool {
	exist, err := redis.Int(this.Conn.Do("EXISTS", Key_RedisInfo+strconv.FormatUint(id, 10)))
	if err != nil {
		glog.Error("检查redis是否存在失败 id:", id, ", err:", err)
		return false
	}

	return exist > 0
}

func (this *AdminRedis) GetRedisInfo(id uint64) (int, *DB_RedisInfo) {
	var info DB_RedisInfo
	err := this.Conn.GetObject(Key_RedisInfo+strconv.FormatUint(id, 10), &info)
	if err != nil {
		glog.Error("获取redis详细信息出错, id:", id, ", err:", err)
		return common.ERR_DB, nil
	}

	return common.ERR_OK, &info
}

func (this *AdminRedis) GetRedisList() (result RedisListRet) {
	result.Ret = common.GetErrInfo(common.ERR_OK)
	ids, err := redis.Strings(this.Conn.Do("zrange", Key_RedisList, 0, -1))
	if err != nil {
		result.Ret = common.GetErrInfo(common.ERR_DB)
		return
	}

	for _, id := range ids {
		var info DB_RedisInfo
		err = this.Conn.GetObject(Key_RedisInfo+id, &info)
		if err != nil {
			glog.Error("获取redis详细信息出错, id:", id, ", err:", err)
			return
		}
		var retInstance RedisInstance
		RedisInstance_Conv(&retInstance, &info)
		result.ListInfo = append(result.ListInfo, retInstance)
	}
	return
}

func (this *AdminRedis) CreateRedisCheck() bool {
	num, err := redis.Int(this.Conn.Do("llen", Key_RedisPools))
	if err != nil {
		glog.Error("创建Redis获取可用地址, err:", err)
		return false
	}
	if num <= 0 {
		return false
	}

	return true
}

func (this *AdminRedis) CreateRedis(name string) (int, *DB_RedisInfo) {
	addr, err := redis.String(this.Conn.Do("lpop", Key_RedisPools))
	if err != nil {
		glog.Error("创建Redis获取地址出错, name:", name, ", err:", err)
		return common.ERR_DB, nil
	}
	if addr == "" {
		return common.ERR_REDIS_POOLS_EMPTY, nil
	}
	id, err := redis.Uint64(this.Conn.Do("incr", Key_RedisIdIndex))
	if err != nil {
		glog.Error("创建数据库信息,操作redis出错, name:", name, ", err:", err)
		return common.ERR_DB, nil
	}

	var info = DB_RedisInfo{
		Id:      id,
		Name:    name,
		Addr:    addr,
		MemUsed: 0,
		DBSize:  0,
		CTime:   time.Now().Unix(),
	}

	err = this.Conn.SetObject(Key_RedisInfo+strconv.FormatUint(id, 10), &info)
	if err != nil {
		glog.Error("插入数据库信息出错, name:", name, ", err:", err)
		return common.ERR_DB, nil
	}

	_, err = this.Conn.Do("zadd", Key_RedisList, info.CTime, id)
	if err != nil {
		glog.Error("插入数据库列表出错, name:", name, ", err:", err)
		return common.ERR_DB, nil
	}

	return common.ERR_OK, &info
}

func (this *AdminRedis) RecycleRedis(info *DB_RedisInfo) bool {
	_, err := this.Conn.Do("zrem", Key_RedisList, info.Id)
	if err != nil {
		glog.Error("回收Redis失败, id:", info.Id, ", name:", info.Name, ", err:", err)
		return false
	}
	_, err = this.Conn.Do("lpush", Key_RedisPools, info.Addr)
	if err != nil {
		glog.Error("回收Redis失败, id:", info.Id, ", name:", info.Name, ", err:", err)
		return false
	}

	errRet := this.Conn.Del(Key_RedisInfo + strconv.FormatUint(info.Id, 10))
	if !errRet {
		glog.Error("回收Redis删除数据失败, id:", info.Id, ", name:", info.Name, ", err:", err)
	}

	return true
}
