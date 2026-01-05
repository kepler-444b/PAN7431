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
 *   PAN211�жϱ�־λ����
 **************************************************************************/
#define RF_IT_TX_IRQ         0x80 /* ��������ж� */
#define RF_IT_MAX_RT_IRQ     0x40 /* ������ģʽ�´ﵽ����������������ʧ���ж� */
#define RF_IT_ADDR_ERR_IRQ   0x20 /* ��ַ�����ж� */
#define RF_IT_CRC_ERR_IRQ    0x10 /* CRC�����ж� */
#define RF_IT_LEN_ERR_IRQ    0x08 /* ���ݳ��ȴ����ж� */
#define RF_IT_PID_ERR_IRQ    0x04 /* ������ģʽ�½��յ�����PID�ж� */
#define RF_IT_RX_TIMEOUT_IRQ 0x02 /* ���ճ�ʱ�ж� */
#define RF_IT_RX_IRQ         0x01 /* ��������ж� */
#define RF_IT_ALL_IRQ        0xFF /* �����жϱ�־ */

/**************************************************************************
 *   PAN211���书�ʶ���
 **************************************************************************/
#define PAN211_TXPWR_0dBm 0x00 /* ���书�ʣ�0dBm */
#define PAN211_TXPWR_1dBm 0x01 /* ���书�ʣ�1dBm */
#define PAN211_TXPWR_2dBm 0x02 /* ���书�ʣ�2dBm */
#define PAN211_TXPWR_3dBm 0x03 /* ���书�ʣ�3dBm */
#define PAN211_TXPWR_4dBm 0x04 /* ���书�ʣ�4dBm */
#define PAN211_TXPWR_5dBm 0x05 /* ���书�ʣ�5dBm */
#define PAN211_TXPWR_6dBm 0x06 /* ���书�ʣ�6dBm */
#define PAN211_TXPWR_7dBm 0x07 /* ���书�ʣ�7dBm */
#define PAN211_TXPWR_8dBm 0x08 /* ���书�ʣ�8dBm */
#define PAN211_TXPWR_9dBm 0x09 /* ���书�ʣ�9dBm */

/*************************************************************************
 *   �Ĵ�����д����
 **************************************************************************/
/**
 * @brief д�뵥���ֽڵ�ָ���Ĵ���
 * @param Addr Ҫд��ļĴ�����ַ
 * @param Value Ҫд��Ĵ����ĵ��ֽ�����
 * @return ��
 */
void PAN211_WriteReg(unsigned char Addr, unsigned char Value);
/**
 * @brief ��ָ���ļĴ�����ȡ�����ֽ�
 * @param Addr Ҫ��ȡ�ļĴ�����ַ
 * @return ��ȡ���Ĵ����ĵ��ֽ�����
 */
unsigned char PAN211_ReadReg(unsigned char Addr);
/**
 * @brief ����д����PAN211�Ĵ�����ֵ
 * @param Addr ��ʼ�Ĵ�����ַ
 * @param Buffer ����Ҫд�����ݵĻ�����
 * @param Len Ҫд����ֽ���
 */
void PAN211_WriteRegs(unsigned char Addr, unsigned char *Buffer, unsigned char Len);
/**
 * @brief ������ȡ���PAN211�Ĵ�����ֵ
 * @param Addr ��ʼ�Ĵ�����ַ
 * @param Buffer �洢��ȡ���ݵĻ�����
 * @param Len Ҫ��ȡ���ֽ���
 */
void PAN211_ReadRegs(unsigned char Addr, unsigned char *Buffer, unsigned char Len);
/**
 * @brief д�����ݵ�PAN211 TX FIFO
 * @param Buffer Ҫд��FIFO�����ݻ�����
 * @param Size Ҫд�����ݵ��ֽ���
 */
void PAN211_WriteFIFO(unsigned char *Buffer, unsigned char Size);
/**
 * @brief ��PAN211 RX FIFO��ȡ����
 * @param Buffer �洢��FIFO��ȡ���ݵĻ�����
 * @param Size Ҫ��ȡ���ݵ��ֽ���
 */
