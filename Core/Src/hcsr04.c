/* ================================================================
 * HC-SR04 超声波测距驱动 - 源文件
 * 
 * 【新手必读 - 定时器输入捕获原理】
 * 
 * 什么是"输入捕获"？
 *   想象一个秒表（TIM2），每秒跳 1000000 次（1MHz）。
 *   当一个引脚（PA1/ECHO）从 0 变成 1 时，硬件自动把当前的
 *   秒表读数"捕获"存到一个寄存器里（CCR2）。
 *   CPU 不需要每微秒去读一次引脚，硬件帮你做了。
 *   CPU 只在"捕获完成"时通过中断被通知一次。
 * 
 * 为什么需要切换边沿？
 *   测脉宽 = 需要知道"上升沿时刻"和"下降沿时刻"
 *   第1次中断（上升沿）：记录时间 T1，切换为下降沿捕获
 *   第2次中断（下降沿）：记录时间 T2，脉宽 = T2-T1，切换回上升沿
 * 
 * 【中断优先级说明】
 *   HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);
 *   第1个0：抢占优先级（Preemption Priority），数字越小越优先
 *   第2个0：子优先级（Sub Priority）
 *   STM32F1 的优先级分组是 NVIC_PRIORITYGROUP_4（4位全用于抢占优先级）
 *   所以子优先级参数实际无效，只有抢占优先级生效。
 *   0 = 最高优先级（测距需要及时响应）
 * 
 * 【为什么要关中断？】
 *   __disable_irq() / __enable_irq() 是临界区保护。
 *   防止在读取 distance 时恰好被 TIM2 中断更新了值，
 *   导致读到"半个新值+半个旧值"的错乱数据。
 *   这种做法叫"互斥"（Mutual Exclusion）。
 * ================================================================ */

#include "hcsr04.h"
#include <string.h>  /* memset */

/* ---- 全局变量：存储测量结果 ---- */
static volatile float  g_distance = -1.0f;  /* -1 = 未测量/超时 */
static volatile uint8_t g_capture_done = 0; /* 0=等待中, 1=捕获完成 */
static volatile uint32_t g_capture_rise = 0; /* 上升沿计数值 */

/* ---- 计数器溢出次数（处理 65535 回绕） ---- */
static volatile uint16_t g_overflow_count = 0;

/* ---- 前向声明（C语言要求函数在调用前先声明） ---- */
static void delay_us(uint32_t us);

/* ================================================================
 * 初始化 HC-SR04
 * 
 * 步骤说明：
 *   1. 把 PA0 从 CubeMX 配的 AF 模式切换为普通 GPIO 输出模式
 *      （因为 TIM2_CH1 配的是 TIMING 模式，没有实际的硬件输出）
 *   2. 使能 TIM2 中断用于捕获 ECHO 信号
 *   3. 启动输入捕获
 * 
 * 【知识点】HAL_NVIC_EnableIRQ vs HAL_TIM_Base_Start_IT
 *   HAL_NVIC_EnableIRQ：告诉 NVIC（嵌套向量中断控制器）"TIM2 的中断请求，请处理"
 *   HAL_TIM_IC_Start_IT：告诉 TIM2 "开始输入捕获，有事件时产生中断请求"
 *   两者缺一不可。NVIC 是总开关，TIM2 的外设中断使能是分开关。
 * ================================================================ */
