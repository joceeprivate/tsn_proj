#!/bin/sh

if [ $# -ne 1 ]; then
    echo "Enter the board Num. (0-9)"
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

tadma_prog ep off
qbv_sched ep off
echo "##### Switch config Done. #####"