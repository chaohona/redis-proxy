#!/bin/bash

cd redis/bin/
. cluster_run.sh $1
cd ../..

cp redis/data/cluster/nodes-10001.conf proxy/conf/cluster_conf.conf
killall gredis-proxy
cd proxy
rm -rf log.txt
./bin/gredis-proxy > log.txt 2>&1 &