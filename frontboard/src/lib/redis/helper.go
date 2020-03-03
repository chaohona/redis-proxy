package redis

type RedisOp struct {
	conn *RedisCon
}

func NewRedisOp(conn *RedisCon) *RedisOp {
	return &RedisOp{conn: conn}
}

//func (this *RedisOp) Flush(isclose bool) error {
//	if isclose {
//		defer this.conn.D()
//	}
//	return this.conn.Flush()
//}

//func (this *RedisOp) Close() error {
//	return this.conn.Close()
//}

func (this *RedisOp) GetConn() *RedisCon {
	return this.conn
}

func (this *RedisOp) Set(key string, val interface{}) error {
	//return this.conn.Send("SET", key, val)
	return this.conn.SendArg2("SET", key, val)
}

func (this *RedisOp) SetEX(key string, seconds int64, val interface{}) error {
	return this.conn.SendArg3("SETEX", key, seconds, val)
}

func (this *RedisOp) SetObject(key string, obj interface{}) error {
	return this.conn.Send("HMSET", Args{}.Add(key).AddFlat(obj)...)
}

func (this *RedisOp) SetField(key string, fieldKey string, obj interface{}) error {
	//return this.conn.Send("HSET", Args{}.Add(key).Add(fieldKey).AddFlat(obj)...)
	return this.conn.SendArg3("HSET", key, fieldKey, obj)
}

func (this *RedisOp) Get(key string) (interface{}, error) {
	//return this.conn.Do("GET", key)
	return this.conn.DoArg1("GET", key)
}

func (this *RedisOp) GetObject1(key string, obj interface{}) error {
	v, err := Values(this.conn.DoArg1("HGETALL", key))
	if err != nil {
		return err
	}
	return ScanStruct(v, obj)
}

func (this *RedisOp) GetObject(key string, obj interface{}) error {
	fields, err := GetStructFields(obj)
	if err != nil {
		return err
	}
	v, err := Values(this.conn.Do("HMGET", Args{}.Add(key).AddFlat(fields)...))
	if err != nil {
		return err
	}
	return ScanStructByValue(v, fields, obj)
}

func (this *RedisOp) Incrby(key string, val interface{}) error {
	return this.conn.SendArg2("INCRBY", key, val)
}

func (this *RedisOp) FieldIncrby(key string, fieldKey interface{}, val interface{}) error {
	return this.conn.SendArg3("HINCRBY", key, fieldKey, val)
}

func (this *RedisOp) GetField(objKey string, fieldKey interface{}) (interface{}, error) {
	return this.conn.DoArg2("HGET", objKey, fieldKey)
}

func (this *RedisOp) Exist(key string) bool {
	v, err := Bool(this.conn.DoArg1("EXISTS", key))
	if err != nil {
		return false
	}
	return v
}

func (this *RedisOp) ExistField(objKey string, fieldKey interface{}) bool {
	v, err := Bool(this.conn.DoArg2("HEXISTS", objKey, fieldKey))
	if err != nil {
		return false
	}
	return v
}

func (this *RedisOp) DelField(objKey, fieldKey interface{}) bool {
	v, err := Bool(this.conn.DoArg2("HDEL", objKey, fieldKey))
	if err != nil {
		return false
	}
	return v
}

func (this *RedisOp) Del(key string) bool {
	v, err := Bool(this.conn.DoArg1("DEL", key))
	if err != nil {
		return false
	}
	return v
}

func (this *RedisOp) GeoRadiusUint(key string, longi, lati float64, radius, count int64) ([]uint64, []float64, error) {
	values, err := Values(this.Do("GEORADIUS", key, longi, lati, radius, "m", "WITHCOORD", "COUNT", count, "ASC"))
	if err != nil {
		return nil, nil, err
	}
	var ids []uint64
	var poss []float64
	for _, value := range values {
		datas, _ := Values(value, nil)
		for idx, data := range datas {
			if idx == 0 {
				id, err := Uint64(data, nil)
				if err != nil {
					return nil, nil, err
				}
				ids = append(ids, id)
			} else {
				pos, _ := Float64s(data, nil)
				poss = append(poss, pos...)
			}
		}
	}
	return ids, poss, nil
}

