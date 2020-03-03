package redis

import (
	"bufio"
	"errors"
	"fmt"
	"math/rand"
	"strconv"

	"github.com/mediocregopher/radix/v3"
	"github.com/mediocregopher/radix/v3/resp/resp2"
)

// RadixDriver the Redis service based on the radix go client,
// contains the config and the redis pool.
type RadixDriver struct {
	// Connected is true when the Service has already connected
	Connected bool
	// Config the read-only redis database config.
	Config Config
	pool   *radix.Pool
}

// Connect connects to the redis, called only once
func (r *RadixDriver) Connect(c Config) error {
	if c.Timeout < 0 {
		c.Timeout = DefaultRedisTimeout
	}

	if c.Network == "" {
		c.Network = DefaultRedisNetwork
	}

	if c.Addr == "" {
		c.Addr = DefaultRedisAddr
	}

	if c.MaxActive == 0 {
		c.MaxActive = 10
	}

	if c.Delim == "" {
		c.Delim = DefaultDelim
	}

	var options []radix.DialOpt

	if c.Password != "" {
		options = append(options, radix.DialAuthPass(c.Password))
	}

	if c.Timeout > 0 {
		options = append(options, radix.DialTimeout(c.Timeout))
	}

	if c.Database != "" { //  *dialOpts.selectDb is not exported on the 3rd-party library,
		// but on its `DialSelectDB` option it does this:
		// do.selectDB = strconv.Itoa(db) -> (string to int)
		// so we can pass that string as int and it should work.
		dbIndex, err := strconv.Atoi(c.Database)
		if err == nil {
			options = append(options, radix.DialSelectDB(dbIndex))
		}

	}

	var connFunc radix.ConnFunc

	if len(c.Clusters) > 0 {
		cluster, err := radix.NewCluster(c.Clusters)
		if err != nil {
			// maybe an
			// ERR This instance has cluster support disabled
			return err
		}

		connFunc = func(network, addr string) (radix.Conn, error) {
			topo := cluster.Topo()
			node := topo[rand.Intn(len(topo))]
			return radix.Dial(c.Network, node.Addr, options...)
		}
	} else {
		connFunc = func(network, addr string) (radix.Conn, error) {
			return radix.Dial(c.Network, c.Addr, options...)
		}
	}

	pool, err := radix.NewPool(c.Network, c.Addr, c.MaxActive, radix.PoolConnFunc(connFunc))
	if err != nil {
		return err
	}

	r.Connected = true
	r.pool = pool
	r.Config = c
	return nil
}

// PingPong sends a ping and receives a pong, if no pong received then returns false and filled error
func (r *RadixDriver) PingPong() (bool, error) {
	var msg string
	err := r.pool.Do(radix.Cmd(&msg, "PING"))
	if err != nil {
		return false, err
	}
	return (msg == "PONG"), nil
}

// CloseConnection closes the redis connection.
func (r *RadixDriver) CloseConnection() error {
	if r.pool != nil {
		return r.pool.Close()
	}
	return ErrRedisClosed
}

// Set sets a key-value to the redis store.
// The expiration is setted by the secondsLifetime.
func (r *RadixDriver) Set(key string, value interface{}, secondsLifetime int64) error {
	// fmt.Printf("%#+v. %T. %s\n", value, value, value)

	// if vB, ok := value.([]byte); ok && secondsLifetime <= 0 {
	// 	return r.pool.Do(radix.Cmd(nil, "MSET", r.Config.Prefix+key, string(vB)))
	// }

	var cmd radix.CmdAction
	// if has expiration, then use the "EX" to delete the key automatically.
	if secondsLifetime > 0 {
		cmd = radix.FlatCmd(nil, "SETEX", r.Config.Prefix+key, secondsLifetime, value)
	} else {
		cmd = radix.FlatCmd(nil, "SET", r.Config.Prefix+key, value) // MSET same performance...
	}

	return r.pool.Do(cmd)
}

// Get returns value, err by its key
// returns nil and a filled error if something bad happened.
func (r *RadixDriver) Get(key string) (interface{}, error) {
	var redisVal interface{}
	mn := radix.MaybeNil{Rcv: &redisVal}

	err := r.pool.Do(radix.Cmd(&mn, "GET", r.Config.Prefix+key))
	if err != nil {
		return nil, err
	}
	if mn.Nil {
		return nil, fmt.Errorf("%s: %w", key, ErrKeyNotFound)
	}
	return redisVal, nil
}

