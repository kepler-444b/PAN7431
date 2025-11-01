/**************************************************************************
 * @file      pan211.c
 * @version   V2.2.3
 * $Revision: 1 $
 * $Date:     2025/06/14 $
 * @brief     pan211 driver API definition
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

/*-----------------------------------------------------------------------------------------------
 *                                   Configuration Parameters
 *-----------------------------------------------------------------------------------------------
 *   Name                 | Value                                   | Description
 * -----------------------|-----------------------------------------|----------------------------
 *   XTAL_FREQ            | XTAL_FREQ_32M                           | 晶振频率
 *   EN_AGC               | 1                                       | 是否使能AGC
 *   INTERFACE_MODE       | USE_SPI_3LINE                           | 接口模式
 *   ChipMode             | PAN211_CHIPMODE_XN297                   | 芯片工作模式
 *   WorkMode             | PAN211_WORKMODE_ENHANCE                 | 工作模式
 *   TxMode               | PAN211_TX_MODE_SINGLE                   | 发送模式
 *   RxMode               | PAN211_ENHANCE_RX_MODE_CONTINOUS        | 接收模式
 *   TxPower              | PAN211_TXPWR_9dBm                       | 发射功率
 *   Channel              | 78                                      | 工作频道
 *   DataRate             | PAN211_DR_250Kbps                       | 数据传输速率
 *   EnWhite              | 1                                       | 是否使能白化
 *   Crc                  | PAN211_CRC_2byte                        | CRC校验方式
 *   TxLen                | 64                                      | 发送数据长度
 *   RxLen                | 64                                      | 接收数据长度
 *   RxTimeoutUs          | 7000                                    | 接收超时时间(微秒)
 *   EnDPL                | 1                                       | 是否使能动态有效负载长度，仅在增强型模式下有效
 *   EnManuPid            | 0                                       | 是否使用手动包ID，仅在增强型模式下有效
 *   EnRxPlLenLimit       | 0                                       | 是否限制接收有效负载长度，仅在增强型模式下有效
 *   EnTxNoAck            | 1                                       | 发送是否不需要应答，仅在增强型模式下有效
 *   AddrWidth            | 5                                       | 发送地址宽度
 *   TxAddr               | [0xCC, 0xCC, 0xCC, 0xCC, 0xCC]          | 发送地址
 *   RxAddr               |                                         | 各接收通道使能状态及地址
 *                        | True, [0xcc, 0xcc, 0xcc, 0xcc, 0xcc]    | Pipe0接收通道使能状态及地址
 *                        | False, [0xc1, 0xcc, 0xcc, 0xcc, 0xcc]   | Pipe1接收通道使能状态及地址
 *                        | False, [0xc2, 0xcc, 0xcc, 0xcc, 0xcc]   | Pipe2接收通道使能状态及地址
 *                        | False, [0xc3, 0xcc, 0xcc, 0xcc, 0xcc]   | Pipe3接收通道使能状态及地址
 *                        | False, [0xc4, 0xcc, 0xcc, 0xcc, 0xcc]   | Pipe4接收通道使能状态及地址
 *                        | False, [0xc5, 0xcc, 0xcc, 0xcc, 0xcc]   | Pipe5接收通道使能状态及地址
 *   IOMUX_EN             | 0                                       | 复用中断引脚功能使能
 *   InterruptMask        |                                         | 复用中断引脚关联的中断事件
 *                        | RF_IT_TX_IRQ                            |
 *                        | RF_IT_MAX_RT_IRQ                        |
 *                        | RF_IT_ADDR_ERR_IRQ                      |
 *                        | RF_IT_CRC_ERR_IRQ                       |
 *                        | RF_IT_LEN_ERR_IRQ                       |
 *                        | RF_IT_PID_ERR_IRQ                       |
 *                        | RF_IT_RX_TIMEOUT_IRQ                    |
 *                        | RF_IT_RX_IRQ                            |
 *   PowerTable           |                                         | 可配置的功率档位列表
 *                        | PAN211_TXPWR_0dBm                       |
 *                        | PAN211_TXPWR_9dBm                       |
 *   RxGain               | 0                                       | 接收增益
 *   TxDevSelect          | 300K                                    | 发射频偏选择
 *-----------------------------------------------------------------------------------------------*/

