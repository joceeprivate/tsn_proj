TRDP中的example/sendHello.c可测试发送多个comid的TRDP包

一些函数分析:

tlp_publish 将发送的comid依次加入队列, 并初始化他们的首次发送时间
tlp_publish => pNewElement->interval & timeToGo

tlc_getInterval 获取等待时间, 直到需要接收报文或发送报文

vos_select 等待报文接收, 或发送时间, 任一满足

tlc_process 遍历接收和发送队列, 完成报文接收或发送

很多地方用到vos_gettime函数, 已更改为读取ptp时间, 这样和TADMA保持同步

本工程支持多种系统, linux属于POSIX, 相关底层接口在/src/vos/posix



使用bitbake -c install trdp 编译trdp

将bld/output/linux-dbg/sendHello 复制到开发板/usr/sbin下

测试命令:
qbv_sched ep

sendHello -t 10.1.1.1 -c 10000 -n 128 -s 30000 -d 1430    COMID 10000-10127 所有COMID周期30ms

or

sendHello -t 10.1.1.1 -c 10000 -n 128 -d 1430 -v  COMID 10000-10127 周期为30/60/100/250 每32个一变

最大支持128个COMID, 周期大于30ms




