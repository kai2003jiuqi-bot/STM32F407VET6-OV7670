/*
 * OV7670.c
 * OV7670 DCMI DMA Driver
 * Created on: Aug 5, 2024
 *     Author: K.Rudenko
 */

#include "OV7670.h"
#include "ili9341_display.h"
#include "dcmi.h"
#include "i2c.h"
#include "tim.h"

const uint8_t OV7670_reg[][2] =
{
  /* Color mode and resolution settings */
  {OV7670_REG_COM7,             0x04},         // VGA+RGB, 靠DCW缩到QQVGA
//{OV7670_REG_COM7,             0xCU},         // QCIF (176*144), RGB
  {OV7670_REG_RGB444,           0x00},         // RGB444 Disable
  {OV7670_REG_COM15,            0xD0},         // RGB565
  {OV7670_REG_TSLB,             0xCU},         // UYVY
  {OV7670_REG_COM13,            0x80},         // gamma enable, UV auto adjust, UYVY
  {OV7670_REG_RSRVD,            0x84},         // Important!
  /* Clock settings */
  {OV7670_REG_COM3,             0x04},         // DCW enable
  {OV7670_REG_COM14,            0x1A},         // divide by 4, scaling on
  {OV7670_REG_SCALING_XSC,      0x3A},         // scaling_xsc
  {OV7670_REG_SCALING_YSC,      0x35},         // scaling_ysc
  {OV7670_REG_SCALING_DCWCTR,   0x22},         // downsample by 4
  {OV7670_REG_SCALING_PCLK_DIV, 0xF2},         // divide by 4
  /* Windowing */
  {OV7670_REG_HSTART,           0x16},         // HSTART
  {OV7670_REG_HSTOP,            0x04},         // HSTOP
  {OV7670_REG_HREF,             0xA4},         // HREF (QQVGA)
  {OV7670_REG_VSTRT,            0x02},         // VSTART (QQVGA)
  {OV7670_REG_VSTOP,            0x7A},         // VSTOP (QQVGA)
  {OV7670_REG_VREF,             0x0a},         // VREF (VSTART_LOW = 2, VSTOP_LOW = 2)
  /* Color matrix coefficient */
#if 0
#else
  {OV7670_REG_MTX1,             0x80},
  {OV7670_REG_MTX2,             0x80},
  {OV7670_REG_MTX3,             0x00},
  {OV7670_REG_MTX4,             0x22},
  {OV7670_REG_MTX5,             0x5E},
  {OV7670_REG_MTX6,             0x80},
  {OV7670_REG_MTXS,             0x9E},
#endif
  {OV7670_REG_COM16,            0x38},         // edge enhancement, de-noise, AWG gain enabled
  /* gamma curve */
#if 1
  {OV7670_REG_GAM1,             16},
  {OV7670_REG_GAM2,             30},
  {OV7670_REG_GAM3,             53},
  {OV7670_REG_GAM4,             90},
  {OV7670_REG_GAM5,             105},
  {OV7670_REG_GAM6,             118},
  {OV7670_REG_GAM7,             130},
  {OV7670_REG_GAM8,             140},
  {OV7670_REG_GAM9,             150},
  {OV7670_REG_GAM10,            160},
  {OV7670_REG_GAM11,            180},
  {OV7670_REG_GAM12,            195},
  {OV7670_REG_GAM13,            215},
  {OV7670_REG_GAM14,            230},
  {OV7670_REG_GAM15,            244},
  {OV7670_REG_SLOP,             16},
#else
#endif
  /* FPS */
//{OV7670_REG_DBLV,             0x4a},         // PLL  x4
  {OV7670_REG_CLKRC,            0x00},         // Pre-scalar = XCLK/5 (QR项目验证值)
  /* Others */
  {OV7670_REG_MVFP,             0x31},         // Mirror flip
//{OV7670_REG_COM17,            0x08},         // Test screen with color bars
  {OV7670_REG_DUMMY,            OV7670_REG_DUMMY},
};

