/**
 ******************************************************************************
 * @file       ssd1306.h
 * @brief      SSD1306 OLED 驱动头文件（直接写入 GDDRAM，无帧缓冲）
 *             100% 对照野火 bsp_i2c_oled 方案重写
 ******************************************************************************
 */
#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "main.h"
#include "i2c.h"
#include <string.h>

/* ---- I2C 地址 ---- */
#define SSD1306_ADDR    0x3C   /* 7位设备地址 */
#define I2C_TIMEOUT     100    /* I2C 超时 (ms) */

/* ---- 写命令/数据标识 ---- */
#define SSD1306_CMD     0x00   /* 下一个字节是命令 */
#define SSD1306_DATA    0x40   /* 下一个字节是数据 */

/* ---- 屏幕分辨率 ---- */
#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

/* ==================== API 函数 ==================== */

void SSD1306_Init(void);
void SSD1306_ClearBuffer(void);     /* 清屏（直接写OLED GDDRAM） */
void SSD1306_UpdateScreen(void);    /* 空函数，无帧缓冲无需刷新 */

void SSD1306_ShowChar_6x8(uint8_t page, uint8_t col, char ch);
void SSD1306_ShowChar_8x16(uint8_t page, uint8_t col, char ch);

void SSD1306_ShowString_6x8(uint8_t x, uint8_t y, const char *str, uint8_t color);
void SSD1306_ShowString_8x16(uint8_t x, uint8_t y, const char *str, uint8_t color);
void SSD1306_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len, uint8_t font_size);

#endif /* __SSD1306_H__ */
