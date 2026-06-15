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
-   OV7670 → STM32F407VET6：
-   XCLK  →  PA2    (TIM5_CH3, 42MHz)
-   PCLK  →  PA6    (DCMI_PIXCLK)
-   VSYNC →  PB7    (DCMI_VSYNC)
-   HREF  →  PA4    (DCMI_HSYNC)
-   D0    →  PC6    (DCMI_D0)
-   D1    →  PC7    (DCMI_D1)
-   D2    →  PE0    (DCMI_D2)
-   D3    →  PE1    (DCMI_D3)
-   D4    →  PE4    (DCMI_D4)
-   D5    →  PB6    (DCMI_D5)
-   D6    →  PE5    (DCMI_D6)
-   D7    →  PE6    (DCMI_D7)
-   SCL   →  PB10   (I2C2_SCL)
-   SDA   →  PB11   (I2C2_SDA)
-   RST   →  PD11   (GPIO输出)
-   PWDN  →  PD12   (GPIO输出)

- ILI9341 → STM32F407VET6：
- SCK → PB13        (SPI2_SCK, AF5)
- MOSI → PB15       (SPI2_MOSI, AF5)
- MISO → PC2        (SPI2_MISO, AF5，用于读取 GRAM)
- CS → PB12         (GPIO 输出，低电平有效)
- DC → PD9          (GPIO 输出，数据 / 命令选择)
- RESET → PD10      (GPIO 输出，低电平复位)
- LED → PA7         (GPIO 输出，高电平背光常亮)
- GND → GND         (电源地)
- VCC → 3.3V/5V     (模块供电)

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
- [K-Rudenko/OV7670-ILI9341-STM32F407VET6](https://github.com/kostia-r/OV7670-ILI9341-STM32F407VET6)
- [ComputerNerd/ov7670-no-ram-arduino-uno](https://github.com/ComputerNerd/ov7670-no-ram-arduino-uno)