#ifndef __HCSR04_H__
#define __HCSR04_H__

#include "main.h"
#include "tim.h"

/* ================================================================
 * HC-SR04 超声波测距模块 - 头文件
 * 
 * 硬件：TRIG → PA0 (GPIO 输出，手动发脉冲)
 *       ECHO → PA1 (TIM2_CH2 输入捕获)
 * 
 * 测距公式：距离(cm) = ECHO高电平时间(μs) / 58
 *   推导：声速 340m/s = 0.034cm/μs
 *        距离 = 时间 × 声速 / 2（往返）= 时间 × 0.017
 *        即：距离 = 时间 / 58.8 ≈ 时间 / 58
 * 
 * TIM2 配置（CubeMX）：
 *   时钟源：72MHz (APB1定时器时钟)
 *   预分频：72-1=71 → 1MHz (每1μs 计数值+1)
 *   自动重装：65535 → 最大测量时间 65.535ms
 *   最大可测距离 = 65535 / 58 ≈ 1130cm（远超 HC-SR04 的 400cm 极限）
 * 
 * 知识点：
 *   1. 定时器输入捕获：自动锁存引脚跳变时刻的计数值
 *   2. 中断回调：HAL 框架提供 HAL_TIM_IC_CaptureCallback 作为用户钩子
 *   3. 边沿切换：捕获上升沿→记录时间→切下降沿→计算脉宽→切回上升沿
 * ================================================================ */

/* ---- TRIG 引脚宏 ---- */
#define HCSR04_TRIG_PORT    GPIOA
#define HCSR04_TRIG_PIN     GPIO_PIN_0

/* ---- 超时值 ---- */
#define HCSR04_TIMEOUT_CM   450    // 超时距离(cm)，超出视为"无目标"

/**
 * @brief 初始化 HC-SR04 模块
 * 
 * 工作内容：
 *   1. 配置 PA0(TRIG) 为推挽输出（覆盖 CubeMX 的 AF 配置）
 *   2. 使能 TIM2 中断，用于输入捕获回调
 *   3. 启动 TIM2 输入捕获（上升沿触发）
 */
void HCSR04_Init(void);

/**
 * @brief 触发一次超声波测距
 * 
 * TRIG 引脚发送 ≥10μs 高脉冲，模块自动发出 8 个 40kHz 方波，
 * ECHO 引脚随后输出高电平，持续时间为超声波往返时间。
 * 
 * 注意：此函数仅"发起"测距，实际距离在中断回调中计算，
 *       主循环通过 HCSR04_GetDistance() 读取结果。
 */
void HCSR04_Trigger(void);

/**
 * @brief 获取最近一次测量的距离
 * @return 距离（厘米），-1 表示超时/无效
 */
float HCSR04_GetDistance(void);

/**
 * @brief 查询测量是否完成
 * @return 1=已完成, 0=等待中
 * 
 * 主循环调用流程：
 *   HCSR04_Trigger();
 *   while (!HCSR04_IsDone());  // 等中断完成
 *   dist = HCSR04_GetDistance();
 * 
 * 如果不用 while 死等，可以：
 *   HCSR04_Trigger();
 *   HAL_Delay(60);  // HC-SR04 最多 38ms 就回来
 *   dist = HCSR04_GetDistance();
 */
uint8_t HCSR04_IsDone(void);

#endif /* __HCSR04_H__ */
