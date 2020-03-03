package redis

import (
	"errors"
	"lib/glog"
	"regexp"
	"strings"
	"sync"
)

const (
	MAX_KEY_LEN = 464
)

var (
	RedisConfigError = errors.New("redis address config error!")
	RedisAuthError   = errors.New("redis auth error!")
	ErrorKeyTooLong  = errors.New("key is too long")

	addrReg = regexp.MustCompile(`(\w*\d*)?@tcp\((\d+.\d+.\d+.\d+:\d+)+\)\/(\d+)+`)
)

type AddressFunc func() string

var pools map[int]*RedisPool

func init() {
	pools = make(map[int]*RedisPool)
}

func SetWarnTimeOut(key int, t int64) {
	pool := pools[key]
	if pool == nil {
		return
	}
	if t == 0 {
		t = Default_Warn_TT
	}
	//	for _, con := range pool.nodes {
	//		con.SetWarnTimeOut(t)
	//	}
	pool.node.SetWarnTimeOut(t)
}

func SetReconTimeOut(key int, t int64) {
	pool := pools[key]
	if pool == nil {
		return
	}
	if t == 0 {
		t = Default_Recon_TT
	}
	//	for _, con := range pool.nodes {
	//		con.SetReconTimeOut(t)
	//	}
	pool.node.SetReconTimeOut(t)
}

func SetTimeOut(key int, warn, recon int64) {
	SetWarnTimeOut(key, warn)
	SetReconTimeOut(key, recon)
}

func RegRedisKey(key int, address string) error {
	pool, err := NewRedisPool(address, 2, 1024)
	if err != nil {
		return err
	}
	pools[key] = pool
	glog.Info("[Redis] Reg-Redis-address:", address, "-key:", key)
	return nil
}

func RegRedisKeyByFunc(getAddress AddressFunc, keys ...int) error {
	pool, err := NewRedisPoolByFunc(getAddress, 2, 1024)
	if err != nil {
		return err
	}
	for idx, _ := range keys {
		key := keys[idx]
		pools[key] = pool
		glog.Info("[Redis] Reg-Redis-address:", pool.node.server, "-key:", key)
	}
	return nil
}

var (
	redisAddrByFunc func(string) string
)

func RegKeyFunc(getAddressFunc func(string) string) {
	redisAddrByFunc = getAddressFunc
}

func RegKeyByFunc(addressKey string, keys ...int) error {
	for idx, _ := range keys {
		key := keys[idx]
		pool, err := NewRedisPoolByFunc(func() string { return redisAddrByFunc(addressKey) }, 2, 1024)
		if err != nil {
			glog.Error("[Redis] Reg Redis address:", pool.node.server, ",key:", addressKey)
			return err
		}
		pools[key] = pool
		glog.Info("[Redis] Reg Redis address:", pool.node.server, ",key:", addressKey)
	}
	return nil
}

func RegRedisKeySize(key int, address string, size int) error {
	pool, err := NewRedisPool(address, size, 1024)
	if err != nil {
		return err
	}
	pools[key] = pool
	glog.Info("[Redis] Reg Redis, address:", address, ",key:", key)
	return nil
}

func Get(key int) *RedisCon {
	pool, ok := pools[key]
	if !ok {
		return nil
	}
	return pool.get()
}

func CloseAll() {
	for _, pool := range pools {
		pool.Destory()
	}
}

type RedisPool struct {
	nodes []*RedisCon
	//	round   *chans
	node    *RedisCon
	size    int
	cur     int
	address string
	pwd     string
	slot    string
}

// e.g.  pwd@tcp(192.168.124.130:6379)/1
func NewRedisPool(address string, size, chanSize int) (pool *RedisPool, err error) {
	node, err := NewRedisCon(address, chanSize, chanSize)
	if err != nil {
		return nil, err
	}
	pool = &RedisPool{address: address, node: node}
	//	pool.node = node
	//	pool.nodes = make([]*RedisCon, size)
	//	//pool.round = NewChans(chanSize, chanSize)
	//	for i := 0; i < size; i++ {
	//		rs, err = NewRedisCon(address, pwd, chanSize, chanSize)
	//		if err != nil {
	//			return
	//		}
	//		if dbKey != "" && dbKey != "0" {
	//			rs.SendToConn(ch, "SELECT", dbKey)
	//			result = <-ch
	//			ret, err = String(result.Data, result.Err)
	//			if err != nil {
	//				glog.Error("Get select redis db " + dbKey + " result failed")
	//				rs.fatal(err)
	//				return
	//			}
	//			if ret != "OK" {
	//				glog.Error("Invalid db slot")
	//				err = RedisAuthError
	//				rs.fatal(err)
	//				return
	//			}
	//		}

	//		pool.nodes[i] = rs
	//	}
	return
}

