#!/bin/bash

ps -ef | grep $1 | grep -v grep | awk '{print $2}' | xargs kill -9