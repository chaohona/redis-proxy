#!/bin/bash

REDIS_PORT1=6381
REDIS_PORT2=6382
REDIS_PORT3=6383

cd redis/bin/
sh tiny_run.sh $1
cd ../..


cd proxy
tiny_conf="conf/tiny.yml"
echo "#" > conf/tiny.yml

redis_pool=""

LISTEN_PORT=64000
for num in {0..128}
do
	let "LISTEN_PORT=$LISTEN_PORT+1"
	echo "server$num:" >> ${tiny_conf}
	echo "  listen: 127.0.0.1:${LISTEN_PORT}" >> ${tiny_conf}
	echo "  servers:" >> ${tiny_conf}
	echo "    - 127.0.0.1:${REDIS_PORT1}@${num}" >> ${tiny_conf}
	echo "    - 127.0.0.1:${REDIS_PORT2}@${num}" >> ${tiny_conf}
	echo "    - 127.0.0.1:${REDIS_PORT3}@${num}" >> ${tiny_conf}
	echo "" >> ${tiny_conf}
	redis_pool=${redis_pool}" 127.0.0.1:${LISTEN_PORT}"
done

cd ..

chmod +x redis/bin/redis-cli
redis/bin/redis-cli -p 6380 lpush redis_pools ${redis_pool}

sleep 1

chmod +x redis/bin/killall
redis/bin/killall gredis-proxy
cd proxy
rm -rf log.txt
chmod +x bin/gredis-proxy
sed -i "s/PROXY_MODE/tiny/" ./bin/gredis.yml
nohup ./bin/gredis-proxy > log.txt 2>&1 &
cd ..

sleep 1

base_path=`pwd`
cd frontboard
cp -f config/config.json.example config/config.json
sed -i "s#BASE_PATH#${base_path}#g" config/config.json

mkdir log
chmod +x run.sh
sh ./run.sh
cd ..