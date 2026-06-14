/*
 * OV7670.c
 * OV7670 DCMI DMA Driver
 * Created on: Aug 5, 2024
 *     Author: K.Rudenko
 */

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include "OV7670.h"
#include "ili9341_display.h"
#include "dcmi.h"
#include "i2c.h"
#include "tim.h"

/******************************************************************************
 *                               LOCAL MACRO                                  *
 ******************************************************************************/
#define OV7670_RGB565_BYTES           (2U)
#define OV7670_LINES_IN_CHUNK         (1U)
#define OV7670_WIDTH_SIZE_BYTES       (OV7670_WIDTH * OV7670_RGB565_BYTES)
#define OV7670_WIDTH_SIZE_WORDS       (OV7670_WIDTH_SIZE_BYTES / 4U)
#define OV7670_HEIGHT_SIZE_BYTES      (OV7670_HEIGHT * OV7670_RGB565_BYTES)
#define OV7670_HEIGHT_SIZE_WORDS      (OV7670_HEIGHT_SIZE_BYTES / 4U)
#define OV7670_FRAME_SIZE_BYTES       (OV7670_WIDTH * OV7670_HEIGHT * OV7670_RGB565_BYTES)
#define OV7670_FRAME_SIZE_WORDS       (OV7670_FRAME_SIZE_BYTES / 4U)

#if (OV7670_STREAM_MODE == OV7670_STREAM_MODE_BY_LINE)
/* For double stream-line buffer */
#define OV7670_BUFFER_SIZE            (OV7670_WIDTH_SIZE_BYTES * OV7670_RGB565_BYTES * OV7670_LINES_IN_CHUNK)
#define OV7670_DMA_DATA_LEN           (OV7670_WIDTH_SIZE_WORDS * OV7670_LINES_IN_CHUNK)
/* Macro for update address to second half of double-line buffer */
#define OV7670_SWITCH_BUFFER()        ((ov_buf_addr != (uint32_t)buffer) ?\
        (ov_buf_addr + (OV7670_BUFFER_SIZE)/ 2U) : (uint32_t)buffer)
#define OV7670_RESET_BUFFER_ADDR()    (uint32_t)buffer

#define OV7670_START_XLK(htim, channel)\
        do{SET_BIT(htim->Instance->CCER, (0x1UL << channel));\
        SET_BIT(htim->Instance->CR1, TIM_CR1_CEN);}while(0)

#define OV7670_STOP_XLK(htim, channel)\
        do{CLEAR_BIT(htim->Instance->CCER, (0x1UL << channel));\
        CLEAR_BIT(htim->Instance->CR1, TIM_CR1_CEN);}while(0)

#else
/* For whole-size snapshot buffer */
#define OV7670_BUFFER_SIZE             (OV7670_FRAME_SIZE_BYTES)
#define OV7670_DMA_DATA_LEN            (OV7670_FRAME_SIZE_WORDS)
#endif

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
  {OV7670_REG_MTX1,             0xb3},
  {OV7670_REG_MTX2,             0xb3},
  {OV7670_REG_MTX3,             0x00},
  {OV7670_REG_MTX4,             0x3d},
  {OV7670_REG_MTX5,             0xa7},
  {OV7670_REG_MTX6,             0xe4},
  {OV7670_REG_MTXS,             0x9e},
#else
  {OV7670_REG_MTX1,             0x80},
  {OV7670_REG_MTX2,             0x80},
  {OV7670_REG_MTX3,             0x00},
  {OV7670_REG_MTX4,             0x22},
  {OV7670_REG_MTX5,             0x5E},
  {OV7670_REG_MTX6,             0x80},
  {OV7670_REG_MTXS,             0x9E},
#endif
//{OV7670_REG_COM8,             0x84},
//{OV7670_REG_COM9,             0x0a},         // AGC Ceiling = 2x
//{0x5FU,                       0x2f},         // AWB B Gain Range (empirically decided)
        // without this bright scene becomes yellow (purple). might be because of color matrix