#include "pan211.h"
// #pragma push
// #pragma O0
__attribute__((optnone))
/* ========================================================================== */
/*                            用户实现函数                                     */
/* ========================================================================== */
void
RF_Gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  /* Push-pull output */
    GPIO_InitStruct.Pull  = GPIO_PULLUP;          /* Enable pull-up */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; /* GPIO speed */
    /* GPIO Initialization */
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_3;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  /* Push-pull output */
    GPIO_InitStruct.Pull  = GPIO_PULLUP;          /* Enable pull-up */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; /* GPIO speed */
    /* GPIO Initialization */
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;      /* Input  Mode */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;          /* Enable pull-up */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; /* GPIO speed */
    /* GPIO Initialization */
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
}

/* TODO: 根据你的硬件实现：
 *   - SPI_CS引脚配置为推挽输出
 *   - SPI_SCK引脚配置为推挽输出
 *   - SPI_DATA引脚配置为推挽输出或输入模式，低功耗模式下须配置为输入带上拉电阻
 */
#define SPI_CS_HIGH     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET)   /* 将SPI_CS引脚拉高 */
#define SPI_CS_LOW      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET) /* 将SPI_CS引脚拉低 */
#define SPI_SCK_HIGH    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET)   /* 将SPI_SCK引脚拉高 */
#define SPI_SCK_LOW     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET) /* 将SPI_SCK引脚拉低 */
#define SPI_DATA_HIGH   HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, GPIO_PIN_SET)   /* 将SPI_DATA引脚拉高 */
#define SPI_DATA_LOW    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_3, GPIO_PIN_RESET) /* 将SPI_DATA引脚拉低 */
#define SPI_DATA_STATUS HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_1)                  /* 检测SPI_DATA引脚状态 */
#define IRQ_STATUS      HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_0)
#define DELAY() \
    __NOP();    \
    __NOP();    \
    __NOP();    \
    __NOP();    \
    __NOP();

unsigned char SPI_RW(unsigned char rw_data)
{
    unsigned char i;
    for (i = 0; i < 8; i++) {
        SPI_SCK_LOW;
        DELAY();
        if (rw_data & 0x80) {
            SPI_DATA_HIGH;
        } else {
            SPI_DATA_LOW;
        }
        rw_data = rw_data << 1;
        SPI_SCK_HIGH;
        DELAY();
        if (SPI_DATA_STATUS) {
            rw_data = rw_data | 0x01;
        }
    }
    SPI_SCK_LOW;
    DELAY();
    return rw_data;
}
/**
 * @brief SPI 写入单个字节
 * @param Value 要写入的单字节数据
 * @note 此接口为软件SPI实现，具体实现方式可能因MCU平台而异。
 * @note 调用该函数前，必须先调用 SPI_CS_LOW 函数拉低 CS 引脚。
 * @note 该函数会将 SPI_DATA 引脚设置为输出模式，并在传输完成后将其设置为输入模式。
 * @note PAN211 SPI 接口使用的是 3 线 SPI 模式。
 * @note PAN211 SPI 配置为：
 *       时钟极性：低电平有效
 *       时钟相位：第一个边沿采样数据
 *       数据传输顺序：MSB优先
 * @note 以发送0xCC数据为例，时序图如下：
 *      SPI_CS：  ____________________________________________________
 *      SPI_CLK:  ____|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__
 *      SPI_DATA: ___|￣￣￣￣￣￣￣￣￣￣|___________|￣￣￣￣￣￣￣￣￣￣￣|_____________
 *      BIT DATA:     1     1     0     0      1     1      0     0
 */
void SPI_WriteByte(unsigned char Value)
{
    SPI_RW(Value);
}

/**
 * @brief SPI 读取单个字节
 * @return unsigned char 读取的数据字节
 * @note 此接口为软件SPI实现，具体实现方式可能因MCU平台而异。
 * @note 调用该函数前，必须先调用 SPI_CS_LOW 函数拉低 CS 引脚。
 * @note 该函数会将 SPI_DATA 引脚设置为输出模式，并在传输完成后将其设置为输入模式。
 * @note PAN211 SPI 接口使用的是 3 线 SPI 模式。
 * @note PAN211 SPI 配置为：
 *       时钟极性：低电平有效
 *       时钟相位：第一个边沿采样数据
 *       数据传输顺序：MSB优先
 * @note 以发送0x33数据为例，时序图如下：
 *      SPI_CS：  ___________________________________________________
 *      SPI_CLK:  ___|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__|￣￣|__
 *      SPI_DATA: _____________|￣￣￣￣￣￣￣￣￣￣￣|___________|￣￣￣￣￣￣￣￣￣￣￣￣￣
 *      BIT DATA:    0     0     1     1     0     0     1     1
 */
