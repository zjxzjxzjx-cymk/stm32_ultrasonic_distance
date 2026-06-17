/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "buzzer.h"
#include "key.h"
#include "hcsr04.h"
#include "ssd1306.h"
#include "syn6288.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* 非阻塞报警状态机变 */
static uint8_t  alarm_active  = 0;  /* 0=正常, 1=报警中 */
static uint32_t alarm_tick    = 0;  /* 报警节奏计时（毫秒） */
static uint32_t voice_tick    = 0;  /* 语音播报间隔计时 */
static uint32_t last_voice_d  = 0;  /* 上次播报时的距离 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* ===== 初始化所有外设模块 ===== */

  /* 开机提示：蜂鸣器短响 */
  Buzzer_BeepShort(100);

  /* OLED 初始化并显示开机画面 */
  SSD1306_Init();
  SSD1306_ClearBuffer();
  SSD1306_ShowString_6x8(20, 10, "Ultrasonic", 1);
  SSD1306_ShowString_6x8(24, 25, "Ranger v1.0", 1);
  SSD1306_ShowString_6x8(28, 50, "Loading...", 1);
  SSD1306_UpdateScreen();
  HAL_Delay(1000);

  /* 超声波模块初始化（使能 TIM2 中断、配置 TRIG 引脚） */
  HCSR04_Init();

  /* SYN6288 语音模块初始化 */
  SYN6288_Init();

  /* 显示就绪画面 */
  SSD1306_ClearBuffer();
  SSD1306_ShowString_6x8(20, 30, "System Ready", 1);
  SSD1306_UpdateScreen();
  HAL_Delay(800);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t  key_val;
    float    distance;
    uint8_t  threshold;
    uint32_t now;

    /* ---- 1. 按键扫描（阻塞式，按一下才走） ---- */
    key_val = Key_Scan();
    if (key_val != KEY_NONE) {
        Key_Process(key_val);           /* 更新阈值 */
        Buzzer_BeepShort(50);           /* 按键反馈音 */
    }

    /* ---- 2. 超声波测距 ---- */
    HCSR04_Trigger();

    /* 等测量完成（非阻塞：用超时保护） */
    {
        uint32_t t0 = HAL_GetTick();
        while (!HCSR04_IsDone()) {
            if (HAL_GetTick() - t0 > 100) break;  /* 100ms 超时 */
        }
    }

    /*
     * 关键：超时时强制 distance = -1，防止读到旧值。
     * 障石物移走 → 无回波超时 → 如果不做此判断，HCSR04_GetDistance() 返回旧值
     * → 程序以为障碍物还在 → 蜂鸣器一直响
     */
    if (!HCSR04_IsDone()) {
        distance = -1.0f;               /* 超时：无回波，标记无效 */
    } else {
        distance = HCSR04_GetDistance(); /* 成功：取新值 */
    }
    threshold = Key_GetThreshold();     /* 读取报警阈值 */
    now       = HAL_GetTick();          /* 当前系统滴答（毫秒） */

    printf("Dist: %.1f cm, Thr: %d\r\n", distance, threshold);  /* 调试输出 */

    /* ---- 3. OLED 显示刷新 ---- */
    SSD1306_ClearBuffer();

    /* 第一行：标题 */
    SSD1306_ShowString_6x8(0, 0, "Distance:", 1);

    if (distance >= 0 && distance <= 400) {
        /* 大号数字 + 单位 "cm"（8x16大字紧跟） */
        SSD1306_ShowNum(20, 10, (int32_t)distance, 3, 2);
        SSD1306_ShowString_8x16(50, 10, "cm", 1);
    } else {
        SSD1306_ShowString_6x8(20, 16, "-- NO SIGNAL --", 1);
        SSD1306_ShowString_6x8(0, 34, "Out of range!", 1);
    }

    /* 底部报警行：全部 8x16 大字 */
    SSD1306_ShowString_8x16(0,  40, "Alarm:", 1);
    SSD1306_ShowNum(54, 40, threshold, 2, 2);
    SSD1306_ShowString_8x16(78, 40, "cm", 1);


    SSD1306_UpdateScreen();

    /* ---- 4. 非阻塞蜂鸣器报警 ---- */
    if (distance >= 0 && distance <= threshold) {
        /*
         * 非阻塞报警：不用 HAL_Delay，用滴答定时器计时。
         * 100ms 响，100ms 停，循环 → 急促 "滴滴" 声。
         */
        alarm_active = 1;

        if (now - alarm_tick >= 100) {
            alarm_tick = now;
            BUZZER_TOGGLE();   /* 翻转蜂鸣器状态（宏，操作 BSRR 寄存器） */
        }
    } else {
        /* 距离正常 → 关闭报警 */
        if (alarm_active) {
            BUZZER_OFF();
            alarm_active = 0;
        }
    }

    /* ---- 5. 语音播报（每 3 秒播报一次，距离变化 >5cm 立即播报） ---- */
    if (distance >= 0 && distance <= 400) {
        uint8_t dist_int = (uint8_t)distance;
        uint8_t dist_diff = (dist_int > last_voice_d)
                            ? (dist_int - last_voice_d)
                            : (last_voice_d - dist_int);

        if (now - voice_tick >= 3000 || dist_diff >= 5) {
            voice_tick   = now;
            last_voice_d = dist_int;
            printf("[MAIN] Triggering voice: %d cm\r\n", dist_int);
            SYN6288_SpeakDistance(dist_int);
        }
    }

    /* ---- 主循环间隔 200ms（5Hz 刷新） ---- */
    HAL_Delay(200);

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief 重定向 printf 到 USART1（调试串口）
  * 
  * Keil 下需要在 Target → Code Generation 中勾选 "Use MicroLIB"。
  * 否则链接时报 __use_no_semihosting 相关的错误。
  * 
  * MicroLIB 是 ARM 的精简 C 库，printf 通过 fputc 输出：
  *   printf("Hello") → 底层调用 fputc('H') → fputc('e') → ...
  * 
  * 这里把 fputc 重定向到 USART1_TX（PA9）。
  */
#ifdef __GNUC__
  /* GCC 编译器用 _write */
  int _write(int fd, char *ptr, int len) {
      HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, 100);
      return len;
  }
#else
  /* Keil ARMCC 编译器用 fputc */
  int fputc(int ch, FILE *f) {
      HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
      return ch;
  }
#endif

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