//{0x60U,                       0x98},         // AWB R Gain Range (empirically decided)
//{0x61U,                       0x70},         // AWB G Gain Range (empirically decided)
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
  /* gamma = 1 */
  {OV7670_REG_GAM1,             4},
  {OV7670_REG_GAM2,             8},
  {OV7670_REG_GAM3,             16},
  {OV7670_REG_GAM4,             32},
  {OV7670_REG_GAM5,             40},
  {OV7670_REG_GAM6,             48},
  {OV7670_REG_GAM7,             56},
  {OV7670_REG_GAM8,             64},
  {OV7670_REG_GAM9,             72},
  {OV7670_REG_GAM10,            80},
  {OV7670_REG_GAM11,            96},
  {OV7670_REG_GAM12,            112},
  {OV7670_REG_GAM13,            144},
  {OV7670_REG_GAM14,            176},
  {OV7670_REG_GAM15,            208},
  {OV7670_REG_SLOP,             64},
#endif
  /* FPS */
//{OV7670_REG_DBLV,             0x4a},         // PLL  x4
  {OV7670_REG_CLKRC,            0x00},         // Pre-scalar = XCLK/5 (QR项目验证值)
  /* Others */
  {OV7670_REG_MVFP,             0x31},         // Mirror flip
//{OV7670_REG_COM17,            0x08},         // Test screen with color bars
  {OV7670_REG_DUMMY,            OV7670_REG_DUMMY},
};
/******************************************************************************
 *                           LOCAL DATA TYPES                                 *
 ******************************************************************************/


enum
{
    FALSE, TRUE
};

enum
{
    BUSY, READY
};

/******************************************************************************
 *                         LOCAL DATA PROTOTYPES                              *
 ******************************************************************************/

/* Peripheral handles (直接使用全局) */
/* Driver state */
static uint32_t            ov_mode;
static volatile uint32_t   ov_buf_addr;
static volatile uint32_t   ov_line_cnt;
static volatile uint8_t    ov_state;

/* Image buffer */
static uint8_t buffer[160*120*2];

/******************************************************************************
 *                       LOCAL FUNCTIONS PROTOTYPES                           *
 ******************************************************************************/

static HAL_StatusTypeDef SCCB_Write(uint8_t regAddr, uint8_t data);
static HAL_StatusTypeDef SCCB_Read(uint8_t regAddr, uint8_t *data);
static uint8_t isFrameCaptured(void);

/******************************************************************************
 *                              GLOBAL FUNCTIONS                              *
 ******************************************************************************/

void OV7670_Init(void)
{
    /* PWDN to LOW */
    HAL_GPIO_WritePin(OV7670_GPIO_PORT_PWDN, OV7670_GPIO_PIN_PWDN, GPIO_PIN_RESET);
    /* RET pin to LOW */
    HAL_GPIO_WritePin(OV7670_GPIO_PORT_RET, OV7670_GPIO_PIN_RET, GPIO_PIN_RESET);
    OV7670_DELAY(100);
    /* RET pin to HIGH */
    HAL_GPIO_WritePin(OV7670_GPIO_PORT_RET, OV7670_GPIO_PIN_RET, GPIO_PIN_SET);
    OV7670_DELAY(100);

    /* Start camera XLK signal to be able to do initialization */
    HAL_TIM_OC_Start(&htim5, TIM_CHANNEL_3);

    /* Do camera reset */
    SCCB_Write(OV7670_REG_COM7, 0x80);
    OV7670_DELAY(30);

    /* Get camera ID */
    uint8_t buf[4] = {0};
    HAL_StatusTypeDef ret = SCCB_Read(OV7670_REG_VER, buf);
    // DEBUG_LOG("[OV7670] dev id = 0x%02X (ret=%d)", buf[0], ret);
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
        // DEBUG_LOG("[OV7670] NOT DETECTED, skipping config");
        log_info("OV7670", "NOT DETECTED, skipping config");
    }
    /* Stop camera XLK signal */
    HAL_TIM_OC_Stop(&htim5, TIM_CHANNEL_3);

    /* Initialize buffer address */
    ov_buf_addr = (uint32_t) buffer;
}