unsigned char SPI_ReadByte(void)
{
    return SPI_RW(0x00);
}

/**
 * @brief 当开启 DATA 引脚复用中断功能后，在 SPI 空闲期间检测 DATA 引脚是否有中断事件，低电平有效。
 * @note DATA 引脚复用中断功能的开启方法是将 IRQ_DATA_MUX_EN（Page0 0x03[2]）置1。
 * @return 1: 有中断触发
 * @return 0: 没有中断触发
 */
unsigned char IRQDetected(void)
{
    return IRQ_STATUS == 0; /* SPI_DATA引脚状态为低电平表示IRQ触发 */
}

/**
 * @brief 毫秒级延时
 * @param Ms 需要延时的毫秒数,范围1-65535
 * @return 无
 * @note 该函数会调用 BSP_DelayMs 函数进行延时，具体实现方式可能因MCU平台而异。
 * @note 须保证BSP_DelayMs(1)的延时大于等于1ms
 */
void DelayMs(unsigned short Ms)
{
    /* TODO: 根据你的硬件实现 */
    HAL_Delay(Ms);
}

// #pragma pop

/* ========================================================================== */
/*                            PAN211 寄存器命令                                */
/* ========================================================================== */

/**
 * @brief 写入单个字节到指定寄存器
 * @param Addr 要写入的寄存器地址
 * @param Value 要写入寄存器的单字节数据
 * @return 无
 */
void PAN211_WriteReg(unsigned char Addr, unsigned char Value)
{
    SPI_CS_LOW;                          /* 拉低CS引脚，开始SPI传输 */
    SPI_WriteByte(((Addr << 1) | 0x01)); /* BIT7~1寄存器地址, BIT0=1表示写操作 */
    SPI_WriteByte(Value);                /* 写入数据到寄存器 */
    SPI_CS_HIGH;                         /* 拉高CS引脚，结束SPI传输 */
}

/**
 * @brief 从指定的寄存器读取单个字节
 * @param Addr 要读取的寄存器地址
 * @return unsigned char 从寄存器读取的值
 */
unsigned char PAN211_ReadReg(unsigned char Addr)
{
    unsigned char Value;
    SPI_CS_LOW;                        /* 拉低CS引脚，开始SPI传输 */
    SPI_WriteByte((Addr << 1) & 0xFE); /* BIT7~1寄存器地址, BIT0=0表示读操作 */
    Value = SPI_ReadByte();            /* 读取寄存器数据 */
    SPI_CS_HIGH;                       /* 拉高CS引脚，结束SPI传输 */
    return Value;
}

/**
 * @brief 连续写入多个PAN211寄存器的值
 * @param Addr 起始寄存器地址
 * @param Buffer 包含要写入数据的缓冲区
 * @param Len 要写入的字节数
 */
void PAN211_WriteRegs(unsigned char Addr, unsigned char *Buffer, unsigned char Len)
{
    SPI_CS_LOW;                          /* 拉低CS引脚，开始SPI传输 */
    SPI_WriteByte(((Addr << 1) | 0x01)); /* BIT7~1寄存器地址, BIT0=1表示写操作 */
    for (int i = 0; i < Len; i++) {
        SPI_WriteByte(Buffer[i]); /* 循环写入Buffer中的数据到寄存器 */
    }
    SPI_CS_HIGH; /* 拉高CS引脚，结束SPI传输 */
}

/**
 * @brief 连续读取多个PAN211寄存器的值
 * @param Addr 起始寄存器地址
 * @param Buffer 存储读取数据的缓冲区
 * @param Len 要读取的字节数
 */
void PAN211_ReadRegs(unsigned char Addr, unsigned char *Buffer, unsigned char Len)
{
    SPI_CS_LOW;                        /* 拉低CS引脚，开始SPI传输 */
    SPI_WriteByte((Addr << 1) & 0xFE); /* BIT7~1寄存器地址, BIT0=0表示读操作 */
    for (int i = 0; i < Len; i++) {
        Buffer[i] = SPI_ReadByte(); /* 循环读取寄存器数据到Buffer中 */
    }
    SPI_CS_HIGH; /* 拉高CS引脚，结束SPI传输 */
}