void HCSR04_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* ---- 步骤1：重新配置 PA0 为普通 GPIO 推挽输出 ---- */
    /*
     * 为什么不直接用 CubeMX 的输出比较模式？
     *   因为输出比较模式需要精确控制 TIM2 的比较值来产生脉冲，
     *   复杂度高、配置易出错。对初学者来说，
     *   用 GPIO 位操作（HAL_GPIO_WritePin + 微秒延时）更直观。
     */
    GPIO_InitStruct.Pin   = HCSR04_TRIG_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;   /* 推挽输出 */
    GPIO_InitStruct.Pull  = GPIO_NOPULL;           /* 不上拉/下拉 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(HCSR04_TRIG_PORT, &GPIO_InitStruct);

    /* TRIG 初始拉低 */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

    /* ---- 步骤2：使能 TIM2 中断 ---- */
    /*
     * 进入 CubeMX → NVIC Settings → TIM2 global interrupt 勾选 Enable
     * 可自动生成此代码。这里手动补充，效果相同。
     */
    HAL_NVIC_SetPriority(TIM2_IRQn, 0, 0);   /* 最高抢占优先级 */
    HAL_NVIC_EnableIRQ(TIM2_IRQn);           /* 在 NVIC 中使能 */

    /* ---- 步骤3：使能 TIM2 更新中断（用于溢出处理） + 启动输入捕获 ---- */
    /*
     * TIM_IT_UPDATE：计数器溢出（从 65535 归零时触发）
     * TIM_IT_CC2：通道2输入捕获事件
     * 
     * 为什么要启用更新中断？
     *   当距离>655cm（脉宽>65535μs）时，计数器会溢出归零。
     *   通过计数溢出次数，可以测到更远的距离（虽然 HC-SR04 只能到400cm）。
     *   如果不启用，超过 65.5ms 的脉宽会被错误计算。
     */
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);  /* 使能溢出中断 */
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);  /* 启动通道2输入捕获 */
}

/* ================================================================
 * 触发测距
 * 
 * 向 TRIG 引脚发送一个 10μs 以上的高脉冲。
 * 之后 HC-SR04 自动发出 8 个 40kHz 超声波脉冲，
 * ECHO 引脚回传高电平（时长 = 声波往返时间）。
 * 
 * 为什么要复位 g_overflow_count？
 *   因为这是新一次测量，溢出不累积。
 * ================================================================ */