/* Peripheral handles (直接使用全局) */
/* Driver state */
static volatile uint32_t   ov_buf_addr;
static volatile uint32_t   ov_line_cnt;
static volatile uint8_t    ov_state;
/* Image buffer */
static uint8_t buffer[OV7670_WIDTH * OV7670_HEIGHT * 2];
static HAL_StatusTypeDef SCCB_Write(uint8_t regAddr, uint8_t data);
static HAL_StatusTypeDef SCCB_Read(uint8_t regAddr, uint8_t *data);
static uint8_t isFrameCaptured(void);
static void OV7670_DELAY(uint32_t time);
static void XCLK_Start(void);
static void XCLK_Stop(void);
static void OV7670_w_pwdn(uint8_t value);
static void OV7670_w_rst(void);

void OV7670_Init(void)
{
    OV7670_w_pwdn(0);
    OV7670_w_rst();
    /* Start camera XLK signal to be able to do initialization */
    XCLK_Start();

    /* Do camera reset */
    SCCB_Write(OV7670_REG_COM7, 0x80);
    OV7670_DELAY(30);

    /* Get camera ID */
    uint8_t buf[4] = {0};
    HAL_StatusTypeDef ret = SCCB_Read(OV7670_REG_VER, buf);
    log_info("OV7670", "dev id = 0x%02X (ret=%d)", buf[0], ret);

    if (ret == HAL_OK)
    {
        /* Do camera reset */
        SCCB_Write(OV7670_REG_COM7, 0x80);
        OV7670_DELAY(30);

        /* Do camera configuration */
        for (uint32_t i = 0; OV7670_reg[i][0] != OV7670_REG_DUMMY; i++)
        {
            SCCB_Write(OV7670_reg[i][0], OV7670_reg[i][1]);
            OV7670_DELAY(1);
        }
    }
    else
    {
        log_info("OV7670", "NOT DETECTED, skipping config");
    }
    /* Stop camera XLK signal */
    XCLK_Stop();

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
    XCLK_Start();
    /* Start DCMI capturing */
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, ov_buf_addr, (OV7670_WIDTH / 2U));
}

void OV7670_Stop(void)
{
    while(!isFrameCaptured());
    __disable_irq();
    HAL_DCMI_Stop(&hdcmi);
    ov_state = 0;
    __enable_irq();
    XCLK_Stop();
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
            /* Disable DCMI Camera interface */
            HAL_DCMI_Stop(hdcmi);
            /* Stop camera XLK signal until captured image data is drawn */
            //XCLK_Stop();
            /* Reset line counter */
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

        /* Update line counter */
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

static HAL_StatusTypeDef SCCB_Write(uint8_t regAddr, uint8_t data)
{
    HAL_StatusTypeDef ret = HAL_ERROR;
    for (int i = 0; i < 3; i++)
    {
        ret = HAL_I2C_Mem_Write(&hi2c2, OV7670_SCCB_ADDR, regAddr,
                I2C_MEMADD_SIZE_8BIT, &data, 1U, 100U);
        if (ret == HAL_OK) break;
    }
    return ret;
}

static HAL_StatusTypeDef SCCB_Read(uint8_t regAddr, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_ERROR;
    for (int i = 0; i < 3; i++)
    {
        ret = HAL_I2C_Master_Transmit(&hi2c2, OV7670_SCCB_ADDR, &regAddr, 1U, 100U);
        if (ret == HAL_OK)
            ret = HAL_I2C_Master_Receive(&hi2c2, OV7670_SCCB_ADDR, data, 1U, 100U);
        if (ret == HAL_OK) break;
    }
    return ret;
}

static uint8_t isFrameCaptured(void)
{
    uint8_t retVal;
    __disable_irq();
    retVal = (ov_line_cnt == 0U) ? 1 : 0;
    __enable_irq();
    return retVal;
}

void OV7670_DELAY(uint32_t time)
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
    OV7670_DELAY(50);
    HAL_GPIO_WritePin(OV7670_RST_GPIO_Port, OV7670_RST_Pin, GPIO_PIN_SET);
    OV7670_DELAY(50);
}

static void XCLK_Start(void)  
{ 
    TIM5->CCER |= TIM_CCER_CC3E; 
    TIM5->CR1 |= TIM_CR1_CEN; 
}

static void XCLK_Stop(void)   
{ 
    TIM5->CCER &= ~TIM_CCER_CC3E; 
    TIM5->CR1 &= ~TIM_CR1_CEN; 
}