/**
 * @brief 写入数据到PAN211 TX FIFO
 * @param Buffer 要写入FIFO的数据缓冲区
 * @param Size 要写入数据的字节数
 */
void PAN211_WriteFIFO(unsigned char *Buffer, unsigned char Size)
{
    PAN211_WriteReg(0x02, 0x74);          /* 进入待机状态 */
    PAN211_WriteRegs(0x01, Buffer, Size); /* 其中0x01为TX FIFO寄存器，向0x01寄存器连续写入要发送的数据 */
}

/**
 * @brief 从数据FIFO读取数据
 * @param Buffer 存储从FIFO读取数据的缓冲区
 * @param Size 要读取的字节数
 */
void PAN211_ReadFIFO(unsigned char *Buffer, unsigned char Size)
{
    PAN211_ReadRegs(0x01, Buffer, Size); /* 其中0x01为TX FIFO寄存器，从0x01寄存器连续读取数据到Buffer中 */
}

/* ========================================================================== */
/*                      PAN211 初始化函数(由工具自动生成)                       */
/* ========================================================================== */

/**
 * @brief 上电后初始化PAN211收发器到STB3状态
 * @return 初始化是否成功，1表示成功，0表示失败
 * @note 下列代码由工具自动生成，未经Panchip公司确认请勿更改！
 */
