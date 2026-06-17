\# 🔉 超声波测距系统 — UltraasonicRanger



> 基于 STM32F103C8T6 + HC-SR04 + OLED + SYN6288 语音播报 + 蜂鸣器报警



\[!\[MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-blue)]()

\[!\[IDE](https://img.shields.io/badge/IDE-Keil5%20%2B%20CubeMX-orange)]()

\[!\[HAL](https://img.shields.io/badge/Library-STM32%20HAL-green)]()



\---



\## 📸 功能演示



| 功能                |                                        说明                                 |

|--------------------|-------------------------------------------------------------------------------|

|   🎯 实时测距  | HC-SR04 超声波模块，量程 2cm\~400cm，精度 ±1cm |

| 📺 OLED 显示 | 0.96寸 I2C SSD1306，显示距离、阈值、报警状态 |

| 🔊 语音播报 | SYN6288 语音合成模块，播报"当前距离XX厘米" |

| 🚨 蜂鸣器报警 | 低于阈值时急促滴滴报警，非阻塞不卡主循环 |

| 🔌 串口调试 | USART1 printf 实时输出距离/状态/帧数据 |



\---



\## 🛠️ 硬件清单 (BOM)



| 序号 | 物料 | 规格 | 数量 |

|------|------|------|------|

| 1 | 单片机最小系统板 | STM32F103C8T6 (蓝色小板) | 1 |

| 2 | ST-Link 下载器 | ST-Link V2 | 1 |

| 3 | 超声波模块 | HC-SR04 | 1 |

| 4 | OLED 显示屏 | 0.96寸 I2C SSD1306 (4Pin) | 1 |

| 5 | 语音合成模块 | SYN6288 | 1 |

| 6 | 有源蜂鸣器模块 | 4脚 SIG型 (3V3/GND/SIG/NC) | 1 |

| 7 | 轻触按键 | 6x6x5mm | 1 |

| 8 | 扬声器/小喇叭 | 8Ω 0.5W (接 SYN6288 SPK+/SPK-) | 1 |

| 9 | 面包板 | 830孔 | 1 |

| 10 | 杜邦线 | 公母各若干 | - |

| 11 | USB-TTL模块(调试) | CH340 / CP2102 | 1 |



\---



\## 🔌 引脚接线一览



```

STM32F103C8T6          外设模块

┌────────────┐         ┌──────────────┐

│ PA0 ───────┼────────→│ HC-SR04 TRIG  │

│ PA1 ───────┼←────────│ HC-SR04 ECHO  │

│            │         └──────────────┘

│ PA2(TX) ───┼────────→│ SYN6288 RXD   │

│ PA3(RX) ───┼←────────│ SYN6288 TXD   │

│ PA4 ───────┼←────────│ SYN6288 BUSY  │

│            │         └──────────────┘

│ PB6(SCL) ──┼────────→│ OLED SCL      │

│ PB7(SDA) ──┼────────→│ OLED SDA      │

│            │         └──────────────┘

│ PB0 ───────┼────────→│ 蜂鸣器 SIG    │

│ PB13 ──────┼←────────│ 按键 → GND    │

│            │         └──────────────┘

│ PA9(TX) ───┼────────→│ USB-TTL RX    │  (调试串口)

│ PA10(RX) ──┼←────────│ USB-TTL TX    │

└────────────┘         └──────────────┘

```



> ⚠️ \*\*重要提醒：\*\*

> - STM32 GND 必须与 SYN6288 GND \*\*共地\*\*，否则模块收不到数据

> - SYN6288 模块用 \*\*5V\*\* 供电，HC-SR04 用 5V，OLED 用 3.3V

> - 喇叭插在 SYN6288 模块的 SPK+/SPK-，不是插 STM32

> - 蜂鸣器 SIG 型：SIG=HIGH 响，代码已选 `BUZZER\_DRIVE\_HIGH`



\---



\## ⚡ 快速上手（5 分钟跑起来）



\### 1. 安装开发环境



| 工具 | 用途 | 获取方式 |

|------|------|---------|

| Keil MDK-ARM V5 | 编译 \& 烧录 | \[ARM 官网](https://www.keil.com/download/product/) |

| STM32CubeMX | 引脚/时钟配置 | \[ST 官网](https://www.st.com/stm32cubemx) |

| 串口助手 | 看 printf 调试输出 | SSCOM / XCOM / MobaXterm |



\### 2. 编译项目



```

1\. 打开 Keil5

2\. Project → Open Project → 选择 UltraasonicRanger/ 下的 .uvprojx 文件

3\. 编译 (F7) → 0 Error, 5 Warning

4\. ST-Link 连接目标板 → 烧录 (F8)

```



> 若 IntelliSense 报错，项目内已含 `.vscode/c\_cpp\_properties.json`。



\### 3. 面包板搭建



按上面引脚接线图把各模块接好，上电。



\### 4. 看串口输出



```

USB-TTL 接 PA9(TX)/PA10(RX)/GND

串口助手设置：115200-8-N-1

```



上电后串口输出类似：

```

\[SYN6288] Init start...

\[SYN6288] BUSY ready after 300 ms

\[SYN6288] Init done.

Dist: 30.8 cm, Thr: 20

```



\### 5. 测距验证



```

用手掌在 HC-SR04 前方移动：

&#x20; - OLED 上距离数字实时变化

&#x20; - 距离 < 阈值时蜂鸣器急促滴滴

&#x20; - 每 3 秒 SYN6288 语音播报

&#x20; - 短按 PB13：阈值 +10cm / 长按 PB13：阈值 -10cm

```



\---



\## 📂 项目结构



```

chaoshengbo/

├── UltraasonicRanger/          ← Keil5 + CubeMX 工程

│   ├── Core/

│   │   ├── Inc/                ← 头文件

│   │   │   ├── hcsr04.h        ← HC-SR04 超声波驱动

│   │   │   ├── ssd1306.h       ← OLED SSD1306 驱动

│   │   │   ├── syn6288.h       ← SYN6288 语音合成驱动

│   │   │   ├── buzzer.h        ← 蜂鸣器驱动

│   │   │   ├── key.h           ← 按键驱动

│   │   │   └── ...

│   │   └── Src/                ← 源文件

│   │       ├── main.c          ← 主程序（调度逻辑）

│   │       ├── hcsr04.c        ← 输入捕获测距

│   │       ├── ssd1306.c       ← OLED 字库+渲染

│   │       ├── syn6288.c       ← 协议帧封装

│   │       ├── buzzer.c        ← 非阻塞报警

│   │       ├── key.c           ← 长短按消抖

│   │       └── ...

│   ├── Drivers/                ← HAL 库 (CubeMX 生成)

│   └── UltraasonicRanger.ioc   ← CubeMX 工程文件

│

├── 01\_项目架构设计.md           ← 完整架构 \& 设计文档

├── 02\_面包板测试与PCB制版步骤.md ← 面包板搭建 \& PCB 制版

├── 03\_初学者知识手册.md         ← 13章知识手册 (GPIO/定时器/I2C/UART...)

└── 04\_调试复盘与知识总结.md     ← 实战 Bug 复现 \& 排查

```



\---



\## 🧠 软件架构



```

┌──────────────────────────────────┐

│       main.c — 任务调度           │

│  200ms周期：按键→测距→显示→报警→语音 │

├──────────────────────────────────┤

│  hcsr04.c    TIM2\_CH2 输入捕获    │

│  ssd1306.c   I2C 直接写GDDRAM     │

│  syn6288.c   USART2 9600 协议帧   │

│  buzzer.c    PB0 非阻塞Toggle     │

│  key.c       PB13 长短按消抖      │

├──────────────────────────────────┤

│  STM32 HAL (CubeMX 生成)          │

├──────────────────────────────────┤

│  STM32F103C8T6 (72MHz, 64K Flash)  │

└──────────────────────────────────┘

```



\*\*关键设计要点：\*\*



| 要点 | 说明 |

|------|------|

| 非阻塞报警 | 用 `HAL\_GetTick()` 时间戳驱动，不用 `HAL\_Delay` 卡主循环 |

| `volatile` 变量 | 中断回调中修改的距离变量加 `volatile`，防止编译器优化读到旧值 |

| 超时保护 | `HCSR04\_IsDone()` 判断测量是否完成，超时返回 -1，防止读到旧距离 |

| OLED 直接写 | 对照野火参考项目，无帧缓冲，`Font6x8\[95]\[6]` 逐字节复制验证过的字库 |

| SYN6288 协议 | 对照官方参考项目，帧长度 `text+3`，XOR 包含 `0xFD` 帧头 |



\---



\## 📖 配套文档



四份详细文档已包含在本仓库中，建议按顺序阅读：



| 文档 | 适合人群 | 内容 |

|------|---------|------|

| \[01\_项目架构设计](01\_项目架构设计.md) | 想了解整体设计 | 硬件架构、软件分层、引脚分配、CubeMX配置 |

| \[02\_面包板测试](02\_面包板测试与PCB制版步骤.md) | 想搭硬件 | 材料清单、分步接线、分模块测试、PCB制版 |

| \[03\_初学者知识手册](03\_初学者知识手册.md) | 想学底层原理 | GPIO/定时器/中断/I2C/UART/C语言进阶 |

| \[04\_调试复盘](04\_调试复盘与知识总结.md) | 遇到 Bug 想查 | OLED乱码、SYN6288不响、蜂鸣器反转的真实修复过程 |



\---



\## 🐛 常见问题



<details>

<summary><b>OLED 屏幕乱码？</b></summary>



检查三点：<br>

1\. I2C 地址是否正确（7位=`0x3C`，HAL层=`0x78`）<br>

2\. 字库取模方式是否与显示逻辑一致（本项目用共阴-列行式-逆向输出）<br>

3\. 确认编译的是正确的 `.c` 文件

</details>



<details>

<summary><b>SYN6288 喇叭不响？</b></summary>



按三步排查：<br>

1\. \*\*硬件层\*\*：短接 PA2/PA3 做 USART 回环自检<br>

2\. \*\*模块层\*\*：发命令后读 PA3 看模块是否回应 `0x4A`<br>

3\. \*\*协议层\*\*：printf 打印发送帧，确认 byte\[2]=`text+3`、byte\[4]=`0x01`、XOR 包含 FD<br><br>

\*\*最常见的遗漏：GND 没有共地。\*\*

</details>



<details>

<summary><b>蜂鸣器大于阈值时一直响？</b></summary>



两种可能：<br>

1\. SIG 型模块驱动模式选错了——确认 `buzzer.h` 选的是 `BUZZER\_DRIVE\_HIGH`<br>

2\. HC-SR04 超时返回了上一次的旧距离值——确认 `main.c` 中调用了 `HCSR04\_IsDone()` 做超时判断

</details>



<details>

<summary><b>Keil 编译后 0 Error 但烧录不进去？</b></summary>



检查 ST-Link 连接：SWCLK ↔ SWCLK, SWDIO ↔ SWDIO, GND ↔ GND。如果 ST-Link V2 的 3.3V 已接目标板，就不用外接电源。

</details>



\---



\## 🎓 适合谁学习



\- 📚 刚学完 STM32 基础知识，想做第一个完整项目

\- 🔧 想了解 GPIO/定时器/中断/I2C/UART 在实际项目中的用法

\- 🐛 想学习真实的嵌入式调试方法（不是"一次就通"的演示代码）

\- 🎤 想玩 SYN6288 语音合成模块



\---



\## 📄 License



MIT License — 可以自由使用、修改、分发。



\---



\## 🙏 致谢



\- STM32 HAL 库 \& CubeMX — STMicroelectronics

\- OLED 驱动方案参考 — 野火科技 `bsp\_i2c\_oled`

\- SYN6288 协议参考 — 绿深旗舰店官方 `7.1、STM32F103C8T6应用程序`



