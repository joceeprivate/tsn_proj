#!/bin/sh

if [ $# -ne 3 ]; then
    echo "Wrong No. of input parameters. Should be 3. COMID NUM CYCLE"
    exit
fi

qbv_sched ep

if [ $2 -le 128 ] && [ $3 -ge 30 ]; then
    transHello -t 10.1.1.1 -c $1 -n $2 -s `expr $3 * 1000` -d 1430 -v
else
    echo "Process num <128. Cycle time >=30."
fi

