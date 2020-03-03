#!/bin/bash

cd redis/bin/
. cluster_run.sh $1
cd ../..

sleep 5
. run.sh $1