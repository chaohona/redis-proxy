#!/bin/bash

if [ $# = 0 ]; then
    REDIS_IP=127.0.0.1
else
    REDIS_IP=$1
fi
echo $REDIS_IP

for port in {1..4}
do
	let "destport=10000+$port"
	echo $destport
	./redis-cli -p ${destport} -h ${REDIS_IP} flushall
done