unsigned char PAN211_Init(void)
{
    unsigned char Value_2, Value_4;

    RF_Gpio_Init();
    DelayMs(10);

    /* 1. SPI 或 IIC 接口初始化 */
    PAN211_WriteReg(0x00, 0x00);
    PAN211_WriteReg(0x04, 0x03);

    /* 2. 进入STB3状态 */ // 此一项不能用工具配置
    PAN211_WriteReg(0x02, 0x04);
    DelayMs(1); /* 延时1ms以上 */
    PAN211_WriteReg(0x02, 0x74);
    DelayMs(1);                  /* 延时1ms以上 */
    PAN211_WriteReg(0x03, 0x01); /* 软复位 */
    DelayMs(1);                  /* 延时1ms以上 */
    PAN211_WriteReg(0x03, 0x03); /* 释放软复位 */

    /* 3. 读取工厂校准值 */
    PAN211_WriteReg(0x00, 0x01);
    PAN211_WriteReg(0x05, 0x00);
    PAN211_WriteReg(0x04, 0x04);
    Value_2 = PAN211_ReadReg(0x04);
    PAN211_WriteReg(0x04, 0x08);
    Value_4 = PAN211_ReadReg(0x04);
    PAN211_WriteReg(0x05, 0x01);
    if ((Value_2 & 0x0F) != 1) {
        return 0; /* 初始化失败 */
    }
    PAN211_WriteReg(0x47, 0x83 | ((Value_2 >> 1) & 0x70));
    PAN211_WriteReg(0x43, 0x10 | (!(Value_2 & 0x10)));

    /* 4. 写入预配置page1寄存器 */
    PAN211_WriteReg(0x0A, 0x48);
    PAN211_WriteReg(0x32, 0x10);
    PAN211_WriteReg(0x33, 0x1C);
    PAN211_WriteReg(0x37, 0x15);
    PAN211_WriteReg(0x3A, 0x54);
    PAN211_WriteReg(0x3E, 0xF1);
    PAN211_WriteReg(0x41, 0xA2);
    PAN211_WriteReg(0x46, 0xB0);
    PAN211_WriteReg(0x49, 0x44);
    PAN211_WriteReg(0x4C, 0x48);

    /* 5. 写入预配置page0寄存器（与默认值相同的寄存器已被注释） */
    PAN211_WriteReg(0x00, 0x00);
    PAN211_WriteReg(0x05, (Value_4 >> 4) | 0xC0);
    PAN211_WriteReg(0x07, 0x8b); /* [7:6]Crc [5:4]WORK_MODE [3]EnWhiten [2]crcSkipAddr [1]EnTxNoAck [0]Endian */
    PAN211_WriteReg(0x08, 0xbb); /* [4]EnDPL [3]ENHANCE [1:0]AddrWidth */
    PAN211_WriteReg(0x09, 0x40); /* [7:0]RxLen */
    PAN211_WriteReg(0x0a, 0x40); /* [7:0]TxLen */
    // PAN211_WriteReg(0x0b, 0x00); /* [7:0]InterruptMask */
    // PAN211_WriteReg(0x0d, 0x00); /* [7:0]TRxDelayTimeUs[7:0] */
    // PAN211_WriteReg(0x0e, 0x00); /* [6:0]TRxDelayTimeUs[14:8] */
    // PAN211_WriteReg(0x0f, 0xcc); /* Pipe0-RxAddr[7:0] */
    // PAN211_WriteReg(0x10, 0xcc); /* Pipe0-RxAddr[15:8] */
    // PAN211_WriteReg(0x11, 0xcc); /* Pipe0-RxAddr[23:16] */
    // PAN211_WriteReg(0x12, 0xcc); /* Pipe0-RxAddr[31:24] */
    // PAN211_WriteReg(0x13, 0xcc); /* Pipe0-RxAddr[39:32] */
    // PAN211_WriteReg(0x14, 0xcc); /* TxAddr[7:0] */
    // PAN211_WriteReg(0x15, 0xcc); /* TxAddr[15:8] */
    // PAN211_WriteReg(0x16, 0xcc); /* TxAddr[23:16] */
    // PAN211_WriteReg(0x17, 0xcc); /* TxAddr[31:24] */
    // PAN211_WriteReg(0x18, 0xcc); /* TxAddr[39:32] */
    // PAN211_WriteReg(0x19, 0x00); /* [6]HDR_LEN_EXIST[5:4]BLEHeadNum */
    // PAN211_WriteReg(0x1a, 0x7f); /* [6:0]WhiteInitVal */
    // PAN211_WriteReg(0x1b, 0x00); /* [7:0]BLE Header */
    // PAN211_WriteReg(0x1f, 0x01); /* [5]EnRxPipe5 [4]EnRxPipe4 [3]EnRxPipe3 [2]EnRxPipe2 [1]EnRxPipe1 [0]EnRxPipe0 */
    // PAN211_WriteReg(0x20, 0xcc); /* Pipe1-RxAddr[7:0] */
    // PAN211_WriteReg(0x21, 0xcc); /* Pipe1-RxAddr[15:8] */
    // PAN211_WriteReg(0x22, 0xcc); /* Pipe1-RxAddr[23:16] */
    // PAN211_WriteReg(0x23, 0xcc); /* Pipe1-RxAddr[31:24] */
    // PAN211_WriteReg(0x24, 0xcc); /* Pipe1-RxAddr[39:32] */
    // PAN211_WriteReg(0x25, 0xcc); /* Pipe2-RxAddr[7:0] */
    // PAN211_WriteReg(0x26, 0xcc); /* Pipe3-RxAddr[7:0] */
    // PAN211_WriteReg(0x27, 0xcc); /* Pipe4-RxAddr[7:0] */
    // PAN211_WriteReg(0x28, 0xcc); /* Pipe5-RxAddr[7:0] */
    PAN211_WriteReg(0x29, 0x00); /* [7:4]AutoDelayUs [3:0]AutoMaxCnt */
    // PAN211_WriteReg(0x2a, 0x01); /* [7]TxMode [6:5]RxMode */
    PAN211_WriteReg(0x2b, 0x58); /* [7:0]RxTimeoutUs[7:0] */
    PAN211_WriteReg(0x2c, 0x1b); /* [7:0]RxTimeoutUs[15:8] */
    // PAN211_WriteReg(0x2d, 0x00); /* [6:4]BLEWhiteListMatchMode [3:2]BLELengthFilterMode */
    // PAN211_WriteReg(0x2f, 0x00); /* BLEWhiteList[7:0] */
    // PAN211_WriteReg(0x30, 0x00); /* BLEWhiteList[15:8] */
    // PAN211_WriteReg(0x31, 0x00); /* BLEWhiteList[23:16] */
    // PAN211_WriteReg(0x32, 0x00); /* BLEWhiteList[31:24] */
    // PAN211_WriteReg(0x33, 0x00); /* BLEWhiteList[39:32] */
    // PAN211_WriteReg(0x34, 0x00); /* BLEWhiteList[47:40] */
    // PAN211_WriteReg(0x35, 0x07); /* [5:0]BLEWhiteListOffset */
    PAN211_WriteReg(0x36, 0xb0);
    PAN211_WriteReg(0x37, 0x6b);
    PAN211_WriteReg(0x38, 0x4b);
    PAN211_WriteReg(0x39, 0x55); // 校准频点，请勿修改
    PAN211_WriteReg(0x43, 0x3b);
    PAN211_WriteReg(0x44, 0x8c);
    PAN211_WriteReg(0x55, 0xdd);
    PAN211_WriteReg(0x56, 0xc9);
    PAN211_WriteReg(0x57, 0xb7);
    PAN211_WriteReg(0x5a, 0x10);
    PAN211_WriteReg(0x5b, 0xfd);
    PAN211_WriteReg(0x5c, 0xe9);
    PAN211_WriteReg(0x5d, 0xdc);
    PAN211_WriteReg(0x5e, 0x02);
    PAN211_WriteReg(0x5f, 0x06);
    PAN211_WriteReg(0x60, 0x0e);
    PAN211_WriteReg(0x61, 0x2e);
    PAN211_WriteReg(0x66, 0x34);
    PAN211_WriteReg(0x68, 0x0d);
    PAN211_WriteReg(0x6e, 0x20);

    /* 6. 射频校准 */
    PAN211_WriteReg(0x00, 0x01);
    PAN211_WriteReg(0x1B, 0x08);
    while (((PAN211_ReadReg(0x70) & 0x40) != 0x40)); /* 或延时1ms以上 */
    PAN211_WriteReg(0x1B, 0x10);
    DelayMs(55); /* 延时55ms以上 */
    PAN211_WriteReg(0x02, 0x76);
    DelayMs(1); /* 延时200us以上 */
    PAN211_WriteReg(0x1B, 0x20);
    while (((PAN211_ReadReg(0x7F) & 0x80) != 0x80)); /* 或延时2ms以上 */
    PAN211_WriteReg(0x1B, 0x40);
    while (((PAN211_ReadReg(0x6D) & 0x80) != 0x80)); /* 或延时1ms以上 */
    PAN211_WriteReg(0x1B, 0x80);
    while (((PAN211_ReadReg(0x7F) & 0x80) != 0x80)); /* 或延时2ms以上 */
    PAN211_WriteReg(0x1B, 0x00);
    PAN211_WriteReg(0x02, 0x74);
    PAN211_WriteReg(0x00, 0x00);
    PAN211_WriteReg(0x73, 0xFF);

    PAN211_WriteReg(0x39, 0x4E); // Channel

    return 1; /* 初始化成功 */
}

