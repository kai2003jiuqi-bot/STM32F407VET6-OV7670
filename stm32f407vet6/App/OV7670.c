/*
 * 简述：
 * 基于STM32F407VET6；
 * OV7670摄像头驱动，采用QQVGA分辨率160x120，采用DCMI的循环DMA采集模式；
 * 采集一帧图像会触发一次中断；
 * 帧回调中通过RGB565双线性插值将160x120放大到320x240并显示。
 *
 * 引脚连接（OV7670 → STM32F407VET6）：
 *   XCLK  →  PA2    (TIM5_CH3, 42MHz)
 *   PCLK  →  PA6    (DCMI_PIXCLK)
 *   VSYNC →  PB7    (DCMI_VSYNC)
 *   HREF  →  PA4    (DCMI_HSYNC)
 *   D0    →  PC6    (DCMI_D0)
 *   D1    →  PC7    (DCMI_D1)
 *   D2    →  PE0    (DCMI_D2)
 *   D3    →  PE1    (DCMI_D3)
 *   D4    →  PE4    (DCMI_D4)
 *   D5    →  PB6    (DCMI_D5)
 *   D6    →  PE5    (DCMI_D6)
 *   D7    →  PE6    (DCMI_D7)
 *   SCL   →  PB10   (I2C2_SCL)
 *   SDA   →  PB11   (I2C2_SDA)
 *   RST   →  PD11   (GPIO输出)
 *   PWDN  →  PD12   (GPIO输出)
 *
 * 使用方法：
 * 0、配置好I2C、XCLK（用定时器输出42MHX方波、DCMI；
 *    编写帧触发中断HAL_DCMI_FrameEventCallback函数；
 *    如需移植的话，可更改全局函数；
 * 1、调用OV7670_Init()初始化摄像头；
 * 2、调用OV7670_Start()开始采集图像；
 * 3、当采集完一帧后触发帧中断HAL_DCMI_FrameEventCallback；
 *    完整的图像会保存在数组capture_data中；
 *    帧回调中自动对160x120做RGB565双线性插值放大到320x240并显示；
 * 4、可以选择调用OV7670_Stop()停止采集图像；
 *
 */

#include "OV7670.h"
#include "OV7670_reg.h"
#include "rgb565_utils.h"
#include "ili9341_display.h"
#include "dcmi.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"
#include "log.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <string.h>

// 全局变量
uint8_t capture_data[OV7670_WIDTH * OV7670_HEIGHT * 2]; // 存储捕获的一帧图像

static uint8_t OV7670_SCCB_Write(uint8_t regAddr, uint8_t data);
static uint8_t OV7670_SCCB_Read(uint8_t regAddr, uint8_t *data);
static void OV7670_Delay(uint32_t time);
static void OV7670_XLK_Enable(void);
static void OV7670_XLK_Disable(void);
static void OV7670_W_PWDN(uint8_t value);
static void OV7670_W_RST(uint8_t value);

/* ==================================================================== */
/*                           Public API                                  */
/* ==================================================================== */

/**
 * @brief 初始化OV7670
 */
void OV7670_Init(void)
{
    // 1: PWDN拉低（正常工作模式）
    OV7670_W_PWDN(0);
    // 2: RST拉低50ms → 拉高50ms（硬件复位）
    OV7670_W_RST(0);
    OV7670_Delay(50);
    OV7670_W_RST(1);
    OV7670_Delay(50);
    // 3: 开启XCLK，SCCB通信需要时钟
    OV7670_XLK_Enable();
    // 4: SCCB软复位（COM7 bit0=1）
    OV7670_SCCB_Write(OV7670_REG_COM7, 0x80);
    OV7670_Delay(30);
    // 5: 读设备ID，VER应为0x73
    uint8_t buf[4] = {0};
    uint8_t ret = OV7670_SCCB_Read(OV7670_REG_VER, buf);
    log_info("OV7670", "dev id = 0x%02X (ret=%d)", buf[0], ret);
    // 6: 检测到设备，写入全部寄存器配置
    if (ret == HAL_OK)
    {
        OV7670_SCCB_Write(OV7670_REG_COM7, 0x80);
        OV7670_Delay(30);
        for (uint32_t i = 0; OV7670_reg[i][0] != OV7670_REG_DUMMY; i++)
        {
            OV7670_SCCB_Write(OV7670_reg[i][0], OV7670_reg[i][1]);
            OV7670_Delay(1);
        }
    }
    else
    {
        log_info("OV7670", "NOT DETECTED, skipping config");
    }
    // 7: 关闭XCLK（Start时会重新开启）
    OV7670_XLK_Disable();
}

