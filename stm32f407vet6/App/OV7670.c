/*
 * 简述：
 * OV7670摄像头驱动，采用QQVGA分辨率160x120，采用DCMI的循环DMA采集模式
 * 采集一帧图像会触发一次中断。
 * 使用方法：
 * 0、编写帧触发中断HAL_DCMI_FrameEventCallback函数；
 *    如需移植的话，可更改全局函数
 * 1、调用OV7670_Init()初始化摄像头
 * 2、调用OV7670_Start()开始采集图像
 * 3、当采集完一帧后触发帧中断HAL_DCMI_FrameEventCallback，
 *    完整的图像会保存在数组capture_data中，
 * 4、可以选择调用OV7670_Stop()停止采集图像
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

static uint8_t capture_data[OV7670_WIDTH * OV7670_HEIGHT * 2];

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
    // 1: pull PWDN low → normal operation
    OV7670_W_PWDN(0);
    // 2: hardware reset (RST low 50ms → high 50ms)
    OV7670_W_RST(0);
    OV7670_Delay(50);
    OV7670_W_RST(1);
    OV7670_Delay(50);
    // 3: start XCLK for SCCB communication
    OV7670_XLK_Enable();
    // 4: soft reset via SCCB
    OV7670_SCCB_Write(OV7670_REG_COM7, 0x80);
    OV7670_Delay(30);
    // 5: read device ID (VER should be 0x73)
    uint8_t buf[4] = {0};
    uint8_t ret = OV7670_SCCB_Read(OV7670_REG_VER, buf);
    log_info("OV7670", "dev id = 0x%02X (ret=%d)", buf[0], ret);
    // 6: if detected, write all registers
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
    // 7: stop XCLK (will restart on Start)
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
 * @brief 帧完成中断回调（VSYNC下降沿触发时调用）
*/
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    // write to LCD 
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
        ret = HAL_I2C_Master_Transmit(&hi2c2, OV7670_SCCB_ADDR, &regAddr, 1U, 100U);
        if (ret != HAL_OK) continue;
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
