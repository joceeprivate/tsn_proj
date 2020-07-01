#!/bin/sh

if [ $# -ne 3 ]; then
    echo "Wrong No. of input parameters. Should be 3."
    exit
fi

if [ $1 -ge 0 ] && [ $1 -le 9 ]; then
    sed -i "s/\([.:]\)[0-9]\([0def]\)$/\1$1\2/g" /etc/network/interfaces
    ifconfig eth0 down
    ifconfig ep down
    ifconfig eth1 down
    ifconfig eth2 down
    ifconfig eth0 hw ether 00:0a:35:00:01:"$1"0
    ifconfig ep hw ether 00:0a:35:00:01:"$1"d
    ifconfig ep 10.1.1."$1"0 netmask 255.255.255.0
    ifconfig eth1 hw ether 00:0a:35:00:01:"$1"e
    ifconfig eth2 hw ether 00:0a:35:00:01:"$1"f
    ifconfig eth0 up
    ifconfig ep up
    ifconfig eth1 up
    ifconfig eth2 up
    devmem 0x8007800c 32 0x350001"$1"0
    devmem 0x80078010 32 0xf000a
    echo "##### Board $1 IP & MAC config done. #####"
else
    echo "Wrong board num. Should be 0-9."
    exit
fi

switch_prog pst -s swp0 --state=4
switch_prog pst -s swp1 --state=0
switch_prog pst -s swp2 --state=0
if [ $2 -ne 0 ]; then
    devmem 0x80078048 32 0xA
    switch_prog ale -s -l 1 -t 300
    switch_prog ale -s -l 1 -u
    echo "##### Address learning Enabled. #####"
else
    echo "##### Address learning Disabled. #####"
fi
switch_prog pst -s swp1 --state=1
switch_prog pst -s swp2 --state=1

bridge=`ifconfig |grep br0 |grep -v grep`
if [ -z "$bridge" ]; then
    if [ $3 -ne 0 ]; then
        ip link add name br0 type bridge
        ip link set dev eth1 master br0
        ip link set dev eth2 master br0
        ifconfig br0 up
        ip link set dev br0 type bridge stp_state 1
        echo "##### STP Enabled. ######"
    else
        switch_prog pst -s swp1 --state=2
        switch_prog pst -s swp2 --state=2
        switch_prog pst -s swp1 --state=3
        switch_prog pst -s swp2 --state=3
        switch_prog pst -s swp1 --state=4
        switch_prog pst -s swp2 --state=4
        echo "##### STP Disabled. ######"
    fi
elif [ $3 -eq 0 ]; then
    ip link set dev br0 type bridge stp_state 0
    ifconfig br0 down
    ip link delete dev br0 type bridge
    switch_prog pst -s swp1 --state=2
    switch_prog pst -s swp2 --state=2
    switch_prog pst -s swp1 --state=3
    switch_prog pst -s swp2 --state=3
    switch_prog pst -s swp1 --state=4
    switch_prog pst -s swp2 --state=4
    echo "##### STP Disabled. ######"
fi

tadma_prog ep off
qbv_sched ep off
echo "##### Switch config Done. #####"
