#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "bootCount.h"
#include "ili9341_display.h"
#include "OV7670.h"
#include "dcmi.h"
#include "i2c.h"
#include "tim.h"

TaskHandle_t loopTaskHandle = NULL;

void loop(void *p);

/* ==================================================================== */
/*  OV7670 行回调：每采集完一行数据，立即写入 LCD                         */
/* ==================================================================== */
static void OV7670_DrawLine(const uint8_t *buffer, uint32_t buf_size,
                            uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2)
{
    ILI9341_SetRegion(x1, y1, x2, y2);
    ILI9341_WritePixels((const uint16_t *)buffer, buf_size / 2);
}

/* ==================================================================== */
/*  OV7670 寄存器读取（SCCB 协议，参考 OV7670_R_REG）                    */
/*  SCCB 读 = 发地址(带STOP) + 重新START读数据，不能用 Mem_Read          */
/* ==================================================================== */
static uint8_t OV7670_ReadReg(uint8_t reg)
{
    uint8_t val = 0;
    /* 第一步：发寄存器地址（带 STOP） */
    if (HAL_I2C_Master_Transmit(&hi2c2, 0x42, &reg, 1, 100) != HAL_OK)
    {
        return 0xFF;
    }
    /* 第二步：重新 START + 读数据 */
    if (HAL_I2C_Master_Receive(&hi2c2, 0x42, &val, 1, 100) != HAL_OK)
    {
        return 0xFF;
    }
    return val;
}

void app_main()
{
    log_info("sys", "OV7670 Demo");
    log_info("sys", "Boot Count: %d", BOOTCOUNT_Get());

    /* ---- LCD 初始化 ---- */
    ILI9341_Init();
    ILI9341_Clear(ILI9341_COLOR_BLUE);
    printf("LCD init finished\r\n");
    HAL_Delay(1000);

    /* ---- OV7670 初始化 ---- */
    OV7670_Init(&hdcmi, &hi2c2, &htim5, TIM_CHANNEL_3);

    /* OV7670_Init 结束后 XCLK 被关闭了，读寄存器前要重新开启 */
    HAL_TIM_OC_Start(&htim5, TIM_CHANNEL_3);

    /* ---- 读几个寄存器验证配置 ---- */
    printf("====== OV7670 Register Verify ======\r\n");
    uint8_t pid   = OV7670_ReadReg(0x0A);
    uint8_t ver   = OV7670_ReadReg(0x0B);
    uint8_t com7  = OV7670_ReadReg(0x12);
    uint8_t com15 = OV7670_ReadReg(0x40);
    uint8_t clkrc = OV7670_ReadReg(0x11);

    printf("  PID(0x0A)    = 0x%02X  (expect 0x76)\r\n", pid);
    printf("  VER(0x0B)    = 0x%02X  (expect 0x73)\r\n", ver);
    printf("  COM7(0x12)   = 0x%02X  (expect 0x14 = QVGA+RGB)\r\n", com7);
    printf("  COM15(0x40)  = 0x%02X  (expect 0xD0 = RGB565)\r\n", com15);
    printf("  CLKRC(0x11)  = 0x%02X  (expect 0x00)\r\n", clkrc);

    if (pid == 0x76 && ver == 0x73)
    {
        printf("  [OK] OV7670 detected and configured successfully!\r\n");
    }
    else
    {
        printf("  [FAIL] OV7670 not responding! Check wiring/I2C/XCLK.\r\n");
    }
    printf("====================================\r\n");

    /* 验证完毕，XCLK 停掉等 Start 再开 */
    HAL_TIM_OC_Stop(&htim5, TIM_CHANNEL_3);

    /* ---- 注册行回调 ---- */
    OV7670_RegisterCallback(OV7670_DRAWLINE_CBK, (OV7670_FncPtr_t)OV7670_DrawLine);

    /* ---- 启动摄像头连续采集 ---- */
    OV7670_Start();

    xTaskCreate(loop, "loop", 256 * 1, NULL, 1, &loopTaskHandle);
    vTaskStartScheduler();
}

void loop(void *p)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}