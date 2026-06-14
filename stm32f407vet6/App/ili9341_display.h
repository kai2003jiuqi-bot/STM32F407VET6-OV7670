/**
 * @file ili9341_display.h
 * @brief           :  ILI9341显示屏驱动, 包含STM32F407VET6的引脚定义，以及屏幕方向的选择
 *              
 * @date 2025-11-12
 * 
 * 
 */
#ifndef __ili9341_display_H
#define __ili9341_display_H

#include "spi.h"
#include "gpio.h"

// 更改这里，调整、屏幕方向
#define USE_HORIZONTAL 3  // 0: 正竖屏 1: 反竖屏 2: 正横屏 3: 反横屏

void ili9341_display_hardware_reset(void);     
void ili9341_display_write_index(uint8_t Index);
void ili9341_display_write_data_8bit(uint8_t Data);
void ili9341_display_write_data_16bit(uint16_t Data);
void ili9341_display_write_data_bulk_16bit(uint16_t *data, uint32_t len);
void ili9341_display_set_xy(uint16_t Xpos, uint16_t Ypos);
void ili9341_display_set_region(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd);
void ili9341_display_init(void);
void ili9341_display_drawPoint(uint16_t x, uint16_t y, uint16_t color);
void ili9341_display_clear(uint16_t color);

// 设置屏幕方向
#if (USE_HORIZONTAL==0 || USE_HORIZONTAL==1)
    #define X_MAX_PIXEL  240
    #define Y_MAX_PIXEL  320
#endif

#if( USE_HORIZONTAL==2 || USE_HORIZONTAL==3)
    #define X_MAX_PIXEL  320
    #define Y_MAX_PIXEL  240
#endif

// 颜色宏定义
#define ILI9341_BLACK   0x0000
#define ILI9341_WHITE   0xFFFF
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_BLUE    0x001F
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F

// ILI9341 寄存器宏定义
#define ILI9341_CMD_SLPOUT        0x11    // 睡眠退出
#define ILI9341_CMD_CF            0xCF    // 电源控制B
#define ILI9341_CMD_ED            0xED    // 电源控制A
#define ILI9341_CMD_E8            0xE8    // 驱动时序控制A
#define ILI9341_CMD_F6            0xF6    // 接口控制
#define ILI9341_CMD_CB            0xCB    // 电源控制B
#define ILI9341_CMD_F7            0xF7    // 驱动时序控制B
#define ILI9341_CMD_EA            0xEA    // 驱动时序控制C
#define ILI9341_CMD_C0            0xC0    // 面板控制
#define ILI9341_CMD_C1            0xC1    // 电源控制1
#define ILI9341_CMD_C5            0xC5    // 电源控制2
#define ILI9341_CMD_C7            0xC7    // 电源控制3
#define ILI9341_CMD_PIXFMT        0x3A    // 像素格式设置
#define ILI9341_CMD_MADCTL        0x36    // 内存访问控制
#define ILI9341_CMD_FRMCTR1       0xB1    // 帧速率控制1
#define ILI9341_CMD_DFUNCTR       0xB4    // 显示功能控制
#define ILI9341_CMD_GAMMASET      0xF2    // 伽马设置
#define ILI9341_CMD_GAMMA_CURV    0x26    // 伽马曲线选择
#define ILI9341_CMD_PGAMMA        0xE0    // 正伽马校正
#define ILI9341_CMD_NGAMMA        0xE1    // 负伽马校正
#define ILI9341_CMD_DISPON        0x29    // 显示开启
#define ILI9341_CMD_CASET         0x2A    // 列地址设置
#define ILI9341_CMD_PASET         0x2B    // 页地址设置
#define ILI9341_CMD_RAMWR         0x2C    // 内存写入
#define ILI9341_SLPOUT_PARAM        0x00    // 睡眠退出无意义参数（占位用）

// CF命令（0xCF，电源控制B）参数
#define ILI9341_CF_PARAM1           0x00    // 内部电压调整基础值
#define ILI9341_CF_PARAM2           0xC1    // 电源控制B第二参数
#define ILI9341_CF_PARAM3           0x30    // 电源控制B第三参数

