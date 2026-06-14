/**
 * @file    OV7670_task.c
 * @brief   OV7670 图像采集 → stb JPEG 编码 → VOFA+ FireWater 发送
 */
#include "OV7670_task.h"
#include "OV7670.h"
#include "stb_image_write.h"
#include "vofa.h"
#include "log.h"
#include <string.h>

/* UART 句柄 (用于 VOFA+ 发送) */
extern UART_HandleTypeDef huart1;

/* ------ 队列句柄 ------ */
xQueueHandle OV7670QueueHandle = NULL;

/* ------ JPEG 输出缓冲 ------ */
static uint8_t  s_jpeg_buf[16384];
static size_t   s_jpeg_len;

/* ------ RGB888 转换缓冲 (放 CCMRAM, 不占主 RAM) ------ */
static uint8_t s_rgb888[OV7670_WIDTH * OV7670_HEIGHT * 3]
    __attribute__((section(".ccmram")));

/** stb 写回调: 将 JPEG 输出追加到 s_jpeg_buf */
static void stb_write_cb(void *context, void *data, int size)
{
    (void)context;
    if (s_jpeg_len + (size_t)size > sizeof(s_jpeg_buf)) return;
    memcpy(s_jpeg_buf + s_jpeg_len, data, (size_t)size);
    s_jpeg_len += (size_t)size;
}

/** RGB565 → RGB888 逐像素转换 */
static void rgb565_to_rgb888(const uint16_t *src, uint8_t *dst, int pixels)
{
    for (int i = 0; i < pixels; i++) {
        uint16_t p = src[i];
        int r5 = (p >> 11) & 0x1F;
        int g6 = (p >> 5)  & 0x3F;
        int b5 =  p        & 0x1F;
        *dst++ = (uint8_t)((r5 << 3) | (r5 >> 2));
        *dst++ = (uint8_t)((g6 << 2) | (g6 >> 4));
        *dst++ = (uint8_t)((b5 << 3) | (b5 >> 2));
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

/* ==================================================================== */
/*                          Public API                                   */
/* ==================================================================== */

void OV7670_Task_Init(void)
{
    OV7670QueueHandle = xQueueCreate(1, sizeof(uint8_t));
    if (!OV7670QueueHandle) {
        log_info("OV7670", "Queue create FAILED");
        return;
    }

    OV7670_Start();

    xTaskCreate(OV7670_Task, "OV7670", OV7670_TASK_STACK_SIZE,
                NULL, 1, NULL);
}

void OV7670_Task(void *p)
{
    uint8_t dummy;
    while(1)
    {
        xQueueReceive(OV7670QueueHandle, &dummy, portMAX_DELAY);

        /* RGB565 → RGB888 → stb JPEG 编码 → Vofa+ */
        rgb565_to_rgb888((const uint16_t *)capture_data, s_rgb888,
                         OV7670_WIDTH * OV7670_HEIGHT);

        s_jpeg_len = 0;
        if (stbi_write_jpg_to_func(stb_write_cb, NULL,
                                   OV7670_WIDTH, OV7670_HEIGHT, 3,
                                   s_rgb888, 20))
        {
            vofa_send_jpg(1, s_jpeg_len);
        }
    }
}
