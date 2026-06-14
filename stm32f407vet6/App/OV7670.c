/*
 * OV7670.c
 * OV7670 DCMI DMA Driver
 * Created on: Aug 5, 2024
 *     Author: K.Rudenko
 */

#include "OV7670.h"
#include "OV7670_reg.h"
#include "ili9341_display.h"
#include "dcmi.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"
#include "log.h"

#define OV7670_WIDTH                  160U
#define OV7670_HEIGHT                 120U
#define OV7670_SCCB_ADDR              0x42U

static volatile uint32_t   ov_buf_addr;
static volatile uint32_t   ov_line_cnt;
static volatile uint8_t    ov_state;
static uint8_t buffer[OV7670_WIDTH * OV7670_HEIGHT * 2];
static uint8_t OV7670_SCCB_Write(uint8_t regAddr, uint8_t data);
static uint8_t OV7670_SCCB_Read(uint8_t regAddr, uint8_t *data);
static void OV7670_Delay(uint32_t time);
static void OV7670_XLK_Enable(void);
static void OV7670_XLK_Disable(void);
static void OV7670_w_pwdn(uint8_t value);
static void OV7670_w_rst(void);

void OV7670_Init(void)
{
    OV7670_w_pwdn(0);
    OV7670_w_rst();
    /* Start camera XLK signal to be able to do initialization */
    OV7670_XLK_Enable();

    /* Do camera reset */
    OV7670_SCCB_Write(OV7670_REG_COM7, 0x80);
    OV7670_Delay(30);

    /* Get camera ID */
    uint8_t buf[4] = {0};
    uint8_t ret = OV7670_SCCB_Read(OV7670_REG_VER, buf);
    log_info("OV7670", "dev id = 0x%02X (ret=%d)", buf[0], ret);

    if (ret == HAL_OK)
    {
        /* Do camera reset */
        OV7670_SCCB_Write(OV7670_REG_COM7, 0x80);
        OV7670_Delay(30);

        /* Do camera configuration */
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
    /* Stop camera XLK signal */
    OV7670_XLK_Disable();

    /* Initialize buffer address */
    ov_buf_addr = (uint32_t) buffer;
}

void OV7670_Start(void)
{
    __disable_irq();
    /* Reset buffer address */
    ov_buf_addr = (uint32_t)buffer;
    /* Reset line counter */
    ov_line_cnt = 0U;
    ov_state = 1;
    __enable_irq();
    /* Start camera XLK signal to capture the image data */
    OV7670_XLK_Enable();
    /* Start DCMI capturing */
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, ov_buf_addr, (OV7670_WIDTH / 2U));
}

void OV7670_Stop(void)
{
    __disable_irq();
    HAL_DCMI_Stop(&hdcmi);
    ov_state = 0;
    __enable_irq();
    OV7670_XLK_Disable();
}

uint8_t OV7670_isDriverBusy(void)
{
    uint8_t retVal;
    __disable_irq();
    retVal = (ov_state == 1) ? 1 : 0;
    __enable_irq();
    return retVal;
}

void HAL_DCMI_LineEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    uint8_t vsync_detected = 0;
    uint8_t state = 1;
    uint32_t buf_addr = 0x0U;
    uint32_t lineCnt;

    if (!(hdcmi->Instance->SR & DCMI_SR_VSYNC))
    {
        vsync_detected = 1;
    }

    if (vsync_detected)
    {
        __disable_irq();
        lineCnt = ov_line_cnt;
        __enable_irq();

        /* If this line is the last line of the frame */
        if (lineCnt == OV7670_HEIGHT - 1U)
        {
            HAL_DCMI_Stop(hdcmi);
            lineCnt = 0U;
        }
        else
        {
            /* Increment line counter */
            lineCnt++;
        }

        if (1U)  /* 每行都显示 */
        {
            /* 直接写到 LCD */
            ILI9341_SetRegion(0U, lineCnt, OV7670_WIDTH - 1U, lineCnt);
            ILI9341_WritePixels((const uint16_t *)ov_buf_addr, OV7670_WIDTH);

            /* If driver is still working */
            if (state == 1)
            {
                /* Update buffer address with the next half-part */
                buf_addr = (ov_buf_addr != (uint32_t)buffer) ? (uint32_t)buffer : (ov_buf_addr + (OV7670_WIDTH * 2U));
                /* Capture next line from the snapshot/stream */
                HAL_DCMI_Start_DMA(hdcmi, DCMI_MODE_CONTINUOUS, buf_addr, (OV7670_WIDTH / 2U));
            }
        }

        __disable_irq();
        ov_line_cnt = lineCnt;
        ov_state = state;
        ov_buf_addr = (buf_addr) ? buf_addr : ov_buf_addr;
        __enable_irq();
    }
}


/******************************************************************************
 *                              LOCAL FUNCTIONS                               *
 ******************************************************************************/
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

static uint8_t OV7670_SCCB_Read(uint8_t regAddr, uint8_t *data)
{
    uint8_t ret = HAL_ERROR;
    for (int i = 0; i < 3; i++)
    {
        ret = HAL_I2C_Master_Transmit(&hi2c2, OV7670_SCCB_ADDR, &regAddr, 1U, 100U);
        if (ret == HAL_OK)
            ret = HAL_I2C_Master_Receive(&hi2c2, OV7670_SCCB_ADDR, data, 1U, 100U);
        if (ret == HAL_OK) break;
    }
    return ret;
}

void OV7670_Delay(uint32_t time)
{
    HAL_Delay(time);
}

static void OV7670_w_pwdn(uint8_t value)
{
    HAL_GPIO_WritePin(OV7670_PWDN_GPIO_Port, OV7670_PWDN_Pin,
                      value ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void OV7670_w_rst(void)
{
    HAL_GPIO_WritePin(OV7670_RST_GPIO_Port, OV7670_RST_Pin, GPIO_PIN_RESET);
    OV7670_Delay(50);
    HAL_GPIO_WritePin(OV7670_RST_GPIO_Port, OV7670_RST_Pin, GPIO_PIN_SET);
    OV7670_Delay(50);
}

static void OV7670_XLK_Enable(void)  
{ 
    TIM5->CCER |= TIM_CCER_CC3E; 
    TIM5->CR1 |= TIM_CR1_CEN; 
}

static void OV7670_XLK_Disable(void)   
{ 
    TIM5->CCER &= ~TIM_CCER_CC3E; 
    TIM5->CR1 &= ~TIM_CR1_CEN; 
}