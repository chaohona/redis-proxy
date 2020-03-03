package utils

import (
	"strconv"
	"strings"
)

func IPToUInt32(ip string) uint32 {
	bits := strings.Split(ip, ".")
	if len(bits) != 4 {
		return 0
	}

	b0, _ := strconv.Atoi(bits[0])
	b1, _ := strconv.Atoi(bits[1])
	b2, _ := strconv.Atoi(bits[2])
	b3, _ := strconv.Atoi(bits[3])

	var sum uint32

	sum += uint32(b0) << 24
	sum += uint32(b1) << 16
	sum += uint32(b2) << 8
	sum += uint32(b3)

	return sum
}

func AddrToUint64(addr string) uint64 {
	addrs := strings.Split(addr, ":")
	if len(addrs) != 2 {
		return 0
	}
	intIp := IPToUInt32(addrs[0])
	if intIp == 0 {
		return 0
	}
	intPort, err := strconv.ParseUint(addrs[1], 10, 64)
	if err != nil {
		return 0
	}

	return uint64(intIp)<<32 | uint64(intPort)
}
