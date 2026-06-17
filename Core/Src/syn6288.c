/**
 ******************************************************************************
 * @file       syn6288.c
 * @brief      SYN6288 语音合成驱动 — 100% 对照官方 STM32F103 参考代码
 *
 * 通信参数：USART2, PA2(TX)→SYN6288 RXD, 9600bps 8N1
 *
 * 文本合成帧：FD 00 LL 01 [01|Music] [ZJtext] [XOR]
 *   LL = 文本字节数 + 3
 *   XOR 范围：从 Frame[0] 到最后一个文本字节（包含 0xFD 帧头）
 *
 * 芯片控制帧：FD 00 02 CMD [XOR]   (停止/暂停/恢复/状态查询等)
 *   XOR 范围：从 Frame[0] 到 CMD 字节
 ******************************************************************************
 */

#include "syn6288.h"
#include <string.h>
#include <stdio.h>

static uint8_t syn6288_volume = 10;

/* ---- 芯片控制命令（逐字节对照参考项目 main.c）---- */
static const uint8_t SYN_StopCom[]      = {0xFD, 0x00, 0x02, 0x02, 0xFD}; /* 停止合成 */
static const uint8_t SYN_SuspendCom[]   = {0xFD, 0x00, 0x02, 0x03, 0xFC}; /* 暂停合成 */
static const uint8_t SYN_RecoverCom[]   = {0xFD, 0x00, 0x02, 0x04, 0xFB}; /* 恢复合成 */
static const uint8_t SYN_ChackCom[]     = {0xFD, 0x00, 0x02, 0x21, 0xDE}; /* 状态查询 */
static const uint8_t SYN_PowerDownCom[] = {0xFD, 0x00, 0x02, 0x88, 0x77}; /* 省电模式 */

/* ================================================================
 * 底层函数：发送原始字节（对照参考项目 USART3_SendString）
 * ================================================================ */
static void SYN_SendString(const uint8_t *data, uint8_t len)
{
    printf("[SYN6288] TX %d bytes: ", len);
    for (uint8_t i = 0; i < len; i++)
        printf("%02X ", data[i]);
    printf("\r\n");

    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 1000);
}

/* ================================================================
 * 芯片控制命令发送（对照参考项目 YS_SYN_Set）
 * 用于停止/暂停/恢复/状态查询等
 * ================================================================ */
static void SYN_SendChipCmd(const uint8_t *cmd, uint8_t len)
{
    SYN_SendString(cmd, len);
}

/* ================================================================
 * 文本合成帧发送（对照参考项目 SYN_FrameInfo）
 *
 * 帧格式：
 *   byte[0]     = 0xFD         帧头
 *   byte[1]     = 0x00         长度高字节
 *   byte[2]     = HZ_Length+3  长度低字节 = 文本字节数 + 3
 *   byte[3]     = 0x01         GB2312 编码
 *   byte[4]     = 0x01 | Music<<4  合成命令 + 背景音乐
 *   byte[5..]   = 文本数据 (GB2312 编码的中文 + ASCII 数字)
 *   byte[末尾]   = XOR 校验
 *
 * XOR 校验范围：Frame[0]~Frame[4] 然后 HZdata[0..len-1]
 *    即 INCLUDES 帧头 0xFD（参考项目 for(i=0; i<5; i++)）
 *
 * @param Music   背景音乐选择 (0=无, 1~15=内置背景音乐)
 * @param HZdata  GB2312 编码的文本字符串
 * ================================================================ */
static void SYN_FrameInfo(uint8_t Music, const uint8_t *HZdata)
{
    uint8_t  Frame_Info[64];
    uint8_t  HZ_Length;
    uint8_t  ecc = 0;
    uint8_t  i;

    HZ_Length = strlen((const char *)HZdata);

    /* 帧固定信息 */
    Frame_Info[0] = 0xFD;
    Frame_Info[1] = 0x00;
    Frame_Info[2] = HZ_Length + 3;
    Frame_Info[3] = 0x01;
    Frame_Info[4] = 0x01 | (Music << 4);

    /* XOR 校验：先对前 5 个帧头字节（对照参考项目 for(i=0; i<5; i++)） */
    for (i = 0; i < 5; i++)
        ecc = ecc ^ Frame_Info[i];

    /* XOR 校验：再对文本数据 */
    for (i = 0; i < HZ_Length; i++)
        ecc = ecc ^ HZdata[i];

    /* 填入文本 + 校验 */
    memcpy(&Frame_Info[5], HZdata, HZ_Length);
    Frame_Info[5 + HZ_Length] = ecc;

    /* 发送：5 帧头 + HZ_Length 文本 + 1 XOR = HZ_Length + 6 */
    SYN_SendString(Frame_Info, 5 + HZ_Length + 1);
}

/* ================================================================
 * 初始化 — 等待模块就绪 + 停止残留播放 + 设置音量
 * ================================================================ */