void PAN211_ReadFIFO(unsigned char *Buffer, unsigned char Size);
/**
 * @brief ��ʱ��������λΪ����
 * @param Ms ��ʱ�ĺ�����,��Χ1-65535
 * @return ��
 */
void DelayMs(unsigned short Ms);

// #define DELAY() \
//     __NOP();    \
//     __NOP();    \
//     __NOP();    \
//     __NOP();    \
//     __NOP();

/**
 * @brief ����Ƿ����ж��¼�
 * @return 1: ���жϴ���
 * @return 0: û���жϴ���
 */
unsigned char IRQDetected(void);

/*************************************************************************
 *  оƬ״̬����
 **************************************************************************/
/**
 * @brief ��ʼ��PAN211оƬ
 * @return 1: ��ʼ���ɹ�
 * @return 0: ��ʼ��ʧ��
 */
unsigned char PAN211_Init(void);
/**
 * @brief ����λPAN211оƬ
 * @return ��
 */
void PAN211_SoftRst(void);
/**
 * @brief ��PAN211�ӿ���״̬����˯��״̬
 * @return ��
 */
void PAN211_EnterSleep(void);
/**
 * @brief �������״̬
 * @note �ڵ��ô˺���ǰ��ȷ�����IRQ
 */
void PAN211_EnterStandby(void);
/**
 * @brief ��PAN211�˳�˯��״̬�������״̬
 * @return ��
 */
void PAN211_ExitSleep(void);
/**
 * @brief ��PAN211�˳�����״̬�������״̬
 * @return ��
 */
void PAN211_ExitTxMode(void);
/**
 * @brief ��PAN211�˳�����״̬�������״̬
 * @return ��
 */
void PAN211_ExitRxMode(void);
/**
 * @brief ��PAN211���뷢��״̬����ʼ��������
 * @return ��
 */
void PAN211_TxStart(void);
/**
 * @brief ��PAN211�������״̬����ʼ��������
 * @return ��
 */
void PAN211_RxStart(void);

/*************************************************************************
 *   ���ò���
 **************************************************************************/
/**
 * @brief ����PAN211�շ�����Ƶ��ͨ��
 * @param Channel ��������Ƶͨ������Χ��0��83
 * @return ��
 * @note ʵ��Ƶ�ʽ���(2400 + Channel)MHz
 */
void PAN211_SetChannel(unsigned char Channel);
/**
 * @brief ����PAN211�շ����ĵ�ַ����
 * @param AddrWidth �����ĵ�ַ���ȣ���Χ��3��5
 *        - 01 3�ֽ�
 *        - 10 4�ֽ�
 *        - 11 5�ֽ�
 * @return ��
 * @note �ú������Զ����ý��յ�ַ�ͷ��͵�ַ�Ŀ���
 */
void PAN211_SetAddrWidth(unsigned char AddrWidth);
/**
 * @brief ����PAN211�շ����ľ�̬���յ�ַ
 * @param Pipe Ҫ���õ�ַ��ͨ����ΪPAN211_Pipe_tֵ֮һ
 * @param Addr ������ַ�Ļ�����ָ��
 * @param Len ��ַ����
 * @return ��
 * @note ͨ��������0��5֮������֣�����ͨ����
 * @note ���������ȱ�������շ�����ǰ�ĵ�ַ����
 * @note ����ͨ��[2..5]��ֻд���ַ�ĵ�һ���ֽڣ���Ϊͨ��1-5�����ĸ������Ч��ַ�ֽ�
 */
#if 0
void PAN211_SetRxAddr(unsigned char Pipe, unsigned char *Addr, unsigned char Len);
#else
void PAN211_SetRxAddr(unsigned char *Addr, unsigned char Len);
#endif

/**
 * @brief ����PAN211�շ����ľ�̬���͵�ַ
 * @param Addr ������ַ�Ļ�����ָ��
 * @param Len ��ַ����
 * @return ��
 * @note �ú������Զ����ý��յ�ַ�ͷ��͵�ַ�Ŀ���
 */
