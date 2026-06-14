/*
 * 简述：
 * 基于STM32F407VET6；
 * OV7670摄像头驱动，采用QQVGA分辨率160x120，采用DCMI的循环DMA采集模式；
 * 采集一帧图像会触发一次中断；
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
 * 4、可以选择调用OV7670_Stop()停止采集图像；
 */

#include "OV7670.h"
#include "OV7670_reg.h"
#include "ili9341_display.h"
#include "dcmi.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"
#include "log.h"
#include <string.h>

#define OV7670_WIDTH                  160U
#define OV7670_HEIGHT                 120U
#define OV7670_SCCB_ADDR              0x42U

static uint8_t capture_data[OV7670_WIDTH * OV7670_HEIGHT * 2]; // 存储捕获的一帧图像

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
    // 1: 开启XCLK，摄像头开始输出数据
    // 2: 启动CIRCULAR DMA，持续写入capture_data
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
    // 1: 停止DCMI（DMA也随之停止）
    // 2: 关闭XCLK
{
    HAL_DCMI_Stop(&hdcmi);
    OV7670_XLK_Disable();
}

/**
 * @brief 帧完成中断回调（VSYNC下降沿触发时调用）
*/
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    // 显示到LCD 
    uint8_t buff[OV7670_WIDTH * OV7670_HEIGHT * 2];
    memcpy(buff, capture_data, sizeof(buff));
    ILI9341_SetRegion(0, 0, OV7670_WIDTH - 1U, OV7670_HEIGHT - 1U);
    ILI9341_WritePixels((uint16_t*)buff, OV7670_WIDTH * OV7670_HEIGHT);
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


void OV7670_Init(void);
void OV7670_Start(void);
void OV7670_Stop(void);