// e.g.  pwd@tcp(192.168.124.130:6379)/1
func NewRedisPoolByFunc(addressFunc AddressFunc, size, chanSize int) (pool *RedisPool, err error) {
	address := addressFunc()
	pool, err = NewRedisPool(address, size, chanSize)
	if err != nil {
		return
	}
	pool.node.addressFunc = addressFunc
	return
}

func (pool *RedisPool) get() (conn *RedisCon) {
	conn = pool.node
	//	conn = pool.nodes[pool.cur%pool.size]
	//	if pool.cur++; pool.cur == pool.size {
	//		pool.cur = 0
	//	}
	return
}
func (pool *RedisPool) DoArg1(cmd string, arg interface{}) (interface{}, error) {
	switch argret := arg.(type) {
	case string:
		if len(argret) > MAX_KEY_LEN {
			glog.Error("[redis] key 的长度超过限制 ", len(argret), ",", cmd, ",", argret)
			return nil, ErrorKeyTooLong
		}
	}
	conn := pool.get()
	if conn == nil {
		glog.Error("conn is nil")
		return nil, nil
	}
	c := make(chan *Result, 1)
	err := conn.sendToCmdChanArgsNum1(c, &cmd, arg)
	if err == nil {
		r := <-c
		close(c)
		return r.Data, r.Err
	}
	return nil, err
}
func (pool *RedisPool) DoArg2(cmd string, arg1 interface{}, arg2 interface{}) (interface{}, error) {
	switch argret := arg1.(type) {
	case string:
		if len(argret) > MAX_KEY_LEN {
			glog.Error("[redis] key 的长度超过限制 ", len(argret), ",", cmd, ",", argret)
			return nil, ErrorKeyTooLong
		}
	}
	conn := pool.get()
	if conn == nil {
		glog.Error("conn is nil")
		return nil, nil
	}
	c := make(chan *Result, 1)
	err := conn.sendToCmdChanArgsNum2(c, &cmd, arg1, arg2)
	if err == nil {
		r := <-c
		close(c)
		return r.Data, r.Err
	}
	return nil, err
}
func (pool *RedisPool) DoArg3(cmd string, arg1 interface{}, arg2 interface{}, arg3 interface{}) (interface{}, error) {
	switch argret := arg1.(type) {
	case string:
		if len(argret) > MAX_KEY_LEN {
			glog.Error("[redis] key 的长度超过限制 ", len(argret), ",", cmd, ",", argret)
			return nil, ErrorKeyTooLong
		}
	}
	conn := pool.get()
	if conn == nil {
		glog.Error("conn is nil")
		return nil, nil
	}
	c := make(chan *Result, 1)
	err := conn.sendToCmdChanArgsNum3(c, &cmd, arg1, arg2, arg3)
	if err == nil {
		r := <-c
		close(c)
		return r.Data, r.Err
	}
	return nil, err
}

func (pool *RedisPool) Do(cmd string, args ...interface{}) (interface{}, error) {
	if len(args) > 0 {
		switch argret := args[0].(type) {
		case string:
			if len(argret) > MAX_KEY_LEN {
				glog.Error("[redis] key 的长度超过限制 ", len(argret), ",", cmd, ",", argret)
				return nil, ErrorKeyTooLong
			}
		}
	}
	conn := pool.get()
	if conn == nil {
		glog.Error("conn is nil")
		return nil, nil
	}
	c := make(chan *Result, 1)
	err := conn.SendToConn(c, cmd, &args)
	if err == nil {
		r := <-c
		close(c)
		return r.Data, r.Err
	}
	return nil, err
}
func (pool *RedisPool) Send(cmd string, args ...interface{}) error {
	if len(args) > 0 {
		switch argret := args[0].(type) {
		case string:
			if len(argret) > MAX_KEY_LEN {
				glog.Error("[redis] key 的长度超过限制 ", len(argret), ",", cmd, ",", argret)
				return ErrorKeyTooLong
			}
		}
	}
	conn := pool.get()
	return conn.SendToConn(nil, cmd, &args)
}

func (pool *RedisPool) SendArg1(cmd string, arg interface{}) error {
	switch argret := arg.(type) {
	case string:
		if len(argret) > MAX_KEY_LEN {
			glog.Error("[redis] key 的长度超过限制 ", len(argret), ",", cmd, ",", argret)
			return ErrorKeyTooLong
		}
	}
	conn := pool.get()
	return conn.sendToCmdChanArgsNum1(nil, &cmd, arg)
}