/**
 * @brief PAN211收发器软复位（SOFT_RSTL）
 * @note 本函数必须在STB3状态调用。
 * @note 本函数会复位PAN211收发器内部逻辑，复位之后必须调用PAN211_Init函数重新初始化。
 * @note POR_RSTL负责低压区寄存器复位，SOFT_RSTL负责低压区逻辑复位，注意区分。
 */
void PAN211_SoftRst(void)
{
    PAN211_WriteReg(0x03, 0x00); /* 软复位 */
    DelayMs(1);                  /* 延时1ms以上 */
    PAN211_WriteReg(0x03, 0x02); /* 释放软复位 */
}

/**
 * @brief 进入待机状态
 * @note 在调用此函数前请确保清除IRQ
 */
void PAN211_EnterStandby(void)
{
    PAN211_WriteReg(0x02, 0x74); /* 进入待机状态 */
}

/**
 * @brief 退出发送状态并进入待机状态。在调用此函数前请确保清除IRQ
 */
void PAN211_ExitTxMode(void)
{
    PAN211_WriteReg(0x02, 0x74); /* 进入待机状态 */
}

/**
 * @brief 退出接收状态并进入待机状态
 * @note 在调用此函数前请确保清除IRQ
 */
void PAN211_ExitRxMode(void)
{
    PAN211_WriteReg(0x02, 0x74); /* 进入待机状态 */
}

/**
 * @brief 进入发送状态
 * @note 在进入发送状态前会先回到待机状态
 */
void PAN211_TxStart(void)
{
    PAN211_WriteReg(0x02, 0x74); /* 进入待机状态 */
    PAN211_WriteReg(0x73, 0xFF); /* 清除所有IRQ标志 */
    PAN211_WriteReg(0x02, 0x75); /* 进入发送状态 */
}

/**
 * @brief 退出接收状态并进入待机状态，在调用此函数前请确保清除IRQ
 */
