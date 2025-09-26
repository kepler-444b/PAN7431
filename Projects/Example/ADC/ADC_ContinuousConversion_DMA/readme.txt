================================================================================
                                样例使用说明
                             Sample Description
================================================================================
功能描述：
此样例演示了通过ADC触发DMA搬运ADC_DR中的数据，配置CH0(PA00)为ADC的模拟输入通道，
ADC触发DMA搬运采样数据，DMA搬运完成，在循环中打印搬运的数据。

Function descriptions:
This sample demonstrates the transfer of ADC data from ADC_DR using DMA 
triggered by ADC. It configures CH0(PA00) as the analog input channel for ADC. 
The ADC triggers DMA to transfer the sampled data. After DMA transfer 
completion, the transferred data is printed in a loop.
================================================================================
测试环境：
测试用板：PY32F030_STK
MDK版本： 5.28
IAR版本： 9.20
GCC版本： GNU Arm Embedded Toolchain 10.3-2021.10

Test environment:
Test board: PY32F030_STK
MDK Version: 5.28
IAR Version: 9.20
GCC Version: GNU Arm Embedded Toolchain 10.3-2021.10
================================================================================
使用步骤：
1. 编译下载程序到MCU，并运行；
2. DMA搬运完成ADC的采样数据，然后通过串口打印。

Example execution steps:
1.Compile and download the program to the MCU and run it.
2.DMA transfers the sampled ADC data, which is then printed via the serial port.
================================================================================
注意事项：
通过USB转TTL模块连接PC与STK板,STK板与USB转TTL模块的连线方式如下；
@PrintfConfigStart
STK板        USB转TTL模块
PA02(TX)  -->  RX
PA03(RX)  -->  TX
GND       -->  GND
UART配置为波特率115200，数据位8，停止位1，校验位None
@PrintfConfigEnd

Notes:
Connect the PC to the STK board through the USB to TTL module, and the connection
method between the STK board and the USB to TTL module is as follows:
@PrintfConfigStart
STK board USB to TTL module
PA02(TX)  -->  RX
PA03(RX)  -->  TX
GND       -->  GND
UART is configured as BaudRate 115200, data bit 8, stop bit 1, and parity None.
@PrintfConfigEnd
================================================================================