func (pool *RedisPool) SendArg2(cmd string, arg1 interface{}, arg2 interface{}) error {
	switch arg := arg1.(type) {
	case string:
		if len(arg) > MAX_KEY_LEN {
			glog.Error("[redis] key 的长度超过限制 ", len(arg), ",", cmd, ",", arg)
			return ErrorKeyTooLong
		}
	}
	conn := pool.get()
	return conn.sendToCmdChanArgsNum2(nil, &cmd, arg1, arg2)
}

func (pool *RedisPool) SendArg3(cmd string, arg1 interface{}, arg2 interface{}, arg3 interface{}) error {
	switch arg := arg1.(type) {
	case string:
		if len(arg) > MAX_KEY_LEN {
			glog.Error("[redis] key 的长度超过限制 ", len(arg), ",", cmd, ",", arg)
			return ErrorKeyTooLong
		}
	}
	conn := pool.get()
	return conn.sendToCmdChanArgsNum3(nil, &cmd, arg1, arg2, arg3)
}

func (pool *RedisPool) GetSend() (cmds []*Command) {
	return
}

func (pool *RedisPool) SendSync(cmds *[]*Command, cmd string, args ...interface{}) {
	*cmds = append(*cmds, &Command{
		Cmd:  &cmd,
		Args: &args,
	})
}

func (pool *RedisPool) Flush(cmds []*Command) (result []interface{}, err []error) {
	size := len(cmds)
	c := make(chan *Result, size)
	for i := 0; i < size; i++ {
		cmds[i].ResultChan = c
	}
	conn := pool.get()
	conn.sendToCmdsChan(cmds)

	result = make([]interface{}, size)
	//fmt.Println("cmd len:", size)
	err = make([]error, size)
	for i := 0; i < size; i++ {
		r := <-c
		result[i] = r.Data
		err[i] = r.Err
	}
	return
}

func (pool *RedisPool) Script() {

}

func (pool *RedisPool) Destory() {
	for _, node := range pool.nodes {
		node.Close()
	}
}

// ztgame123654@tcp(192.168.124.130:6379)/1
func ParseRedisKey(address string) (string, string, string) {
	//	if !addrReg.MatchString(address) {
	//		return address, "", "0"
	//	}
	//	args := addrReg.FindStringSubmatch(address)
	//	if len(args) != 4 {
	//		glog.Error("[Redis] read config error!", address)
	//		return "", "", "0"
	//	}
	//	return args[2], args[1], args[3]

	if strings.LastIndex(address, "(") == -1 || strings.LastIndex(address, ")") == -1 || strings.LastIndex(address, "@") == -1 || strings.LastIndex(address, "/") == -1 {
		//glog.Error("[Redis] read config error!", address)
		return address, "", "0"
	}
	server := address[strings.LastIndex(address, "(")+1 : strings.LastIndex(address, ")")]
	pwd := address[0:strings.LastIndex(address, "@")]
	dbKey := address[strings.LastIndex(address, "/")+1:]
	//var servers []string
	//servers = strings.Split(server, "|")
	return server, pwd, dbKey
}

type chans struct {
	pools []*chanPool
	size  int
	cur   int
}

func NewChans(size, poolsize int) (cs *chans) {
	cs = &chans{size: size}
	for i := 0; i < size; i++ {
		cs.pools = append(cs.pools, newChanPool(poolsize))
	}
	return
}

func (cs *chans) get() (pool *chanPool) {
	pool = cs.pools[cs.cur%cs.size]
	if cs.cur++; cs.cur >= cs.size {
		cs.cur = 0
	}
	return
}

type chanPool struct {
	sync.Mutex
	chans []chan *Result
	max   int
}

func newChanPool(size int) (pool *chanPool) {
	pool = &chanPool{}
	for i := 0; i < size; i++ {
		c := make(chan *Result, 1)
		pool.chans = append(pool.chans, c)
	}
	return
}

func (cpool *chanPool) get() (c chan *Result) {
	cpool.Lock()
	last := len(cpool.chans) - 1
	if last >= 0 {
		c = cpool.chans[last]
		cpool.chans = cpool.chans[:last]
		cpool.Unlock()
		return
	}
	cpool.Unlock()
	//	cp := make(chan *Result, 1)
	//	c = &cp
	return make(chan *Result, 1)
}
func (cpool *chanPool) set(c chan *Result) {
	cpool.Lock()
	cpool.chans = append(cpool.chans, c)
	cpool.Unlock()
}
