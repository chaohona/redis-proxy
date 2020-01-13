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
   REDIS_IP=127.0.0.1
else
   REDIS_IP=$1
fi
echo $REDIS_IP

for port in {1..8}
do
	rm -rf ../conf/redis_cluster${port}.conf
	cp -rf redis_cluster.conf.example ../conf/redis_cluster${port}.conf

	let "destport=10000+$port"
	echo $destport
	sed -i "s/SERVER_PORT/$destport/" ../conf/redis_cluster${port}.conf
	./redis-server ../conf/redis_cluster${port}.conf
done

for port in {2..8}
do
	let "destport=10000+$port"
        echo "meet $destport"
	redis-cli -h $REDIS_IP -p 10001 cluster meet $REDIS_IP $destport
done

sleep 3

for port in {1..4}
do
	let "destport=10000+$port"
	cluster_id=`redis-cli -h $REDIS_IP -p $destport cluster myid`
	echo $cluster_id
	let "slave_port=10004+$port"
	redis-cli -h $REDIS_IP -p $slave_port cluster replicate $cluster_id
done

redis-cli -h $REDIS_IP -p 10001 cluster addslots {0..4095}
redis-cli -h $REDIS_IP -p 10002 cluster addslots {4096..8191}
redis-cli -h $REDIS_IP -p 10003 cluster addslots {8192..12287}
redis-cli -h $REDIS_IP -p 10004 cluster addslots {12288..16383}
