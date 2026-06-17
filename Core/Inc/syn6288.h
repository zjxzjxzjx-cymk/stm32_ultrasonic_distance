#ifndef __SYN6288_H__
#define __SYN6288_H__

#include "main.h"
#include "usart.h"

/* ================================================================
 * SYN6288 语音合成模块 - 头文件
 * 
 * 硬件：USART2 → SYN6288 (交叉连接)
 *       STM32 PA2(TX) → SYN6288 RXD
 *       STM32 PA3(RX) → SYN6288 TXD（可选）
 *       STM32 PA4      → SYN6288 BUSY（可选）
 * 
 * 通信参数：9600bps, 8数据位, 1停止位, 无校验
 * 
 * 知识点：
 *   1. UART 异步串行通信：不需要时钟线，双方约定好波特率即可
 *   2. GB2312 编码：中文字符每个占 2 字节
 *   3. 通信协议帧：帧头 + 数据长度 + 命令 + 数据 + 校验
 *   4. XOR 校验：把所有数据字节异或，用于检测传输错误
 * ================================================================ */

/* ---- SYN6288 BUSY 引脚（可选，不接则用延时等待）---- */
#define SYN6288_BUSY_PORT   GPIOA
#define SYN6288_BUSY_PIN    GPIO_PIN_4

/* ---- 播报模式 ---- */
#define SYN6288_SPEAK_NORMAL    0   /* 正常播报 */
#define SYN6288_SPEAK_ALARM     1   /* 报警播报（更频繁） */

/**
 * @brief 初始化 SYN6288 模块
 * 
 * USART2 已由 CubeMX 初始化，此函数：
 *   1. 配置 BUSY 引脚（PA4）为输入上拉
 *   2. 发送初始化命令（可选：设置音量等）
 */
void SYN6288_Init(void);

/**
 * @brief 播报指定距离
 * @param distance_cm  距离值（厘米）
 * 
 * 自动拼接为："当前距离XX厘米"
 * 例：SYN6288_SpeakDistance(30); → 播报 "当前距离30厘米"
 */
void SYN6288_SpeakDistance(uint8_t distance_cm);

/**
 * @brief 播报任意 GB2312 文本
 * @param text  GB2312 编码的文本
 * @param len   文本字节数
 */
void SYN6288_SendText(const uint8_t *text, uint8_t len);

/**
 * @brief 检测模块是否忙
 * @return 1=忙(正在播报), 0=空闲
 * 
 * 如果接了 BUSY 引脚，读电平；否则返回 0。
 */
uint8_t SYN6288_IsBusy(void);

/**
 * @brief 设置音量
 * @param vol  0~15，越大越响，默认 10
 */
void SYN6288_SetVolume(uint8_t vol);

#endif /* __SYN6288_H__ */
