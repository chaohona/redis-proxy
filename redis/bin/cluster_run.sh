#!/bin/bash

chmod +x redis-server
chmod +x redis-cli
chmod +x redis-benchmark
killall redis-server
sleep 2
rm -rf dump.rdb
rm -rf ../data/cluster/*

mkdir ../conf/ ../data/ ../data/cluster/ ../log/

if [ $# = 0 ]; then
    REDIS_IP=0.0.0.0
else
    REDIS_IP=$1
fi
echo $REDIS_IP

for port in {1..4}
do
	rm -rf ../conf/redis_cluster${port}.conf
	cp -rf redis_cluster.conf.example ../conf/redis_cluster${port}.conf

	let "destport=20000+$port"
	echo $destport
	sed -i "s/SERVER_PORT/$destport/" ../conf/redis_cluster${port}.conf
	./redis-server ../conf/redis_cluster${port}.conf
done

sleep 3

./redis-cli -h $REDIS_IP -p 20001 cluster addslots {0..4095}
./redis-cli -h $REDIS_IP -p 20002 cluster addslots {4096..8191}
./redis-cli -h $REDIS_IP -p 20003 cluster addslots {8192..12287}
./redis-cli -h $REDIS_IP -p 20004 cluster addslots {12288..16383}
