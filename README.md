# 项目介绍
STM32F407VET6采集OV7670图像并显示在ili9341(320*240)屏幕上和发送到vofa+显示；
格式为QQVGA(160*120);

# 硬件
- 主控：STM32F407VET6
- 摄像头：OV7670
- 屏幕：ili9341(320*240)

# 硬件架构
- 主控 -I2C或DCMI- OV7670
- 主控 -SPI- ili9341
- 主控 -UART- vofa+

# 引脚连接
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

# 软件架构
- App 所有应用层代码
- stb-master 图像压缩库
- 其余代码均为STM32CubeMX生成

# 使用方法
1. 用cmake编译
2. 用STM32CubeProgrammer下载工程

# 工作流程
- 系统初始化 -> DCMI采集一帧图像 -> LCD屏幕显示图片
                              -> vofa+显示图片

# 致谢
- 原作者：[text](https://github.com/kostia-r/OV7670-ILI9341-STM32F407VET6)