void HCSR04_Trigger(void)
{
    g_capture_done   = 0;      /* 标记"新一轮测量进行中" */
    g_overflow_count = 0;      /* 溢出计数清零 */
    __HAL_TIM_SET_COUNTER(&htim2, 0);  /* 计数器从0开始 */

    /* 发送 TRIG 脉冲：高 → 延时 15μs → 低 */
    /*
     * 为什么是 15μs 而不是 10μs？
     *   规格书要求 ≥10μs。实际用 15μs 留余量，
     *   避免因 HAL_GPIO_WritePin 函数调用本身也有几微秒开销
     *   导致脉冲不足 10μs 而触发失败。
     */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
    delay_us(15);   /* 高电平保持 15 微秒 */
    HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

/* ================================================================
 * 微秒级延时（阻塞式，用于 TRIG 脉冲）
 * 
 * STM32F103 @ 72MHz：
 *   1个 NOP 约 0.014μs（14ns）
 *   __NOP() 循环约 72 次 ≈ 1μs（经验值，非精确）
 *   这里用简单的空循环实现，适合 10~20μs 级延时。
 * 
 * 为什么不精确？
 *   - 编译器优化会影响循环执行次数
 *   - 中断可能导致延时变长
 *   但对于 TRIG 脉冲（只需要 ≥10μs），这种精度足够。
 * 
 * 更精确的做法：用 SysTick 或 DWT 计数器。但为了简洁，先这样写。
 * ================================================================ */
static void delay_us(uint32_t us)
{
    /*
     * 72MHz 下，每个 NOP = 1/72MHz ≈ 13.9ns
     * 1μs 需要约 72 个 NOP
     * 但 C 语言的 for 循环有额外开销，所以这里用 5 的倍数来粗略延时
     * 实际延时 ≈ us × 循环体内耗时
     * 
     * 如果你有示波器，可以实测 TRIG 引脚验证脉冲宽度。
     */
    uint32_t delay = us * 5;  /* 粗略校准因子 */
    while (delay--) {
        __NOP();
    }
}

/* ================================================================
 * 获取距离值（线程安全）
 * 
 * 用 __disable_irq() / __enable_irq() 保护读取过程，
 * 防止中断在读取 g_distance 中途修改它的值。
 * 
 * 例：g_distance 是 float（4字节），如果中断恰好在读了2字节后发生，
 *     并修改了 g_distance，那读到的就是"半个旧+半个新"的错乱值。
 * 
 * 返回值：
 *   ≥0   → 有效距离（厘米）
 *   -1.0 → 未完成/超时/无效
 * ================================================================ */
float HCSR04_GetDistance(void)
{
    float dist;
    __disable_irq();           /* 关全局中断（临界区起点） */
    dist = g_distance;         /* 原子读取 */
    __enable_irq();            /* 开全局中断（临界区终点） */
    return dist;
}

/* ================================================================
 * 查询测量是否完成
 * ================================================================ */
uint8_t HCSR04_IsDone(void)
{
    return g_capture_done;
}

/* ================================================================
 * TIM2 输入捕获中断回调（HAL 框架钩子函数）
 * 
 * 这是 HAL 库提供的"钩子"——你不需要写完整的中断服务函数，
 * HAL 已经帮你处理了寄存器操作，在这个回调里只写业务逻辑即可。
 * 
 * 调用链：
 *   TIM2_IRQHandler() [在 stm32f1xx_it.c]
 *     → HAL_TIM_IRQHandler(&htim2) [HAL 库]
 *       → HAL_TIM_IC_CaptureCallback(&htim2) [此函数]
 * 
 * 工作流程（本函数内）：
 *   第1次进来（上升沿）→ 记录时间 → 切换为下降沿捕获
 *   第2次进来（下降沿）→ 计算脉宽 → 换算距离 → 切换回上升沿
 * ================================================================ */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {

        uint32_t capture_value = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

        /* 判断是上升沿还是下降沿 */
        /*
         * __HAL_TIM_GET_FLAG 检测 TIM_FLAG_CC2（捕获/比较2标志）
         * 触发源是上升沿还是下降沿，取决于当前 ICPolarity 配置。
         * 
         * 这里的逻辑是：
         *   - 如果 ICPolarity 是 RISING（上升沿捕获），那这次中断就是上升沿触发
         *   - 如果 ICPolarity 是 FALLING（下降沿捕获），那这次就是下降沿触发
         * 
         * 但实际上 HAL 在回调时已经给你区分好了，通过检查
         * htim->Channel 和当前的边沿配置来判断。
         * 
         * 更简单的方法：用一个状态变量 g_capture_state
         *   状态 0 → 等待上升沿
         *   状态 1 → 等待下降沿
         */
        static uint8_t is_waiting_falling = 0; /* 0=等上升沿, 1=等下降沿 */

        if (is_waiting_falling == 0) {
            /* ===== 上升沿：记录起始时间 ===== */
            g_capture_rise = capture_value;
            g_overflow_count = 0;       /* 溢出计数从0开始 */

            /* 切换为下降沿捕获 */
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2,
                                          TIM_INPUTCHANNELPOLARITY_FALLING);
            is_waiting_falling = 1;

        } else {
            /* ===== 下降沿：计算脉宽 → 换算距离 ===== */
            uint32_t pulse_width;       /* 高电平脉宽（微秒） */

            if (capture_value >= g_capture_rise) {
                /* 正常情况：没有溢出 */
                pulse_width = capture_value - g_capture_rise;
            } else {
                /* 溢出情况：从 g_capture_rise 到 65535，再从 0 到 capture_value */
                pulse_width = (65535 - g_capture_rise) + capture_value + 1;
            }

            /* 累加上之前的中途溢出次数（每次溢出 = 65536μs） */
            pulse_width += g_overflow_count * 65536;

            /* ===== 距离换算 ===== */
            /*
             * 声速 340m/s = 0.034 cm/μs
             * 距离 = 超声波往返时间 / 2 × 声速
             *      = pulse_width / 2 × 0.034
             *      = pulse_width × 0.017
             *      ≈ pulse_width / 58.0
             * 
             * 为什么是 58.0 而不是 58？
             *   用浮点数保证计算精度，避免整数除法截断。
             */
            g_distance = (float)pulse_width / 58.0f;

            g_capture_done = 1;         /* 标记"测量完成" */

            /* 切换回上升沿，准备下一轮 */
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2,
                                          TIM_INPUTCHANNELPOLARITY_RISING);
            is_waiting_falling = 0;
        }
    }
}

/* ================================================================
 * TIM2 计数器溢出回调
 * 
 * 当计数器从 65535 回到 0 时触发。
 * 如果此时 ECHO 还是高电平（还没收到下降沿），
 * 说明脉宽超过了 65535μs，需要记录溢出次数。
 * 
 * 这个函数对 ≤400cm 的测量其实用不到（65.5ms 远大于实际脉宽），
 * 但保留它可以让代码更健壮。
 * ================================================================ */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        g_overflow_count++;
    }
}