void OV7670_Start(void)
{
    __disable_irq();
    /* Update requested mode */
    ov_mode = DCMI_MODE_CONTINUOUS;
#if (OV7670_STREAM_MODE == OV7670_STREAM_MODE_BY_LINE)
    /* Reset buffer address */
    ov_buf_addr = OV7670_RESET_BUFFER_ADDR();
#endif
    /* Reset line counter */
    ov_line_cnt = 0U;
    ov_state = BUSY;
    __enable_irq();
    /* Start camera XLK signal to capture the image data */
    HAL_TIM_OC_Start(&htim5, TIM_CHANNEL_3);
    /* Start DCMI capturing */
    HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_CONTINUOUS, ov_buf_addr, OV7670_DMA_DATA_LEN);
}

void OV7670_Stop(void)
{
    while(!isFrameCaptured());
    __disable_irq();
    HAL_DCMI_Stop(&hdcmi);
    ov_state = READY;
    __enable_irq();
    HAL_TIM_OC_Stop(&htim5, TIM_CHANNEL_3);
}

uint8_t OV7670_isDriverBusy(void)
{
    uint8_t retVal;
    __disable_irq();
    retVal = (ov_state == BUSY) ? TRUE : FALSE;
    __enable_irq();
    return retVal;
}


/******************************************************************************
 *                               HAL CALLBACKS                                *
 ******************************************************************************/

#if (OV7670_STREAM_MODE == OV7670_STREAM_MODE_BY_FRAME)

void HAL_DCMI_VsyncEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    /* Disable DCMI Camera interface */
    HAL_DCMI_Stop(hdcmi);

    /* Stop camera XLK signal until captured image data is drawn */
    HAL_TIM_OC_Stop(&htim5, TIM_CHANNEL_3);

    /* 直接写到 LCD */
    ILI9341_SetRegion(0, 0, OV7670_WIDTH - 1, OV7670_HEIGHT - 1);
    ILI9341_WritePixels((const uint16_t *)ov_buf_addr, OV7670_WIDTH * OV7670_HEIGHT);

    /* Reset line counter */
    ov_line_cnt = 0U;
    //TODO: check for full-size QVGA buffer mode
    HAL_DCMI_Start_DMA(hdcmi, DCMI_MODE_CONTINUOUS, ov_buf_addr,
            OV7670_FRAME_SIZE_WORDS);
}

void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
}

#else


void HAL_DCMI_LineEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    uint8_t vsync_detected = FALSE;
    uint8_t state = BUSY;
    uint32_t buf_addr = 0x0U;
    uint32_t lineCnt;

    if (!(hdcmi->Instance->SR & DCMI_SR_VSYNC))
    {
        vsync_detected = TRUE;
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
            //HAL_TIM_OC_Stop(&htim5, TIM_CHANNEL_3);
            /* Reset line counter */
            lineCnt = 0U;

            /* Update state if mode is SNAPSHOT */
            if (ov_mode == DCMI_MODE_SNAPSHOT)
            {
                state = READY;
                vsync_detected = FALSE;
            }
        }
        else
        {
            /* Increment line counter */
            lineCnt++;
        }

        if (((lineCnt + 1U) % OV7670_LINES_IN_CHUNK) == 0U)
        {
            /* 直接写到 LCD (MV=1: CASET=行, PASET=列, 交换x/y) */
            ILI9341_SetRegion(lineCnt, 0U, lineCnt, OV7670_WIDTH - 1U);
            ILI9341_WritePixels((const uint16_t *)ov_buf_addr, OV7670_WIDTH);

            /* If driver is still working */
            if (state == BUSY)
            {
                /* Update buffer address with the next half-part */
                buf_addr = OV7670_SWITCH_BUFFER();
                /* Capture next line from the snapshot/stream */
                HAL_DCMI_Start_DMA(hdcmi, DCMI_MODE_CONTINUOUS, buf_addr, OV7670_DMA_DATA_LEN);
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

#endif /* (OV7670_STREAM_MODE == OV7670_STREAM_MODE_BY_LINE) */


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
    retVal = (ov_line_cnt == 0U) ? TRUE : FALSE;
    __enable_irq();
    return retVal;
}