// ED命令（0xED，电源控制A）参数
#define ILI9341_ED_PARAM1           0x64    // 电源控制A第一参数
#define ILI9341_ED_PARAM2           0x03    // 电源控制A第二参数
#define ILI9341_ED_PARAM3           0x12    // 电源控制A第三参数
#define ILI9341_ED_PARAM4           0x81    // 电源控制A第四参数

// E8命令（0xE8，驱动时序控制A）参数
#define ILI9341_E8_PARAM1           0x85    // 驱动时序A第一参数
#define ILI9341_E8_PARAM2           0x11    // 驱动时序A第二参数
#define ILI9341_E8_PARAM3           0x78    // 驱动时序A第三参数

// F6命令（0xF6，接口控制）参数
#define ILI9341_F6_PARAM1           0x01    // 接口数据传输模式
#define ILI9341_F6_PARAM2           0x30    // 接口时序调整
#define ILI9341_F6_PARAM3           0x00    // 默认接口时序（关闭特殊模式）

// CB命令（0xCB，电源控制1）参数
#define ILI9341_CB_PARAM1           0x39    // 电源控制1第一参数
#define ILI9341_CB_PARAM2           0x2C    // 电源控制1第二参数
#define ILI9341_CB_PARAM3           0x00    // 电源控制1第三参数
#define ILI9341_CB_PARAM4           0x34    // 电源控制1第四参数
#define ILI9341_CB_PARAM5           0x05    // 电源控制1第五参数

// F7命令（0xF7，驱动时序控制B）参数
#define ILI9341_F7_PARAM            0x20    // 驱动时序B参数

// EA命令（0xEA，厂商自定义扩展命令）参数
#define ILI9341_EA_PARAM1           0x00    // 扩展命令默认参数1
#define ILI9341_EA_PARAM2           0x00    // 扩展命令默认参数2

// C0命令（0xC0，亮度控制1）参数
#define ILI9341_C0_PARAM            0x20    // 亮度控制基础值

// C1命令（0xC1，亮度控制2）参数
#define ILI9341_C1_PARAM            0x11    // 亮度控制微调值

// C5命令（0xC5，色温控制）参数
#define ILI9341_C5_PARAM1           0x31    // 色温控制第一参数
#define ILI9341_C5_PARAM2           0x3C    // 色温控制第二参数

// C7命令（0xC7，负色温控制）参数
#define ILI9341_C7_PARAM            0xA9    // 负色温控制参数

// PIXFMT（像素格式）命令参数
#define ILI9341_PIXFMT_16BIT        0x55    // 16位RGB565像素格式（常用）

// FRMCTR1（帧速率控制1）命令参数
#define ILI9341_FRMCTR1_PARAM1      0x00    // 帧速率控制基础值
#define ILI9341_FRMCTR1_PARAM2      0x18    // 帧速率调整（约60Hz）

// DFUNCTR（显示功能控制）命令参数
#define ILI9341_DFUNCTR_PARAM1      0x00    // 显示功能控制1
#define ILI9341_DFUNCTR_PARAM2      0x00    // 显示功能控制2

// GAMMASET（伽马设置）命令参数
#define ILI9341_GAMMASET_PARAM      0x00    // 伽马曲线选择（默认曲线）

// GAMMA_CURV（伽马曲线启用）命令参数
#define ILI9341_GAMMA_CURV_PARAM    0x01    // 启用用户自定义伽马曲线

// 内存访问控制方向宏定义（对应 USE_HORIZONTAL 配置）
#define ILI9341_MADCTL_PORTRAIT_NORMAL  0x88    // 正竖屏：MY=0, MX=1, MV=0, BGR=1
#define ILI9341_MADCTL_PORTRAIT_REV    0x48    // 反竖屏：MY=1, MX=0, MV=0, BGR=1
#define ILI9341_MADCTL_LANDSCAPE_NORMAL 0xE8   // 正横屏：MY=0, MX=0, MV=1, BGR=1
#define ILI9341_MADCTL_LANDSCAPE_REV   0x28    // 反横屏：MY=1, MX=1, MV=1, BGR=1

#endif