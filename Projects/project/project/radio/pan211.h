/**************************************************************************
 * @file      pan211.c
 * @version   V2.2
 * $Revision: 1 $
 * $Date:     2025/03/31 $
 * @brief     The header file of pan211 driver
 * @code        ___               ___  _     _
 *             |  _ \ __ _ _ __  / ___| |__ (_)_ __
 *             | |_) / _` | '_ \| |   | '_ \| | '_ \
 *             |  __/ (_| | | | | |___| | | | | |_) |
 *             |_|   \__,_|_| |_|\____|_| |_|_| .__/
 *                                            |_|
 *              (C)2009-2025 PanChip
 * @endcode
 * @author    PanChip
 * @note
 * Copyright (C) 2025 Panchip Technology Corp. All rights reserved.
 *****************************************************************************/
#ifndef _PAN211_H_
#define _PAN211_H_

#include "main.h"
#include "../../app/base.h"
/**************************************************************************
 *   PAN211中断标志位定义
 **************************************************************************/
#define RF_IT_TX_IRQ         0x80 /* 发送完成中断 */
#define RF_IT_MAX_RT_IRQ     0x40 /* 增型型模式下达到最大传输次数产生发送失败中断 */
#define RF_IT_ADDR_ERR_IRQ   0x20 /* 地址错误中断 */
#define RF_IT_CRC_ERR_IRQ    0x10 /* CRC错误中断 */
#define RF_IT_LEN_ERR_IRQ    0x08 /* 数据长度错误中断 */
#define RF_IT_PID_ERR_IRQ    0x04 /* 增型型模式下接收到错误PID中断 */
#define RF_IT_RX_TIMEOUT_IRQ 0x02 /* 接收超时中断 */
#define RF_IT_RX_IRQ         0x01 /* 接收完成中断 */
#define RF_IT_ALL_IRQ        0xFF /* 所有中断标志 */

/**************************************************************************
 *   PAN211发射功率定义
 **************************************************************************/
#define PAN211_TXPWR_0dBm 0x00 /* 发射功率：0dBm */
#define PAN211_TXPWR_1dBm 0x01 /* 发射功率：1dBm */
#define PAN211_TXPWR_2dBm 0x02 /* 发射功率：2dBm */
#define PAN211_TXPWR_3dBm 0x03 /* 发射功率：3dBm */
#define PAN211_TXPWR_4dBm 0x04 /* 发射功率：4dBm */
#define PAN211_TXPWR_5dBm 0x05 /* 发射功率：5dBm */
#define PAN211_TXPWR_6dBm 0x06 /* 发射功率：6dBm */
#define PAN211_TXPWR_7dBm 0x07 /* 发射功率：7dBm */
#define PAN211_TXPWR_8dBm 0x08 /* 发射功率：8dBm */
#define PAN211_TXPWR_9dBm 0x09 /* 发射功率：9dBm */

/*************************************************************************
 *   寄存器读写操作
 **************************************************************************/
/**
 * @brief 写入单个字节到指定寄存器
 * @param Addr 要写入的寄存器地址
 * @param Value 要写入寄存器的单字节数据
 * @return 无
 */
void PAN211_WriteReg(unsigned char Addr, unsigned char Value);
/**
 * @brief 从指定的寄存器读取单个字节
 * @param Addr 要读取的寄存器地址
 * @return 读取到寄存器的单字节数据
 */
unsigned char PAN211_ReadReg(unsigned char Addr);
/**
 * @brief 连续写入多个PAN211寄存器的值
 * @param Addr 起始寄存器地址
 * @param Buffer 包含要写入数据的缓冲区
 * @param Len 要写入的字节数
 */
void PAN211_WriteRegs(unsigned char Addr, unsigned char *Buffer, unsigned char Len);
/**
 * @brief 连续读取多个PAN211寄存器的值
 * @param Addr 起始寄存器地址
 * @param Buffer 存储读取数据的缓冲区
 * @param Len 要读取的字节数
 */