// TTL returns the seconds to expire, if the key has expiration and error if action failed.
// Read more at: https://redis.io/commands/ttl
func (r *RadixDriver) TTL(key string) (seconds int64, hasExpiration bool, found bool) {
	var redisVal interface{}
	err := r.pool.Do(radix.Cmd(&redisVal, "TTL", r.Config.Prefix+key))
	if err != nil {
		return -2, false, false
	}
	seconds = redisVal.(int64)
	// if -1 means the key has unlimited life time.
	hasExpiration = seconds > -1
	// if -2 means key does not exist.
	found = seconds != -2
	return
}

func (r *RadixDriver) updateTTLConn(key string, newSecondsLifeTime int64) error {
	var reply int
	err := r.pool.Do(radix.FlatCmd(&reply, "EXPIRE", r.Config.Prefix+key, newSecondsLifeTime))
	if err != nil {
		return err
	}

	// https://redis.io/commands/expire#return-value
	//
	// 1 if the timeout was set.
	// 0 if key does not exist.

	if reply == 1 {
		return nil
	} else if reply == 0 {
		return fmt.Errorf("unable to update expiration, the key '%s' was stored without ttl", key)
	} // do not check for -1.

	return nil
}

// UpdateTTL will update the ttl of a key.
// Using the "EXPIRE" command.
// Read more at: https://redis.io/commands/expire#refreshing-expires
func (r *RadixDriver) UpdateTTL(key string, newSecondsLifeTime int64) error {
	return r.updateTTLConn(key, newSecondsLifeTime)
}

// UpdateTTLMany like `UpdateTTL` but for all keys starting with that "prefix",
// it is a bit faster operation if you need to update all sessions keys (although it can be even faster if we used hash but this will limit other features),
// look the `sessions/Database#OnUpdateExpiration` for example.
func (r *RadixDriver) UpdateTTLMany(prefix string, newSecondsLifeTime int64) error {
	keys, err := r.getKeys("0", prefix)
	if err != nil {
		return err
	}

	for _, key := range keys {
		if err = r.updateTTLConn(key, newSecondsLifeTime); err != nil { // fail on first error.
			return err
		}
	}

	return err
}

// GetAll returns all redis entries using the "SCAN" command (2.8+).
func (r *RadixDriver) GetAll() (interface{}, error) {
	var redisVal []interface{}
	mn := radix.MaybeNil{Rcv: &redisVal}
	err := r.pool.Do(radix.Cmd(&mn, "SCAN", strconv.Itoa(0))) // 0 -> cursor
	if err != nil {
		return nil, err
	}

	if mn.Nil {
		return nil, err
	}

	return redisVal, nil
}

type scanResult struct {
	cur  string
	keys []string
}

func (s *scanResult) UnmarshalRESP(br *bufio.Reader) error {
	var ah resp2.ArrayHeader
	if err := ah.UnmarshalRESP(br); err != nil {
		return err
	} else if ah.N != 2 {
		return errors.New("not enough parts returned")
	}

	var c resp2.BulkString
	if err := c.UnmarshalRESP(br); err != nil {
		return err
	}

	s.cur = c.S
	s.keys = s.keys[:0]

	return (resp2.Any{I: &s.keys}).UnmarshalRESP(br)
}

func (r *RadixDriver) getKeys(cursor, prefix string) ([]string, error) {
	var res scanResult
	err := r.pool.Do(radix.Cmd(&res, "SCAN", cursor, "MATCH", r.Config.Prefix+prefix+"*", "COUNT", "300000"))
	if err != nil {
		return nil, err
	}

	keys := res.keys[0:]
	if res.cur != "0" {
		moreKeys, err := r.getKeys(res.cur, prefix)
		if err != nil {
			return nil, err
		}

		keys = append(keys, moreKeys...)
	}

	return keys, nil
}

// GetKeys returns all redis keys using the "SCAN" with MATCH command.
// Read more at:  https://redis.io/commands/scan#the-match-option.
func (r *RadixDriver) GetKeys(prefix string) ([]string, error) {
	return r.getKeys("0", prefix)
}

// // GetBytes returns bytes representation of a value based on given "key".
// func (r *Service) GetBytes(key string) ([]byte, error) {
// 	var redisVal []byte
// 	mn := radix.MaybeNil{Rcv: &redisVal}
// 	err := r.pool.Do(radix.Cmd(&mn, "GET", r.Config.Prefix+key))
// 	if err != nil {
// 		return nil, err
// 	}
// 	if mn.Nil {
// 		return nil, ErrKeyNotFound.Format(key)
// 	}

// 	return redisVal, nil
// }

// Delete removes redis entry by specific key
func (r *RadixDriver) Delete(key string) error {
	err := r.pool.Do(radix.Cmd(nil, "DEL", r.Config.Prefix+key))
	return err
}
