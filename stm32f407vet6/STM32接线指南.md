# STM32F407VET6 与 ILI9341、OV7670 接线指南

> 基于项目 [OV7670-ILI9341-STM32F407VET6](https://github.com/K-Rudenko/OV7670-ILI9341-STM32F407VET6) 实际代码分析
>
> MCU: **STM32F407VET6** (LQFP100)

---

## 目录

1. [总览](#总览)
2. [ILI9341 LCD 接线（SPI）](#1-ili9341-lcd-接线spi)
3. [OV7670 摄像头接线（DCMI + I2C）](#2-ov7670-摄像头接线dcmi--i2c)
4. [SD 卡接线（SDIO）](#3-sd-卡接线sdio)
5. [按键及其他](#4-按键及其他)
6. [外设资源汇总](#5-外设资源汇总)
7. [常见问题](#6-常见问题)

---

## 总览

本项目的三大外设占用的主要外设资源：

| 外设 | 接口 | 主要 STM32 外设 | DMA |
|------|------|-----------------|-----|
| ILI9341 | **SPI** | SPI2 | DMA1 Stream4 (TX) |
| OV7670 | **DCMI + I2C** | DCMI + I2C2 + TIM5 | DMA2 Stream1 |
| SD Card | **SDIO** | SDIO + FatFs | — |

---

## 1. ILI9341 LCD 接线（SPI）

ILI9341 通过 **SPI2** 接口连接，使用 **DMA1 Stream4** 进行数据传输（发送），采用软件管理 CS 片选。

### 引脚连接表

| ILI9341 | STM32F407 引脚 | 功能 | 备注 |
|---------|---------------|------|------|
| **SCK** | **PB13** | SPI2_SCK | AF5 |
| **MOSI** (SDI) | **PB15** | SPI2_MOSI | AF5 |
| **MISO** (SDO) | **PC2** | SPI2_MISO | AF5，读 GRAM 时用 |
| **CS** (CSX) | **PB12** | GPIO 输出 | 软件控制，低电平有效 |
| **DC** (DCX) | **PD9** | GPIO 输出 | 数据/命令选择 |
| **RESET** (RESX) | **PD10** | GPIO 输出 | 复位，低电平有效 |
| **LED** (背光) | **PA7** | GPIO 输出高电平 | 已修改为一直置1，常亮 |
| **GND** | GND | 电源地 | |
| **VCC** | 5V / 3.3V | 电源 | 根据模块实际要求 |

### SPI 配置参数

- **SPI 模式**: 主机模式 (Master)
- **时钟极性 (CPOL)**: Low
- **时钟相位 (CPHA)**: 1 Edge
- **波特率预分频**: 2 （即 42 MHz / 2 = 21 MHz）
- **数据大小**: 8位
- **NSS**: 软件管理
- **DMA TX**: DMA1 Stream4, Channel 0, 半字对齐

### 驱动初始化（[CAMERA_APP.c:76](Core/Src/CAMERA_APP.c#L76)）

```c
ILI9341_Init(&hspi2, ILI9341_PIXEL_FMT_RGB565);
```

---

## 2. OV7670 摄像头接线（DCMI + I2C）

OV7670 通过 **DCMI**（数字摄像头接口）接收图像数据，通过 **I2C2**（SCCB 协议兼容）配置寄存器，XCLK 由 **TIM5_CH3** 提供。

### 引脚连接表

| OV7670 | STM32F407 引脚 | 功能 | 备注 |
|--------|---------------|------|------|
| **XCLK** | **PA2** | TIM5_CH3 输出 (OCU Toggle) | 42 MHz 方波 |
| **PCLK** | **PA6** | DCMI_PIXCLK | 像素时钟输入 |
| **VSYNC** | **PB7** | DCMI_VSYNC | 帧同步 |
| **HREF** (HSYNC) | **PA4** | DCMI_HSYNC | 行同步 |
| **D0** | **PC6** | DCMI_D0 | 数据位 0 |
| **D1** | **PC7** | DCMI_D1 | 数据位 1 |
| **D2** | **PE0** | DCMI_D2 | 数据位 2 |
| **D3** | **PE1** | DCMI_D3 | 数据位 3 |
| **D4** | **PE4** | DCMI_D4 | 数据位 4 |
| **D5** | **PB6** | DCMI_D5 | 数据位 5 |
| **D6** | **PE5** | DCMI_D6 | 数据位 6 |
| **D7** | **PE6** | DCMI_D7 | 数据位 7 |
| **SIO_C** (SCL) | **PB10** | I2C2_SCL | SCCB 时钟 (400 kHz) |
| **SIO_D** (SDA) | **PB11** | I2C2_SDA | SCCB 数据 |
| **RESET** | **PD11** | GPIO 输出 | 复位，低电平复位 |
| **PWDN** | **PD12** | GPIO 输出 | 掉电模式，高电平掉电 |

> ⚠️ **注意**: OV7670 头文件 `ov7670.h` 中的注释示例标注 D2→PC8、D3→PC9 是**错误的**。实际代码 [dcmi.c:85-86](Core/Src/dcmi.c#L85) 中 DCMI 配置为 D2→PE0、D3→PE1。PC8/PC9 已用于 SDIO。

### DCMI 配置参数

- **同步模式**: 硬件同步 (HSYNC/VSYNC)
- **PCLK 极性**: 上升沿采样
- **VSYNC 极性**: 高电平有效
- **HSYNC 极性**: 低电平有效
- **捕获速率**: 所有帧
- **数据宽度**: 8位
- **DMA**: DMA2 Stream1, 字对齐

### XCLK 时钟配置（[tim.c:46-49](Core/Src/tim.c#L46)）

```c
htim5.Instance = TIM5;
htim5.Init.Prescaler = 0;        // 预分频 = 1
htim5.Init.Period = 1;           // ARR = 1
htim5.Init.ClockDivision = DIV1; // 84 MHz 时钟
// TIM5_CH3 配置为 Toggle on Match, Pulse = 1
// => XCLK = 84 MHz / (1+1) = 42 MHz
```

### I2C2 配置参数

- **时钟速度**: 400 kHz（快速模式）
- **地址位**: 7位
- **OV7670 SCCB 地址**: 0x42

### 驱动初始化（[CAMERA_APP.c:94](Core/Src/CAMERA_APP.c#L94)）

```c
OV7670_Init(&hdcmi, &hi2c2, &htim5, TIM_CHANNEL_3);
```

---

## 3. SD 卡接线（SDIO）

SD 卡使用 **SDIO** 接口，配合 **FatFs** 文件系统实现图片读写。

### 引脚连接表

| SD 卡 | STM32F407 引脚 | 功能 | 备注 |
|------|---------------|------|------|
| **D0** | **PC8** | SDIO_D0 | 数据线 0，上拉 |
| **D1** | **PC9** | SDIO_D1 | 数据线 1，上拉 |
| **D2** | **PC10** | SDIO_D2 | 数据线 2，上拉 |
| **D3** | **PC11** | SDIO_D3 | 数据线 3，上拉 |
| **CLK** | **PC12** | SDIO_CK | 时钟线 |
| **CMD** | **PD2** | SDIO_CMD | 命令线，上拉 |
| **DETECT** | **PA1** | GPIO 输入 | 卡检测 (可选) |
| **GND** | GND | 电源地 | |
| **VCC** | 3.3V | 电源 | 3.3V 供电 |

### SDIO 配置参数

- **时钟分频**: 4 （SDIO_CK = 48 MHz / (4+2) = **8 MHz**）
- **时钟边沿**: 上升沿
- **总线宽度**: 1位（当前代码实际使用，注释为解决 HAL 4位模式 bug）
- **硬件流控**: 禁用

---

## 4. 按键及其他

| 功能 | STM32F407 引脚 | 描述 |
|------|---------------|------|
| 左按键 (BTN1) | **PC0** | EXTI0 下降沿/上升沿中断 |
| 右按键 (BTN2) | **PC1** | EXTI1 下降沿/上升沿中断 |
| 复位按键 (BTN_RST) | **PE3** | EXTI3 下降沿中断，系统复位 |
| 相机 LED | **PB8** | TIM10_CH1 (未使用) |
| 背光 PWM | **PA7** | TIM14_CH1 (代码中已注释掉) |

---

## 5. 外设资源汇总

| STM32 外设 | Instance | 用途 |
|-----------|----------|------|
| **SPI2** | SPI2 | ILI9341 显示数据传输 |
| **DCMI** | DCMI | OV7670 图像数据捕获 |
| **I2C2** | I2C2 | OV7670 SCCB 寄存器配置 |
| **SDIO** | SDIO | SD 卡读写 |
| **TIM5** | TIM5_CH3 | OV7670 XCLK 时钟 (42 MHz) |
| **TIM11** | TIM11 | 按键消抖定时器 (10ms) |
| **TIM14** | TIM14_CH1 | LCD 背光 PWM (未启用) |
| **TIM10** | TIM10_CH1 | 相机 LED (未启用) |
| **DMA1** | Stream4 (SPI2_TX) | ILI9341 SPI 发送 |
| **DMA2** | Stream1 (DCMI) | OV7670 图像 DMA 传输 |

---

## 6. 常见问题

### Q1: XCLK 是 PC2 还是 PA2？

**PA2**，不是 PC2。

`ov7670.h` 文件注释中有一处笔误写成了 PC2，但实际代码 [tim.c:253-260](Core/Src/tim.c#L253) 中 TIM5_CH3 正确配置在 **PA2**（复用功能 AF2）。在 STM32F407 上，TIM5_CH3 只有 PA2 一个选项。

### Q2: OV7670 数据线 D2/D3 与 SDIO 冲突吗？

不冲突。虽然 `ov7670.h` 的注释写着 D2→PC8、D3→PC9，但实际代码使用 **PE0→D2、PE1→D3**。PC8/PC9 留给 SDIO 使用。

### Q3: XCLK 频率为什么是 42 MHz？

TIM5 时钟源为 84 MHz，配置为 Prescaler=0 (÷1)、ARR=1（计数 0→1 再翻转），Toggle on Match 模式下引脚每周期翻转两次：
```
XCLK = 84 MHz / (1 + 1) = 42 MHz
```
这是 OV7670 支持的最高 XCLK 频率。

### Q4: 硬件接线需要注意什么？

- OV7670 是 3.3V 器件，确保供电电压正确
- SPI2 走线不宜过长（21 MHz 时钟），建议使用杜邦线时控制在 10cm 以内
- DCMI 数据线建议等长布线，保证信号同步
- SDIO 的 D0-D3 和 CMD 需要外接上拉电阻（或使用 STM32 内部上拉，代码已配置 `GPIO_PULLUP`）
- ILI9341 背光 LED 引脚如果不需要 PWM 调光，可直接接 3.3V
