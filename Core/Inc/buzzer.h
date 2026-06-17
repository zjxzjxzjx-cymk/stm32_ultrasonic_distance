#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "main.h"

/* ================================================================
 * 蜂鸣器模块 - 头文件
 * 
 * 硬件：PB0 → 1KΩ电阻 → S8050基极 → 有源蜂鸣器
 * 原理：PB0输出高电平 → 三极管导通 → 蜂鸣器响
 *       PB0输出低电平 → 三极管截止 → 蜂鸣器停
 * 
 * 知识点：
 *   有源蜂鸣器 = 通电即响（自激振荡）
 *   无源蜂鸣器 = 需要PWM方波驱动（本项目不涉及）
 * ================================================================ */

/* -------------------- 蜂鸣器引脚宏定义 -------------------- */
#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_PIN_0

/* -------------------- 蜂鸣器驱动模式选择 -------------------- */
/*
 * 两种接法，选一种即可：
 * 
 * 模式1：三极管驱动（原方案，需 S8050 + 电阻）
 *   3.3V → 蜂鸣器+ → 蜂鸣器- → S8050集电极 → S8050发射极 → GND
 *   PB0 → 1KΩ → S8050基极
 *   PB0=高 → 三极管导通 → 蜂鸣器响
 *   使用：BUZZER_DRIVE_HIGH
 * 
 * 模式2：直驱低端（无三极管，面包板简化测试用）
 *   3.3V → 蜂鸣器+ → 蜂鸣器- → PB0
 *   PB0=低 → 电流吸入 → 蜂鸣器响（GPIO 吸电流 ~25mA，小蜂鸣器够用）
 *   PB0=高 → 不响
 *   使用：BUZZER_DRIVE_LOW
 */
#define BUZZER_DRIVE_HIGH       /* SIG型模块 / 三极管驱动：SIG=HIGH→响 */
//#define BUZZER_DRIVE_LOW      /* 直驱两脚蜂鸣器：PB0=LOW→响（电流吸入） */

/* -------------------- 蜂鸣器开关宏 -------------------- */
#ifdef BUZZER_DRIVE_HIGH
  /* 三极管驱动：PB0=高 → 响 */
  #define BUZZER_ON()     (GPIOB->BSRR = GPIO_PIN_0)
  #define BUZZER_OFF()    (GPIOB->BSRR = (uint32_t)GPIO_PIN_0 << 16)

#else
  /* 直驱低端：PB0=低 → 响（GPIO 吸电流驱动） */
  #define BUZZER_ON()     (GPIOB->BSRR = (uint32_t)GPIO_PIN_0 << 16)  /* PB0=0 */
  #define BUZZER_OFF()    (GPIOB->BSRR = GPIO_PIN_0)                  /* PB0=1 */
#endif

#define BUZZER_TOGGLE() (HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN))

/* -------------------- 蜂鸣器发声模式 -------------------- */
/*
 * 用 HAL_Delay 实现不同的发声模式。
 * HAL_Delay 是阻塞延时，ms 级精度，会让 CPU 啥都不干。
 * 后续学完定时器中断后可以改成非阻塞方式。
 */

/**
 * @brief 短促"滴"一声
 * @param duration_ms  持续时间（毫秒），建议 50~200
 * 
 * 例：Buzzer_BeepShort(100);  → 响100ms后停
 */
void Buzzer_BeepShort(uint16_t duration_ms);

/**
 * @brief 连续短促报警音（距离过近时使用）
 * @param count  响的次数，0 = 一直响直到手动停止
 * 
 * 例：Buzzer_Alarm_SOS(5); → 滴滴滴滴滴（5声）
 */
void Buzzer_Alarm_SOS(uint8_t count);

/**
 * @brief 长鸣一声（用于开机提示）
 */
void Buzzer_BeepLong(uint16_t duration_ms);

/**
 * @brief 停止蜂鸣器
 */
void Buzzer_Stop(void);

#endif /* __BUZZER_H__ */
