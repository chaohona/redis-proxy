#!/bin/bash

SCRIPTDIR=$(cd $(dirname "${BASH_SOURCE[0]}") >/dev/null && pwd)
EXE_PATH=`pwd`
CONFIG_PATH=${SCRIPTDIR}/../conf/
REDIS_SERVER=${SCRIPTDIR}/redis-server
PID_PATH=${SCRIPTDIR}/../pid/
LOG_PATH=${SCRIPTDIR}/../log/

mkdir -p ${PID_PATH}
mkdir -p ${CONFIG_PATH}
mkdir -p ${LOG_PATH}

chmod +x $SCRIPTDIR/redis-server
chmod +x $SCRIPTDIR/killall

$SCRIPTDIR/killall redis-server

REDIS_IP=0.0.0.0
REDIS_PORT=60001
REDIS_PORT_END=60003
REDIS_NUM=3

if [ $# -eq 1 ]; then
	REDIS_PORT=$1
fi

if [ $# -gt 2 ]; then
	REDIS_IP=$1
	REDIS_PORT=$2
fi

if [ $# -eq 3 ]; then
	REDIS_NUM=$3
fi

if [ $REDIS_NUM -lt 1 ]; then
	REDIS_NUM=1
fi

let "REDIS_PORT_END=$REDIS_PORT+$REDIS_NUM"

echo "start $REDIS_NUM redis server, bind on ip $REDIS_IP, port ${REDIS_PORT}-${REDIS_PORT_END}"

LISTEN_PORT=$REDIS_PORT
START_IDX=0
while [ $START_IDX -lt $REDIS_NUM ]
do
		REDIS_CONFIG=${CONFIG_PATH}/redis.${LISTEN_PORT}.conf
		rm -rf ${REDIS_CONFIG}
		cp -rf ${SCRIPTDIR}/redis.conf.example ${REDIS_CONFIG}

		sed -i "s#SERVER_PORT#${LISTEN_PORT}#g" ${REDIS_CONFIG}
		sed -i "s#SERVER_IP#${REDIS_IP}#g" ${REDIS_CONFIG}
		sed -i "s#PID_PATH#${PID_PATH}#g" ${REDIS_CONFIG}
		sed -i "s#LOG_PATH#${LOG_PATH}#g" ${REDIS_CONFIG}
		sed -i "s#PWD_PATH#${EXE_PATH}#g" ${REDIS_CONFIG}
		${REDIS_SERVER} ${REDIS_CONFIG}
		let "LISTEN_PORT=$LISTEN_PORT+1"
		let "START_IDX=$START_IDX+1"
done

cd $EXE_PATH
