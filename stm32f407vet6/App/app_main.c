#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "log.h"
#include "bootCount.h"
#include "ili9341_display.h"
#include "OV7670.h"
#include "rgb565_utils.h"
#include "stb_image_write.h"
#include "vofa.h"
#include <string.h>

TaskHandle_t loopTaskHandle = NULL;
TaskHandle_t OV7670TaskHandle = NULL;

xQueueHandle OV7670QueueHandle = NULL;

/* UART 句柄 (用于 VOFA+ 发送) */
extern UART_HandleTypeDef huart1;

/* ------ JPEG 输出缓冲 ------ */
static uint8_t  s_jpeg_buf[16384];
static size_t   s_jpeg_len;

/** stb 写回调: 将 JPEG 输出追加到 s_jpeg_buf */
static void stb_write_cb(void *context, void *data, int size)
{
    (void)context;
    if (s_jpeg_len + (size_t)size > sizeof(s_jpeg_buf)) return;
    memcpy(s_jpeg_buf + s_jpeg_len, data, (size_t)size);
    s_jpeg_len += (size_t)size;
}

/* ------ RGB888 转换缓冲 (放 CCMRAM, 不占主 RAM) ------ */
static uint8_t s_rgb888[OV7670_WIDTH * OV7670_HEIGHT * 3]
    __attribute__((section(".ccmram")));

/** RGB565 → RGB888 逐像素转换 */
static void rgb565_to_rgb888(const uint16_t *src, uint8_t *dst, int pixels)
{
    for (int i = 0; i < pixels; i++) {
        uint16_t p = src[i];
        int r5 = (p >> 11) & 0x1F;
        int g6 = (p >> 5)  & 0x3F;
        int b5 =  p        & 0x1F;
        *dst++ = (uint8_t)((r5 << 3) | (r5 >> 2));  /* R 8-bit */
        *dst++ = (uint8_t)((g6 << 2) | (g6 >> 4));  /* G 8-bit */
        *dst++ = (uint8_t)((b5 << 3) | (b5 >> 2));  /* B 8-bit */
    }
}

/** 通过 UART 以 Vofa+ FireWater 协议发送 JPEG */
static void vofa_send_jpg(int img_id, size_t jpg_len)
{
    char header[64];
    int hlen = snprintf(header, sizeof(header),
                        "image:%d,%d,-1,-1,%d\n",
                        img_id, (int)jpg_len, Format_JPG);
    HAL_UART_Transmit(&huart1, (uint8_t *)header, hlen, 1000);
    HAL_UART_Transmit(&huart1, s_jpeg_buf, jpg_len, 5000);
}

void loop(void *p);
void OV7670_task(void *p);

void app_main()
{
    printf("\r\n\r\n\r\n\r\n\r\n\r\n\r\n");
    log_info("sys", "OV7670 Demo");
    log_info("sys", "Boot Count: %d", BOOTCOUNT_Get());

    ILI9341_Init();
    OV7670_Init();

    OV7670QueueHandle = xQueueCreate(1, sizeof(uint8_t));
    OV7670_Start();

    xTaskCreate(OV7670_task, "OV7670", 256 * 20, NULL, 1, &OV7670TaskHandle);
    vTaskStartScheduler();
}

/**
 * @brief 栈溢出钩子
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("\n\n*** STACK OVERFLOW *** Task: %s\n\n", pcTaskName);
    log_info("FATAL", "STACK OVERFLOW: %s", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for (;;);
}

void OV7670_task(void *p)
{
    uint8_t dummy;
    while(1)
    {
        xQueueReceive(OV7670QueueHandle, &dummy, portMAX_DELAY);

        /* ======== LCD 显示 (160x120 → 320x240 双线性放大) ======== */
        // const uint16_t *src = (const uint16_t *)capture_data;
        // uint16_t row_buf[320];
        // uint16_t row0[OV7670_WIDTH];
        // uint16_t row1[OV7670_WIDTH];

        // ILI9341_SetRegion(0, 0, 319, 239);
        // for (int sy = 0; sy < OV7670_HEIGHT; sy++)
        // {
        //     memcpy(row0, &src[sy * OV7670_WIDTH], OV7670_WIDTH * sizeof(uint16_t));
        //     for (int dx = 0; dx < 320; dx++)
        //     {
        //         int sx = dx >> 1;
        //         uint16_t p = row0[sx];
        //         if (dx & 1)
        //             p = (sx + 1 < OV7670_WIDTH) ? RGB565_Avg2(p, row0[sx + 1]) : p;
        //         row_buf[dx] = p;
        //     }
        //     ILI9341_WritePixels(row_buf, 320);

        //     if (sy + 1 < OV7670_HEIGHT)
        //     {
        //         memcpy(row1, &src[(sy + 1) * OV7670_WIDTH],
        //                OV7670_WIDTH * sizeof(uint16_t));
        //         for (int dx = 0; dx < 320; dx++)
        //         {
        //             int sx = dx >> 1;
        //             if (dx & 1)
        //             {
        //                 uint16_t tr = (sx + 1 < OV7670_WIDTH) ? row0[sx + 1] : row0[sx];
        //                 uint16_t br = (sx + 1 < OV7670_WIDTH) ? row1[sx + 1] : row1[sx];
        //                 row_buf[dx] = RGB565_Avg4(row0[sx], tr, row1[sx], br);
        //             }
        //             else
        //             {
        //                 row_buf[dx] = RGB565_Avg2(row0[sx], row1[sx]);
        //             }
        //         }
        //     }
        //     ILI9341_WritePixels(row_buf, 320);
        // }

        /* ======== RGB565 → RGB888 → stb JPEG 编码 → Vofa+ ======== */
        rgb565_to_rgb888((const uint16_t *)capture_data, s_rgb888,
                         OV7670_WIDTH * OV7670_HEIGHT);

        s_jpeg_len = 0;
        if (stbi_write_jpg_to_func(stb_write_cb, NULL,
                                   OV7670_WIDTH, OV7670_HEIGHT, 3,
                                   s_rgb888, 50))
        {
            vofa_send_jpg(1, s_jpeg_len);
        }
    }
}