void SYN6288_Init(void)
{
    printf("[SYN6288] Init start...\r\n");

    /* 步骤1：等待模块上电稳定 */
    HAL_Delay(800);

    /* 步骤2：等待 BUSY 引脚变高 */
    {
        uint32_t start = HAL_GetTick();
        uint8_t busy_state = HAL_GPIO_ReadPin(SYN6288_BUSY_PORT, SYN6288_BUSY_PIN);
        printf("[SYN6288] BUSY initial = %d\r\n", busy_state);

        while (HAL_GPIO_ReadPin(SYN6288_BUSY_PORT, SYN6288_BUSY_PIN) == GPIO_PIN_RESET) {
            if (HAL_GetTick() - start > 3000) {
                printf("[SYN6288] BUSY timeout, continuing...\r\n");
                break;
            }
        }
        printf("[SYN6288] BUSY ready after %lu ms\r\n", HAL_GetTick() - start);
    }

    /* 步骤3：停止任何残留的合成播放 */
    printf("[SYN6288] Sending STOP...\r\n");
    SYN_SendChipCmd(SYN_StopCom, sizeof(SYN_StopCom));
    HAL_Delay(100);

    /* 步骤4：设置音量（默认 10） */
    SYN6288_SetVolume(10);

    printf("[SYN6288] Init done.\r\n");
}

/* ================================================================
 * 设置音量 (0~15)
 *
 * 芯片控制命令格式：FD 00 03 [CMD] [vol] [XOR]
 * XOR = FD ^ 00 ^ 03 ^ CMD ^ vol
 * ================================================================ */
void SYN6288_SetVolume(uint8_t vol)
{
    uint8_t frame[6];
    uint8_t xor_val;

    if (vol > 15) vol = 15;
    syn6288_volume = vol;

    frame[0] = 0xFD;
    frame[1] = 0x00;
    frame[2] = 0x03;         /* Len = 3 */
    frame[3] = 0x01;         /* 命令：设置音量 */
    frame[4] = vol;          /* 音量值 0~15 */

    xor_val = 0xFD ^ 0x00 ^ 0x03 ^ 0x01 ^ vol;
    frame[5] = xor_val;

    printf("[SYN6288] SetVolume=%d\r\n", vol);
    SYN_SendChipCmd(frame, 6);
}

/* ================================================================
 * 播报距离："当前距离XX厘米"
 *
 * 文本格式：GB2312 中文 + ASCII 数字（SYN6288 支持 GB2312 码区内
 *           混入 ASCII 字符，无需单独切换到 ASCII 编码模式）
 * ================================================================ */
void SYN6288_SpeakDistance(uint8_t distance_cm)
{
    uint8_t text[30];
    uint8_t idx = 0;

    printf("[SYN6288] SpeakDistance: %d cm\r\n", distance_cm);

    /* "当前距离" (GB2312, 8字节) */
    text[idx++] = 0xB5; text[idx++] = 0xB1;  /* 当 */
    text[idx++] = 0xC7; text[idx++] = 0xB0;  /* 前 */
    text[idx++] = 0xBE; text[idx++] = 0xE0;  /* 距 */
    text[idx++] = 0xC0; text[idx++] = 0xEB;  /* 离 */

    /* 数字（ASCII，最多 3 位，SYN6288 GB2312 模式内置支持 ASCII） */
    if (distance_cm >= 100) {
        text[idx++] = '0' + (distance_cm / 100);
        distance_cm %= 100;
    }
    if (distance_cm >= 10 || idx > 8) {
        text[idx++] = '0' + (distance_cm / 10);
        distance_cm %= 10;
    }
    text[idx++] = '0' + distance_cm;

    /* "厘米" (GB2312, 4字节) */
    text[idx++] = 0xC0; text[idx++] = 0xE5;  /* 厘 */
    text[idx++] = 0xC3; text[idx++] = 0xD7;  /* 米 */

    text[idx] = '\0';

    /* 发送：Music=0 (无背景音乐) */
    SYN_FrameInfo(0, text);
}

/* ================================================================
 * 播报任意 GB2312 文本（内部拼接为字符串后调用 SYN_FrameInfo）
 * ================================================================ */
void SYN6288_SendText(const uint8_t *text, uint8_t len)
{
    uint8_t buf[64];
    if (len > 62) len = 62;
    memcpy(buf, text, len);
    buf[len] = '\0';
    SYN_FrameInfo(0, buf);
}

/* ================================================================
 * 检测忙状态
 * 返回 1 = 忙(正在播报), 0 = 空闲
 * ================================================================ */
uint8_t SYN6288_IsBusy(void)
{
    return (HAL_GPIO_ReadPin(SYN6288_BUSY_PORT, SYN6288_BUSY_PIN) == GPIO_PIN_RESET);
}
