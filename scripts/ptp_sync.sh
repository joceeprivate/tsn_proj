#!/bin/sh

pid=`ps -ef |grep ptp4l |grep -v grep| awk '{print $1}'`
if [ -z "$pid" ]; then
    case $1 in
        "s")
            ptp4l -2 -E -H -i eth1 -i eth2 -p /dev/ptp0 -s -f /usr/sbin/ptp4l_slave_v2_l2.conf &
            echo "PTP4L run as slave-only mode.";;
        "m")
            ptp4l -2 -E -H -i eth1 -i eth2 -p /dev/ptp0 -f /usr/sbin/ptp4l_master_v2_l2.conf &
            #phc2sys -a -rr &
            echo "PTP4L run as normal mode.";;
        *)
            echo "No PTP4L running. Input s or m.";;
    esac
elif [ $1 = "k" ]; then
    kill -9 $pid
    echo "All PTP4L processes killed."
else
    echo "PTP4L already running. Kill it first."
fi

