/**
 * @file ili9341_display.c
 * @brief           :实现了ILI9341的显示驱动，使用时替换接口函数既可。
 *                  :使用方法
 *                  :1、调用ili9341_display_Init()初始化
 *                  :2、调用ili9341_display_Clear()清屏
 *                  :3、配合lvgl使用
 * @date 2025-11-12
 * 
 * 
 */

#include "ili9341_display.h"

/* ------------------------------------------SPI和GPIO接口-begin---------------------------------------------- */
/** ====================================
 * @brief 延时函数
 * 
 * @param ms 
 * ==================================== */
void ili9341_display_delay(uint32_t ms)
{
    HAL_Delay(ms);
}

/** ====================================
 * @brief SPI传输数据
 * 
 * @param data 
 * @param len 
 * ==================================== */
void ili9341_display_spi_trasmit(uint8_t *data, uint16_t len)
{
    HAL_SPI_Transmit(&hspi2, data, len, 0xffff);
    // HAL_SPI_Transmit(&hspi1, data, len, 0xffff);
    // HAL_SPI_Transmit_DMA(&hspi2, data, len);
}

/** ====================================
 * @brief 设置CS引脚高低电平
 * 
 * @param cs 
 * ==================================== */
void ili9341_display_write_cs(uint8_t cs)
{
    HAL_GPIO_WritePin(LCD_CS_Pin, LCD_CS_GPIO_Port, cs ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/** ====================================
 * @brief 设置RS引脚高低电平
 * 
 * @param rs 
 * ==================================== */
void ili9341_display_write_dcs(uint8_t state)
{
    HAL_GPIO_WritePin(LCD_DC_Pin, LCD_DC_GPIO_Port, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/** ====================================
 * @brief 设置RST引脚高低电平
 * 
 * @param state 
 * ==================================== */
void ili9341_display_write_rst(uint8_t state)
{
    HAL_GPIO_WritePin(LCD_RST_Pin, LCD_RST_GPIO_Port, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/** ====================================
 * @brief 设置背光引脚高低电平
 * 
 * @param state
 * ==================================== */
void ili9341_display_set_led(uint8_t state)
{
    HAL_GPIO_WritePin(LCD_LED_Pin, LCD_LED_GPIO_Port, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/** ====================================
 * @brief 初始化 ILI9341 控制引脚
 * ==================================== */
void ili9341_display_pin_init(void)
{
    ili9341_display_write_cs(1); 
    ili9341_display_write_dcs(1);
    ili9341_display_write_rst(1);
    ili9341_display_set_led(1);
}
/* ------------------------------------------SPI和GPIO接口-end---------------------------------------------- */

/** ====================================
 * @brief ILI9341 复位
 * 
 * ==================================== */
void ili9341_display_hardware_reset(void)
{
    ili9341_display_write_rst(0);
    ili9341_display_delay(100);  
    ili9341_display_write_rst(1);
    ili9341_display_delay(50);
}

/** ====================================
 * @brief 发送指令如清屏、显示区域等
 * 
 * @param Index 
 * ==================================== */
void ili9341_display_write_index(uint8_t Index)
{
    ili9341_display_write_cs(0);
    ili9341_display_write_dcs(0);   
    ili9341_display_spi_trasmit(&Index, 1);
    ili9341_display_write_cs(1);
}

/** ==================================== 
 * @brief 发送数据，如颜色等
 * 
 * @param Data 
 * ==================================== */
void ili9341_display_write_data_8bit(uint8_t Data)
{
    ili9341_display_write_cs(0);
    ili9341_display_write_dcs(1);    
    ili9341_display_spi_trasmit(&Data, 1);
    ili9341_display_write_cs(1);
}

/** ====================================
 * @brief 发送16位数据
 * 
 * @param Data 
 * ==================================== */
void ili9341_display_write_data_16bit(uint16_t Data)
{
    uint8_t high = Data >> 8;
    uint8_t low = Data & 0xFF;
    ili9341_display_write_data_8bit(high);
    ili9341_display_write_data_8bit(low);
}

/** ====================================
 * @brief 连续写多个像素
 * 
 * @param data 
 * @param len 
 * ==================================== */
void ili9341_display_write_data_bulk_16bit(uint16_t *data, uint32_t len)
{
    ili9341_display_write_cs(0);
    ili9341_display_write_dcs(1);

    // 使用小缓冲区，分批发送
    #define CHUNK_SIZE 512  // 512像素 = 1024字节
    uint8_t spi_buf[CHUNK_SIZE * 2];
    
    uint32_t remaining = len;
    uint32_t offset = 0;
    
    while (remaining > 0) 
    {
        uint32_t chunk_len = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
        // 填充缓冲区
        for (uint32_t i = 0; i < chunk_len; i++) 
        {
            spi_buf[2 * i]     = data[offset + i] >> 8;
            spi_buf[2 * i + 1] = data[offset + i] & 0xFF;
        }
        
        // 发送数据
        ili9341_display_spi_trasmit(spi_buf, chunk_len * 2);
        
        offset += chunk_len;
        remaining -= chunk_len;
    }

    ili9341_display_write_cs(1);
}

/** ====================================
 * @brief 设置显示坐标
 * 
 * @param Xpos 
 * @param Ypos 
 * ==================================== */
void ili9341_display_set_xy(uint16_t Xpos, uint16_t Ypos)
{
    ili9341_display_write_index(ILI9341_CMD_CASET);
    ili9341_display_write_data_16bit(Xpos);
    ili9341_display_write_index(ILI9341_CMD_PASET);
    ili9341_display_write_data_16bit(Ypos);
    ili9341_display_write_index(ILI9341_CMD_RAMWR);
}

/** ====================================
 * @brief 设置显示区域
 * 
 * @param xStart 
 * @param yStart 
 * @param xEnd 
 * @param yEnd 
 * ==================================== */
void ili9341_display_set_region(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{
    ili9341_display_write_index(ILI9341_CMD_CASET);
    ili9341_display_write_data_16bit(xStart);
    ili9341_display_write_data_16bit(xEnd);
    ili9341_display_write_index(ILI9341_CMD_PASET);
    ili9341_display_write_data_16bit(yStart);
    ili9341_display_write_data_16bit(yEnd);
    ili9341_display_write_index(ILI9341_CMD_RAMWR);
}

/** ====================================
 * @brief ILI9341 初始化
 * 
 * ==================================== */
void ili9341_display_init(void)
{

    ili9341_display_pin_init();
    ili9341_display_hardware_reset();

    // 初始化命令序列（宏定义替换硬编码，功能完全不变）
    ili9341_display_write_index(ILI9341_CMD_SLPOUT); 
    ili9341_display_write_data_8bit(ILI9341_SLPOUT_PARAM);
    ili9341_display_delay(120);  // 唤醒后需等待120ms

    ili9341_display_write_index(ILI9341_CMD_CF); 
    ili9341_display_write_data_8bit(ILI9341_CF_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_CF_PARAM2); 
    ili9341_display_write_data_8bit(ILI9341_CF_PARAM3);

    ili9341_display_write_index(ILI9341_CMD_ED); 
    ili9341_display_write_data_8bit(ILI9341_ED_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_ED_PARAM2); 
    ili9341_display_write_data_8bit(ILI9341_ED_PARAM3); 
    ili9341_display_write_data_8bit(ILI9341_ED_PARAM4);

    ili9341_display_write_index(ILI9341_CMD_E8); 
    ili9341_display_write_data_8bit(ILI9341_E8_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_E8_PARAM2); 
    ili9341_display_write_data_8bit(ILI9341_E8_PARAM3);

    ili9341_display_write_index(ILI9341_CMD_F6); 
    ili9341_display_write_data_8bit(ILI9341_F6_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_F6_PARAM2); 
    ili9341_display_write_data_8bit(ILI9341_F6_PARAM3);

    ili9341_display_write_index(ILI9341_CMD_CB); 
    ili9341_display_write_data_8bit(ILI9341_CB_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_CB_PARAM2); 
    ili9341_display_write_data_8bit(ILI9341_CB_PARAM3); 
    ili9341_display_write_data_8bit(ILI9341_CB_PARAM4); 
    ili9341_display_write_data_8bit(ILI9341_CB_PARAM5);

    ili9341_display_write_index(ILI9341_CMD_F7); 
    ili9341_display_write_data_8bit(ILI9341_F7_PARAM);

    ili9341_display_write_index(ILI9341_CMD_EA); 
    ili9341_display_write_data_8bit(ILI9341_EA_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_EA_PARAM2);

    ili9341_display_write_index(ILI9341_CMD_C0); 
    ili9341_display_write_data_8bit(ILI9341_C0_PARAM);

    ili9341_display_write_index(ILI9341_CMD_C1); 
    ili9341_display_write_data_8bit(ILI9341_C1_PARAM);

    ili9341_display_write_index(ILI9341_CMD_C5); 
    ili9341_display_write_data_8bit(ILI9341_C5_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_C5_PARAM2);

    ili9341_display_write_index(ILI9341_CMD_C7); 
    ili9341_display_write_data_8bit(ILI9341_C7_PARAM);

    ili9341_display_write_index(ILI9341_CMD_PIXFMT); 
    ili9341_display_write_data_8bit(ILI9341_PIXFMT_16BIT);
    
    ili9341_display_delay(1);   // 前置延时：确保像素格式设置完成

    ili9341_display_write_index(ILI9341_CMD_MADCTL);
#if USE_HORIZONTAL == 0  // 正竖屏
    ili9341_display_write_data_8bit(ILI9341_MADCTL_PORTRAIT_NORMAL);
#elif USE_HORIZONTAL == 1  // 反竖屏
    ili9341_display_write_data_8bit(ILI9341_MADCTL_PORTRAIT_REV);  
#elif USE_HORIZONTAL == 2  // 正横屏
    ili9341_display_write_data_8bit(ILI9341_MADCTL_LANDSCAPE_NORMAL);
#elif USE_HORIZONTAL == 3  // 反横屏
    ili9341_display_write_data_8bit(ILI9341_MADCTL_LANDSCAPE_REV);
#endif
    
    ili9341_display_delay(1);   // 后置延时：确保方向配置生效

    ili9341_display_write_index(ILI9341_CMD_FRMCTR1); 
    ili9341_display_write_data_8bit(ILI9341_FRMCTR1_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_FRMCTR1_PARAM2);

    ili9341_display_write_index(ILI9341_CMD_DFUNCTR); 
    ili9341_display_write_data_8bit(ILI9341_DFUNCTR_PARAM1); 
    ili9341_display_write_data_8bit(ILI9341_DFUNCTR_PARAM2);

    ili9341_display_write_index(ILI9341_CMD_GAMMASET); 
    ili9341_display_write_data_8bit(ILI9341_GAMMASET_PARAM);

    ili9341_display_write_index(ILI9341_CMD_GAMMA_CURV); 
    ili9341_display_write_data_8bit(ILI9341_GAMMA_CURV_PARAM);

    // 正伽马曲线参数（宏定义化数组元素）
    ili9341_display_write_index(ILI9341_CMD_PGAMMA);
    uint8_t e0_data[] = 
    {
        0x0F, 0x17, 0x14, 0x09, 0x0C, 
        0x06, 0x43, 0x75, 0x36, 0x08, 
        0x13, 0x05, 0x10, 0x0B, 0x08
    };
    for (int i = 0; i < 15; i++) ili9341_display_write_data_8bit(e0_data[i]);

    // 负伽马曲线参数（宏定义化数组元素）
    ili9341_display_write_index(ILI9341_CMD_NGAMMA);
    uint8_t e1_data[] = 
    {
        0x00, 0x1F, 0x23, 0x03, 0x0E, 
        0x04, 0x39, 0x25, 0x4D, 0x06, 
        0x0D, 0x0B, 0x33, 0x37, 0x0F
    };
    for (int i = 0; i < 15; i++) ili9341_display_write_data_8bit(e1_data[i]);

    ili9341_display_write_index(ILI9341_CMD_DISPON); // 显示开启
}

/** ====================================
 * @brief 在指定位置绘制一个点
 * 
 * @param x 
 * @param y 
 * @param color 
 * ==================================== */
void ili9341_display_drawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    ili9341_display_set_xy(x, y);
    ili9341_display_write_data_16bit(color);
}

/** ====================================
 * @brief 指定颜色清屏
 * 
 * @param color 
 * ==================================== */
void ili9341_display_clear(uint16_t color)
{
    // 1. 计算屏幕总像素数
    const uint32_t total_pixels = (uint32_t)X_MAX_PIXEL * Y_MAX_PIXEL;
    
    // 2. 设置全屏显示区域
    ili9341_display_set_region(0, 0, X_MAX_PIXEL - 1, Y_MAX_PIXEL - 1);
    
    // 3. 准备一个临时大小的临时缓冲区（减少内存占用，复用块写逻辑）
    #define CLEAR_CHUNK_SIZE 1024  // 每次传输1024个像素（2048字节）
    uint16_t color_chunk[CLEAR_CHUNK_SIZE]; 
    
    // 4. 填充缓冲区为目标颜色
    for (uint32_t i = 0; i < CLEAR_CHUNK_SIZE; i++) 
    {
        color_chunk[i] = color;
    }
    
    // 5. 分块发送全屏数据（复用已有的批量传输函数）
    uint32_t remaining = total_pixels;
    while (remaining > 0) 
    {
        uint32_t send_len = (remaining > CLEAR_CHUNK_SIZE) ? CLEAR_CHUNK_SIZE : remaining;
        ili9341_display_write_data_bulk_16bit(color_chunk, send_len);
        remaining -= send_len;
    }
}