void PAN211_ReadRegs(unsigned char Addr, unsigned char *Buffer, unsigned char Len);
/**
 * @brief 写入数据到PAN211 TX FIFO
 * @param Buffer 要写入FIFO的数据缓冲区
 * @param Size 要写入数据的字节数
 */
void PAN211_WriteFIFO(unsigned char *Buffer, unsigned char Size);
/**
 * @brief 从PAN211 RX FIFO读取数据
 * @param Buffer 存储从FIFO读取数据的缓冲区
 * @param Size 要读取数据的字节数
 */
void PAN211_ReadFIFO(unsigned char *Buffer, unsigned char Size);
/**
 * @brief 延时函数，单位为毫秒
 * @param Ms 延时的毫秒数,范围1-65535
 * @return 无
 */
void DelayMs(unsigned short Ms);

#define DELAY() \
    __NOP();    \
    __NOP();    \
    __NOP();    \
    __NOP();    \
    __NOP();

/**
 * @brief 检测是否有中断事件
 * @return 1: 有中断触发
 * @return 0: 没有中断触发
 */
unsigned char IRQDetected(void);

/*************************************************************************
 *  芯片状态操作
 **************************************************************************/
/**
 * @brief 初始化PAN211芯片
 * @return 1: 初始化成功
 * @return 0: 初始化失败
 */
unsigned char PAN211_Init(void);
/**
 * @brief 软复位PAN211芯片
 * @return 无
 */
void PAN211_SoftRst(void);
/**
 * @brief 让PAN211从空闲状态进入睡眠状态
 * @return 无
 */
void PAN211_EnterSleep(void);
/**
 * @brief 进入待机状态
 * @note 在调用此函数前请确保清除IRQ
 */
void PAN211_EnterStandby(void);
/**
 * @brief 让PAN211退出睡眠状态进入空闲状态
 * @return 无
 */
void PAN211_ExitSleep(void);
/**
 * @brief 让PAN211退出发射状态进入空闲状态
 * @return 无
 */
void PAN211_ExitTxMode(void);
/**
 * @brief 让PAN211退出接收状态进入空闲状态
 * @return 无
 */
void PAN211_ExitRxMode(void);
/**
 * @brief 让PAN211进入发射状态，开始发送数据
 * @return 无
 */
void PAN211_TxStart(void);
/**
 * @brief 让PAN211进入接收状态，开始接收数据
 * @return 无
 */
void PAN211_RxStart(void);

/*************************************************************************
 *   配置操作
 **************************************************************************/
/**
 * @brief 设置PAN211收发器的频率通道
 * @param Channel 期望的射频通道，范围从0到83
 * @return 无
 * @note 实际频率将是(2400 + Channel)MHz
 */
void PAN211_SetChannel(unsigned char Channel);
/**
 * @brief 设置PAN211收发器的地址宽度
 * @param AddrWidth 期望的地址宽度，范围从3到5
 *        - 01 3字节
 *        - 10 4字节
 *        - 11 5字节
 * @return 无
 * @note 该函数会自动设置接收地址和发送地址的宽度
 */
void PAN211_SetAddrWidth(unsigned char AddrWidth);
/**
 * @brief 设置PAN211收发器的静态接收地址
 * @param Pipe 要配置地址的通道，为PAN211_Pipe_t值之一
 * @param Addr 包含地址的缓冲区指针
 * @param Len 地址长度
 * @return 无
 * @note 通道可以是0到5之间的数字（接收通道）
 * @note 缓冲区长度必须等于收发器当前的地址宽度
 * @note 对于通道[2..5]，只写入地址的第一个字节，因为通道1-5共享四个最高有效地址字节
 */
#if 0
void PAN211_SetRxAddr(unsigned char Pipe, unsigned char *Addr, unsigned char Len);
#else
void PAN211_SetRxAddr(unsigned char *Addr, unsigned char Len);
#endif