void PAN211_SetTxAddr(unsigned char *Addr, unsigned char Len);
/**
 * @brief ��ȡ�ɴ�RX FIFO��ȡ����Ч���ص�ͨ����
 * @return ͨ���ţ�0-5�������RX FIFOΪ���򷵻�0xFF
 */
unsigned char PAN211_GetRxPipeNum(void);
/**
 * @brief ����ACKӦ���ͨ����
 * @param AckPipeNum Ҫ���õ�ACKͨ���ţ���Χ��0��5
 * @note �ú����������ý������ݰ�ʱ��ACKӦ��ͨ����
 *       ����ǿ��ģʽ�£����ն����յ����ݰ�����Զ�����ACKӦ��
 *       ����ACKӦ��ǰ��Ҫ���ܹ�PAN211_GetRxPipeNum()������ȡ���������ݰ���ͨ���ţ�
 *       Ȼ����ô˺�������ACKӦ���ͨ���š�
 */
void PAN211_SetAckPipeNum(unsigned char AckPipeNum);
/**
 * @brief ��ȡPAN211�շ�����IRQ��־
 * @return STATUS�Ĵ������жϱ�־������������ֵ����ϣ�
 *         - RF_IT_TX_IRQ         0x80  // ��������ж�
 *         - RF_IT_MAX_RT_IRQ     0x40  // ����ʧ���ж�
 *         - RF_IT_ADDR_ERR_IRQ   0x20  // ��ַ�����ж�
 *         - RF_IT_CRC_ERR_IRQ    0x10  // CRC�����ж�
 *         - RF_IT_LEN_ERR_IRQ    0x08  // ���ݳ��ȴ����ж�
 *         - RF_IT_PID_ERR_IRQ    0x04  // PID�����ж�
 *         - RF_IT_RX_TIMEOUT_IRQ 0x02  // ���ճ�ʱ�ж�
 *         - RF_IT_RX_IRQ         0x01  // ��������ж�
 */
unsigned char PAN211_GetIRQFlags(void);
/**
 * @brief ���PAN211�շ����Ĵ�����IRQ��־
 * @param Flags Ҫ�����IRQ��־������������ֵ����ϣ�
 *         - RF_IT_TX_IRQ         0x80  // ��������ж�
 *         - RF_IT_MAX_RT_IRQ     0x40  // ����ʧ���ж�
 *         - RF_IT_ADDR_ERR_IRQ   0x20  // ��ַ�����ж�
 *         - RF_IT_CRC_ERR_IRQ    0x10  // CRC�����ж�
 *         - RF_IT_LEN_ERR_IRQ    0x08  // ���ݳ��ȴ����ж�
 *         - RF_IT_PID_ERR_IRQ    0x04  // PID�����ж�
 *         - RF_IT_RX_TIMEOUT_IRQ 0x02  // ���ճ�ʱ�ж�
 *         - RF_IT_RX_IRQ         0x01  // ��������ж�
 */
void PAN211_ClearIRQFlags(unsigned char Flags);
/**
 * @brief ����PAN211���书��
 * @param TxPower ���书��ֵ��ʹ��PAN211_TXPWR_*����
 *        ���磺PAN211_TXPWR_8dBm
 * @return ��
 */
void PAN211_SetWhiteInitVal(unsigned char Value);
/**
 * @brief ��ȡ���յ�����Ч���ݳ���
 * @return ���ؽ��յ�����Ч���ݳ���
 */
unsigned char PAN211_GetRecvLen(void);
/**
 * @brief ����PAN211���书��
 * @param TxPower ���书��ֵ��ʹ��PAN211_TXPWR_*����
 * @return ��
 * @note �ú������ɹ����Զ����ɣ��û��ɸ�����Ҫѡ���Ҫ�Ĺ�����Լ��ٴ��������
 */
void PAN211_SetTxPower(int8_t TxPower);

/*************************************************************************
 *   �ز�����
 **************************************************************************/
/**
 * @brief �����ز�����ģʽ
 */
void PAN211_StartCarrierWave(void);
/**
 * @brief �˳��ز�����ģʽ
 */
void PAN211_ExitCarrierWave(void);

#endif
