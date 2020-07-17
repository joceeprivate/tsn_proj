# tsn_proj


app/misc_utils是tadma配置程序, app/Open-AVB/example中是udp收发测试程序, app/trdp/example/sendHello中是trdp发送测试程序
driver/xilinx是tsn驱动程序
recipes是bitbake配方

该版本驱动只支持trdp发送测试

将驱动复制到linux源文件目录的/driver/net/ethernet/xilinx下
将app/misc-utils和app/trdp复制到tsn-package/others下, 将配方中的meta-xilinx/recipes-app/trdp文件复制到sources下
如有覆盖记得备份原文件

使用命令bitbake -c compile linux-xlnx -f && bitbake -c assemble_fitimage linux-xlnx编译内核
使用命令bitbake trdp编译应用

将tsn-package/others/trdp/bld/output/linux-dbg/sendHello复制到开发板的/usr/sbin里


程序的基本原理是:
EP驱动将UDP包发送到ST队列,确保被TADMA读取
TADMA驱动将TRDP包按目的ip和Comid划分, 非TRDP包按mac与vid划分
TADMA配置应用按streams.cfg配置TADMA发送顺序

PTP同步校准PTP时钟, 可用ptptime查看
TRDP应用中使用校准的PTP时钟代替之前的系统时钟来规划发送, 读取时间函数为vos_gettime()