func (this *RedisOp) GeoRadiusString(key string, longi, lati float64, radius, count int64) ([]string, []float64, error) {
	values, err := Values(this.Do("GEORADIUS", key, longi, lati, radius, "m", "WITHCOORD", "COUNT", count, "ASC"))
	if err != nil {
		return nil, nil, err
	}
	var ids []string
	var poss []float64
	for _, value := range values {
		datas, _ := Values(value, nil)
		for idx, data := range datas {
			if idx == 0 {
				id, _ := String(data, nil)
				ids = append(ids, id)
			} else {
				pos, _ := Float64s(data, nil)
				poss = append(poss, pos...)
			}
		}
	}
	return ids, poss, nil
}

func (this *RedisOp) SetExpire(key string, second int) error {
	return this.conn.SendArg2("EXPIRE", key, second)
}

func (this *RedisOp) SetExpireAt(key string, timestamp int64) error {
	return this.conn.SendArg2("EXPIREAT", key, timestamp)
}

func (this *RedisOp) Do(commandName string, args ...interface{}) (interface{}, error) {
	return this.conn.Do(commandName, args...)
}

func (this *RedisOp) Send(commandName string, args ...interface{}) error {
	return this.conn.Send(commandName, args...)
}

func (this *RedisOp) GetFields(objKey string, args ...interface{}) (interface{}, error) {
	return this.Do("HMGET", Args{}.Add(objKey).AddFlat(args)...)
}

func (this *RedisOp) SetFields(objKey string, args ...interface{}) (interface{}, error) {
	return this.Do("HMSET", Args{}.Add(objKey).AddFlat(args)...)
}

type RedisObj struct {
	Name string
	conn *RedisPool
}

func NewRedisObj(name string, conn *RedisPool) *RedisObj {
	if conn == nil {
		return nil
	}
	return &RedisObj{name, conn}
}

func (this *RedisObj) GetConn() *RedisPool {
	return this.conn
}

//func (this *RedisObj) Flush(isclose bool) error {
//	if isclose {
//		defer this.conn.Close()
//	}
//	return this.conn.Flush()
//}

//func (this *RedisObj) Close() error {
//	return this.conn.Close()
//}

func (this *RedisObj) SetExpire(second int) error {
	return this.conn.Send("EXPIRE", this.Name, second)
}

func (this *RedisObj) Set(val interface{}) error {
	return this.conn.Send("HMSET", Args{}.Add(this.Name).AddFlat(val)...)
}

func (this *RedisObj) SetField(fieldKey, val interface{}) error {
	return this.conn.SendArg3("HSET", this.Name, fieldKey, val)
}

func (this *RedisObj) Get(val interface{}) error {
	v, err := Values(this.conn.DoArg1("HGETALL", this.Name))
	if err != nil {
		return err
	}
	return ScanStruct(v, val)
}

func (this *RedisObj) GetField(fieldKey string) (interface{}, error) {
	b, err := this.conn.DoArg2("HGET", this.Name, fieldKey)
	return b, err
}

func (this *RedisObj) Incrby(fieldKey string) error {
	return this.conn.SendArg3("HINCRBY", this.Name, fieldKey, 1)
}

func (this *RedisObj) Size() (int, error) {
	return Int(this.conn.DoArg1("HLEN", this.Name))
}

func (this *RedisObj) Remove(fieldKey string) error {
	return this.conn.SendArg2("HDEL", this.Name, fieldKey)
}

func (this *RedisObj) HasField(fieldKey string) bool {
	v, err := Bool(this.conn.DoArg2("HEXISTS", this.Name, fieldKey))
	if err != nil {
		return false
	}
	return v
}

func (this *RedisObj) Exist() bool {
	v, err := Bool(this.conn.DoArg1("EXISTS", this.Name))
	if err != nil {
		return false
	}
	return v
}

func (this *RedisObj) Clear() error {
	return this.conn.SendArg1("DEL", this.Name)
}
