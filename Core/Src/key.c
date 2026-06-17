/* ================================================================
 * 按键模块 - 源文件
 * 
 * 【新手必读 - 按键消抖原理】
 * 
 * 当你按下机械按键时，金属触点不是"啪"一下就接触好的。
 * 用示波器看，电平会在 0 和 1 之间快速跳动 5~20ms，这是"弹跳(bounce)"。
 * 
 * 如果不消抖：
 *   你按一下 → MCU 可能读到 100 次"按下+松开" → 误触发100次
 * 
 * 消抖方法（本模块用的是"延时消抖"，最简单）：
 *   1. 检测到低电平 → 等 20ms → 再读一次
 *   2. 如果还是低电平 → 确认真按下（不是抖动）
 *   3. 如果是高电平 → 是抖动，忽略
 * 
 * 【为什么用 static 变量？】
 *   threshold 用 static 修饰，意思是"这个变量的生命周期 = 整个程序运行期"，
 *   但作用域只在本文件内（别的 .c 文件看不到）。这叫"文件私有变量"。
 *   不用全局变量（extern）的原因是：避免被其他模块意外修改。
 * 
 * 【阻塞式扫描的限制】
 *   当前 Key_Scan() 是阻塞式的——按着键不放，程序就卡在函数里。
 *   对于本项目（200ms 主循环），这个阻塞影响不大。
 *   真正的产品会用"非阻塞扫描"（纯读取状态，不等待），配合定时器实现。
 * ================================================================ */

#include "key.h"

/* ---- 报警阈值：文件私有变量，默认 20cm ---- */
static uint8_t threshold = 20;

/* ================================================================
 * 阻塞式按键扫描
 * 
 * 返回值：
 *   KEY_UP   → PB12 被完整地"按下→松开"了一次
 *   KEY_DOWN → PB13 被完整地"按下→松开"了一次  
 *   KEY_NONE → 没有任何按键动作
 * 
 * 流程：
 *   1. 读 PB12 / PB13 电平
 *   2. 如果都为高（没按），直接返回 KEY_NONE
 *   3. 如果某个为低，延时 20ms 消抖
 *   4. 再次确认电平（排除抖动干扰）
 *   5. 等待按键松开（while 循环卡住，直到松手）
 *   6. 返回对应的按键值
 * ================================================================ */
uint8_t Key_Scan(void)
{
    /* 步骤1+2：先快速判断有没有键按下 */
    if (HAL_GPIO_ReadPin(KEY_UP_PORT, KEY_UP_PIN) == GPIO_PIN_RESET) {
        /* PB12 读到低电平 = 疑似按下 */
        HAL_Delay(KEY_DEBOUNCE_MS);                     // 消抖等 20ms
        if (HAL_GPIO_ReadPin(KEY_UP_PORT, KEY_UP_PIN) == GPIO_PIN_RESET) {
            /* 确认真按下 */
            while (HAL_GPIO_ReadPin(KEY_UP_PORT, KEY_UP_PIN) == GPIO_PIN_RESET) {
                /* 卡在这里直到松手，防止按一次触发多次 */
            }
            return KEY_UP;
        }
    }

    if (HAL_GPIO_ReadPin(KEY_DOWN_PORT, KEY_DOWN_PIN) == GPIO_PIN_RESET) {
        HAL_Delay(KEY_DEBOUNCE_MS);
        if (HAL_GPIO_ReadPin(KEY_DOWN_PORT, KEY_DOWN_PIN) == GPIO_PIN_RESET) {
            while (HAL_GPIO_ReadPin(KEY_DOWN_PORT, KEY_DOWN_PIN) == GPIO_PIN_RESET) {
                /* 等松手 */
            }
            return KEY_DOWN;
        }
    }

    return KEY_NONE;  // 没有有效按键
}

/* ================================================================
 * 获取当前报警阈值
 * 其他模块通过此函数读取阈值，而不直接访问 static 变量。
 * 
 * 这叫"封装"——外部只通过函数接口获取数据，
 * 内部实现细节（static 变量）对外隐藏。
 * ================================================================ */
uint8_t Key_GetThreshold(void)
{
    return threshold;
}

/* ================================================================
 * 处理按键逻辑（由 main.c 的主循环调用）
 * 
 * 功能：
 *   KEY_UP   → 阈值 +5cm（最大不超过 100cm）
 *   KEY_DOWN → 阈值 -5cm（最小不低于 5cm）
 * 
 * 设计思路：
 *   把"检测按键"和"处理按键逻辑"分开，职责清晰。
 *   main.c 调用流程：
 *     key_val = Key_Scan();
 *     if (key_val != KEY_NONE) {
 *         Key_Process(key_val);  // 更新阈值 + 蜂鸣器反馈
 *     }
 * 
 * 注意：阈值步进 5cm 是为了避免频繁微调。
 * ================================================================ */
void Key_Process(uint8_t key_val)
{
    if (key_val == KEY_UP) {
        if (threshold < 100) {
            threshold += 5;    // 每次加 5cm
        }
    } else if (key_val == KEY_DOWN) {
        if (threshold > 5) {
            threshold -= 5;    // 每次减 5cm
        }
    }
}