void PAN211_RxStart(void)
{
    PAN211_WriteReg(0x02, 0x74); /* 进入待机状态 */
    PAN211_WriteReg(0x73, 0xFF); /* 清除所有IRQ标志 */
    PAN211_WriteReg(0x02, 0x76); /* 进入接收状态 */
}

/**
 * @brief 设置PAN211收发器的频率通道
 * @param Channel 期望的射频通道，范围从0到83
 * @note 实际频率将是(2400 + Channel)MHz
 */
void PAN211_SetChannel(unsigned char Channel)
{
    PAN211_WriteReg(0x39, Channel);
    PAN211_RxStart();
}

/**
 * @brief 设置PAN211收发器的地址宽度
 * @param AddrWidth 期望的地址宽度，范围从3到5
 *          - 01 3字节
 *          - 10 4字节
 *          - 11 5字节
 */
void PAN211_SetAddrWidth(unsigned char AddrWidth)
{
    unsigned char Value;
    Value = PAN211_ReadReg(0x08) & 0xFC; // 先清除原有的地址宽度设置，0x08[1:0]为地址宽度
    Value = Value + AddrWidth - 2;
    PAN211_WriteReg(0x08, Value);
}

/**
 * @brief 为指定通道设置静态接收地址
 * @param Pipe 要配置地址的通道，范围从0到5
 * @param Addr 包含地址的缓冲区指针
 * @param Len 地址长度
 * @note 缓冲区长度必须等于收发器当前的地址宽度
 * @note 对于通道[2..5]，只写入地址的第一个字节，因为通道1-5共享四个最高有效地址字节
 */
#if 0
void PAN211_SetRxAddr(unsigned char Pipe, unsigned char *Addr, unsigned char Len)
{
    switch (Pipe) {
        case 0:
            PAN211_WriteRegs(0x0F, Addr, Len); /* 配置通道0的地址 */
            break;
        case 1:
            PAN211_WriteRegs(0x20, Addr, Len); /* 配置通道1的地址 */
            break;
        case 2:
            PAN211_WriteReg(0x25, Addr[0]); /* 配置通道2的地址，Addr[1]~Addr[5]与通道1相同 */
            break;
        case 3:
            PAN211_WriteReg(0x26, Addr[0]); /* 配置通道3的地址，Addr[1]~Addr[5]与通道1相同 */
            break;
        case 4:
            PAN211_WriteReg(0x27, Addr[0]); /* 配置通道4的地址，Addr[1]~Addr[5]与通道1相同 */
            break;
        case 5:
            PAN211_WriteReg(0x28, Addr[0]); /* 配置通道5的地址，Addr[1]~Addr[5]与通道1相同 */
            break;
    }
}
#else

void PAN211_SetRxAddr(unsigned char *Addr, unsigned char Len)
{
    PAN211_WriteRegs(0x0F, Addr, Len); /* 配置通道0的地址 */
}
#endif

/**
 * @brief 设置收发器的静态发送地址
 * @param Addr 包含地址的缓冲区指针
 * @param Len 地址长度
 */
void PAN211_SetTxAddr(unsigned char *Addr, unsigned char Len)
{
    PAN211_WriteRegs(0x14, Addr, Len);
}

/**
 * @brief 获取可从RX FIFO读取的有效负载的通道号
 * @return 通道号（0-5），如果RX FIFO为空则返回0xFF
 */
unsigned char PAN211_GetRxPipeNum(void)
{
    return (PAN211_ReadReg(0x74) >> 4) & 0x07; /* 0x74[6:4]为通道号 */
}

/**
 * @brief 设置ACK应答的通道号
 * @param AckPipeNum 要设置的ACK通道号，范围从0到5
 * @note 该函数用于设置接收数据包时的ACK应答通道号
 *       在增强型模式下，接收端在收到数据包后会自动发送ACK应答，
 *       发送ACK应答前，要先能过PAN211_GetRxPipeNum()函数获取到接收数据包的通道号，
 *       然后调用此函数设置ACK应答的通道号。
 */
void PAN211_SetAckPipeNum(unsigned char AckPipeNum)
{
    PAN211_WriteReg(0x6F, AckPipeNum);
}

/**
 * @brief 获取待处理的IRQ标志
 * @return STATUS寄存器的RX_DONE、TX_DONE、RX_TIMEOUT和MAX_RT位的当前状态
 */
