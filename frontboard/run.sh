#!/bin/bash

startwork()
{
        nohup $PWD/bin/gredis_webserver -config=./config/config.json &
        echo "ps x | grep \"gredis_webserver\""
        ps x|grep "gredis_webserver"
}

stopwork()
{
        SERVERLIST='gredis_webserver'

        for serv in $SERVERLIST
        do
                echo "stop $serv"
                ps aux|grep "/$serv"|sed -e "/grep/d"|awk '{print $2}'|xargs kill 2&>/dev/null
        while test -f run.sh
        do
            count=`ps x|grep -w $serv|sed -e '/grep/d'|wc -l`
            if [ $count -eq 0 ]; then
                break
            fi
            sleep 1
        done
        done
    echo "running server:"`ps x|grep "gredis_webserver -c"|sed -e '/grep/d'|wc -l`
}

echo "-------------------start-----------------------"

case $1 in
stop)
    stopwork
;;
start)
    startwork
;;
*)
    stopwork
    sleep 1
    startwork
;;
esac

echo "-------------------end-----------------------"