/**
 * @brief 启动OV7670，开始捕获图像
 */
void OV7670_Start(void)
{
    OV7670_XLK_Enable();
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS,
                       (uint32_t)capture_data,
                       OV7670_WIDTH * OV7670_HEIGHT / 2);
}

/**
 * @brief 停止OV7670，停止捕获图像
 */
void OV7670_Stop(void)
{
    HAL_DCMI_Stop(&hdcmi);
    OV7670_XLK_Disable();
}

/**
 * @brief 帧完成中断（VSYNC上升沿）—— 将QQVGA图像放大到全屏显示。
 *
 * 算法概览（纯整数 RGB565 双线性插值，逐行处理）：
 *
 *   输出像素 (dx, dy) 对应输入像素 (dx/2, dy/2)：
 *
 *           dx偶, dy偶 → 直接复制输入像素
 *           dx奇, dy偶 → 水平平均 (p0 + p1)/2
 *           dx偶, dy奇 → 垂直平均 (p0 + p2)/2
 *           dx奇, dy奇 → 四角平均 (p0+p1+p2+p3)/4
 *
 *   内存优化：不放完整输出帧缓冲，每处理完1个输入行
 *   （产生2个输出行）立刻送显，总临时栈~1.3KB。
 */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    extern xQueueHandle OV7670QueueHandle;
    uint8_t val = 1;
    xQueueSendFromISR(OV7670QueueHandle, &val, NULL);
}

/* ==================================================================== */
/*                           Global Function                            */
/* ==================================================================== */

/**
 * @brief SCCB写寄存器
*/
static uint8_t OV7670_SCCB_Write(uint8_t regAddr, uint8_t data)
{
    uint8_t ret = HAL_ERROR;
    for (int i = 0; i < 3; i++)
    {
        // 1: I2C写入（寄存器地址+数据）
        ret = HAL_I2C_Mem_Write(&hi2c2, OV7670_SCCB_ADDR, regAddr,
                I2C_MEMADD_SIZE_8BIT, &data, 1U, 100U);
        if (ret == HAL_OK) break;
    }
    return ret;
}


/**
 * @brief SCCB读寄存器
*/
static uint8_t OV7670_SCCB_Read(uint8_t regAddr, uint8_t *data)
{
    uint8_t ret = HAL_ERROR;
    for (int i = 0; i < 3; i++)
    {
        // 1: 发寄存器地址（带STOP）
        ret = HAL_I2C_Master_Transmit(&hi2c2, OV7670_SCCB_ADDR, &regAddr, 1U, 100U);
        if (ret != HAL_OK) continue;
        // 2: 重新START + 读数据
        ret = HAL_I2C_Master_Receive(&hi2c2, OV7670_SCCB_ADDR, data, 1U, 100U);
        if (ret == HAL_OK) break;
    }
    return ret;
}

/**
 * @brief 毫秒延时
*/
void OV7670_Delay(uint32_t time)
{
    HAL_Delay(time);
}

/**
 * @brief 控制PWDN引脚
*/
static void OV7670_W_PWDN(uint8_t value)
{
    HAL_GPIO_WritePin(OV7670_PWDN_GPIO_Port, OV7670_PWDN_Pin,
                      value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}


/**
 * @brief 硬件复位
*/
static void OV7670_W_RST(uint8_t value)
{
    HAL_GPIO_WritePin(OV7670_RST_GPIO_Port, OV7670_RST_Pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
 * @brief 开启XCLK时钟
*/
static void OV7670_XLK_Enable(void)
{
    TIM5->CCER |= TIM_CCER_CC3E;
    TIM5->CR1 |= TIM_CR1_CEN;
}


/**
 * @brief 关闭XCLK时钟
*/
static void OV7670_XLK_Disable(void)
{
    TIM5->CCER &= ~TIM_CCER_CC3E;
    TIM5->CR1 &= ~TIM_CR1_CEN;
}
