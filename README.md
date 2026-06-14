# 项目介绍
STM32F407VET6采集OV7670图像并显示在ili9341(320*240)屏幕上或发送到vofa+显示

# 硬件
- 主控：STM32F407VET6
- 摄像头：OV7670
- 屏幕：ili9341(320*240)

# 硬件架构
- 主控 - I2C或者DCMI- OV7670
- 主控 - SPI - ili9341
- 主控 - UART - vofa+

# 软件架构
- App 所有应用层代码
- stb-master 图像压缩库
- 其余代码均为STM32CubeMX生成

## 使用方法
1. 用cmake编译
2. 用STM32CubeProgrammer下载工程

## 工作流程
系统初始化 -> DCMI采集一帧图像 -> LCD屏幕显示图片
                              -> vofa+显示图片