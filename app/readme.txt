TRDP中的example/sendHello.c可测试发送多个comid的TRDP包

TADMA配置见scripts里, 支持comid 100-107

例如 sendHello -t 10.1.1.1 -c 107 -n 8 -s 30 > /debug.log 向10.1.1.1发送comid为100-107, 周期为30ms的trdp包, 将debug结果保存在/debug.log里
DEBUG显示了每种comid实际发送时间

一些函数分析:

tlp_publish 将发送的comid依次加入队列, 并初始化他们的首次发送时间
tlp_publish => pNewElement->interval & timeToGo

tlc_getInterval 获取等待时间, 直到需要接收报文或发送报文

vos_select 等待报文接收, 或发送时间, 任一满足

tlc_process 遍历接收和发送队列, 完成报文接收或发送

很多地方用到vos_gettime函数, 已更改为读取ptp时间, 这样和TADMA保持同步

本工程支持多种系统, linux属于POSIX, 相关底层接口在/src/vos/posix