unsigned char PAN211_GetIRQFlags(void)
{
    return PAN211_ReadReg(0x73);
}

/**
 * @brief 清除PAN211收发器的待处理IRQ标志
 * @param Flags 要清除的IRQ标志，可以是以下值的组合：
 *         - RF_IT_TX_IRQ
 *         - RF_IT_MAX_RT_IRQ
 *         - RF_IT_ADDR_ERR_IRQ
 *         - RF_IT_CRC_ERR_IRQ
 *         - RF_IT_LEN_ERR_IRQ
 *         - RF_IT_PID_ERR_IRQ
 *         - RF_IT_RX_TIMEOUT_IRQ
 *         - RF_IT_RX_IRQ
 */
void PAN211_ClearIRQFlags(unsigned char Flags)
{
    PAN211_WriteReg(0x73, Flags);
}

/**
 * @brief 设置BLE白化初始值
 * @param Value 白化初始值
 */
void PAN211_SetWhiteInitVal(unsigned char Value)
{
    unsigned char Temp = PAN211_ReadReg(0x1A);
    PAN211_WriteReg(0x1A, (Temp & 0x80) | (Value & 0x7F));
}

/**
 * @brief 获取接收到的有效数据长度
 * @return 有效数据的长度
 */
unsigned char PAN211_GetRecvLen(void)
{
    return PAN211_ReadReg(0x77);
}
/**
 * @brief 从待机状态进入睡眠状态
 */
void PAN211_EnterSleep(void)
{
    PAN211_WriteReg(0x02, 0x74); /* 进入待机状态 */
    PAN211_WriteReg(0x02, 0x21); /* 进入睡眠状态 */
}

/**
 * @brief 退出睡眠状态并进入待机状态
 */
void PAN211_ExitSleep(void)
{
    PAN211_WriteReg(0x02, 0x22); /* 退出睡眠状态 */
    PAN211_WriteReg(0x02, 0x74); /* 进入待机状态 */
    DelayMs(1);                  /* 等待晶振稳定 */
}

/**
 * @brief 设置PAN211发射功率 (工具自动生成)
 *
 * @param TxPower 发射功率值，使用PAN211_TXPWR_*常量
 */
void PAN211_SetTxPower(int8_t TxPower)
{
    switch (TxPower) {
        case 0: /* PAN211_TXPWR_0dBm */
            PAN211_WriteReg(0x00, 0x01);
            PAN211_WriteReg(0x27, 0x0a);
            PAN211_WriteReg(0x3c, 0x13);
            PAN211_WriteReg(0x46, 0xbd);
            PAN211_WriteReg(0x48, 0x88);
            PAN211_WriteReg(0x00, 0x00);
            PAN211_WriteReg(0x43, 0x3b);
            PAN211_WriteReg(0x44, 0x84);
            break;

        case 9: /* PAN211_TXPWR_9dBm */
            PAN211_WriteReg(0x00, 0x01);
            PAN211_WriteReg(0x27, 0x0a);
            PAN211_WriteReg(0x3c, 0x17);
            PAN211_WriteReg(0x46, 0xb0);
            PAN211_WriteReg(0x48, 0x88);
            PAN211_WriteReg(0x00, 0x00);
            PAN211_WriteReg(0x43, 0x3b);
            PAN211_WriteReg(0x44, 0x8c);
            break;
    }
}

/**
 * @brief 进入载波发射模式（由工具自动生成）
 * @note 进入载波模式前可设置频点和发射功率
 *          - PAN211_SetChannel(12)
 *          - PAN211_SetTxPower(PAN211_TXPWR_9dBm)
 */
void PAN211_StartCarrierWave(void)
{
    PAN211_WriteReg(0x41, 0x45);
    PAN211_WriteReg(0x42, 0x2c);
    PAN211_WriteReg(0x2A, 0x81); /* 设置为连续发送模式 */
    PAN211_WriteReg(0x02, 0x75); /* 进入发送状态 */
}

/**
 * @brief 退出载波发射模式（由工具自动生成）
 */
void PAN211_ExitCarrierWave(void)
{
    PAN211_WriteReg(0x41, 0x00);
    PAN211_WriteReg(0x42, 0x00);
    PAN211_WriteReg(0x02, 0x74);
    PAN211_WriteReg(0x2A, 0x01);
}