/**
 * @brief 设置PAN211收发器的静态发送地址
 * @param Addr 包含地址的缓冲区指针
 * @param Len 地址长度
 * @return 无
 * @note 该函数会自动设置接收地址和发送地址的宽度
 */
void PAN211_SetTxAddr(unsigned char *Addr, unsigned char Len);
/**
 * @brief 获取可从RX FIFO读取的有效负载的通道号
 * @return 通道号（0-5），如果RX FIFO为空则返回0xFF
 */
unsigned char PAN211_GetRxPipeNum(void);
/**
 * @brief 设置ACK应答的通道号
 * @param AckPipeNum 要设置的ACK通道号，范围从0到5
 * @note 该函数用于设置接收数据包时的ACK应答通道号
 *       在增强型模式下，接收端在收到数据包后会自动发送ACK应答，
 *       发送ACK应答前，要先能过PAN211_GetRxPipeNum()函数获取到接收数据包的通道号，
 *       然后调用此函数设置ACK应答的通道号。
 */
void PAN211_SetAckPipeNum(unsigned char AckPipeNum);
/**
 * @brief 获取PAN211收发器的IRQ标志
 * @return STATUS寄存器的中断标志，可以是以下值的组合：
 *         - RF_IT_TX_IRQ         0x80  // 发送完成中断
 *         - RF_IT_MAX_RT_IRQ     0x40  // 发送失败中断
 *         - RF_IT_ADDR_ERR_IRQ   0x20  // 地址错误中断
 *         - RF_IT_CRC_ERR_IRQ    0x10  // CRC错误中断
 *         - RF_IT_LEN_ERR_IRQ    0x08  // 数据长度错误中断
 *         - RF_IT_PID_ERR_IRQ    0x04  // PID错误中断
 *         - RF_IT_RX_TIMEOUT_IRQ 0x02  // 接收超时中断
 *         - RF_IT_RX_IRQ         0x01  // 接收完成中断
 */
unsigned char PAN211_GetIRQFlags(void);
/**
 * @brief 清除PAN211收发器的待处理IRQ标志
 * @param Flags 要清除的IRQ标志，可以是以下值的组合：
 *         - RF_IT_TX_IRQ         0x80  // 发送完成中断
 *         - RF_IT_MAX_RT_IRQ     0x40  // 发送失败中断
 *         - RF_IT_ADDR_ERR_IRQ   0x20  // 地址错误中断
 *         - RF_IT_CRC_ERR_IRQ    0x10  // CRC错误中断
 *         - RF_IT_LEN_ERR_IRQ    0x08  // 数据长度错误中断
 *         - RF_IT_PID_ERR_IRQ    0x04  // PID错误中断
 *         - RF_IT_RX_TIMEOUT_IRQ 0x02  // 接收超时中断
 *         - RF_IT_RX_IRQ         0x01  // 接收完成中断
 */
void PAN211_ClearIRQFlags(unsigned char Flags);
/**
 * @brief 设置PAN211发射功率
 * @param TxPower 发射功率值，使用PAN211_TXPWR_*常量
 *        比如：PAN211_TXPWR_8dBm
 * @return 无
 */
void PAN211_SetWhiteInitVal(unsigned char Value);
/**
 * @brief 获取接收到的有效数据长度
 * @return 返回接收到的有效数据长度
 */
unsigned char PAN211_GetRecvLen(void);
/**
 * @brief 设置PAN211发射功率
 * @param TxPower 发射功率值，使用PAN211_TXPWR_*常量
 * @return 无
 * @note 该函数可由工具自动生成，用户可根据需要选择必要的功率项，以减少代码体积。
 */
void PAN211_SetTxPower(int8_t TxPower);

/*************************************************************************
 *   载波操作
 **************************************************************************/
/**
 * @brief 进入载波发射模式
 */
void PAN211_StartCarrierWave(void);
/**
 * @brief 退出载波发射模式
 */
void PAN211_ExitCarrierWave(void);

#endif
