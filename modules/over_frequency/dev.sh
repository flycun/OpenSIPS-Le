#!/bin/bash

case $1 in
    build)
        cd ../../
        pwd;
        make modules modules=modules/over_frequency
    ;;
    start)
        killall opensips
        ulimit -t unlimited
        sleep 1
        /usr/sbin/opensips -f ./dev.cfg -w . &> log.txt &
        echo $?
    ;;
    stop)
        killall opensips
        echo stop
    ;;
    *) echo bad;;
esac
