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

xQueueHandle OV7670QueueHandle = NULL;

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

    /* 合并 header + JPEG 到 s_jpeg_buf 头部，一次发送 */
    memmove(s_jpeg_buf + hlen, s_jpeg_buf, jpg_len);
    memcpy(s_jpeg_buf, header, hlen);
    // HAL_UART_Transmit(&huart1, s_jpeg_buf, hlen + jpg_len, 0xFFFF);
    HAL_UART_Transmit_IT(&huart1, s_jpeg_buf, hlen + jpg_len);
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

        rgb565_to_rgb888((const uint16_t *)capture_data, s_rgb888,
                         OV7670_WIDTH * OV7670_HEIGHT);

        s_jpeg_len = 0;
        if (stbi_write_jpg_to_func(stb_write_cb, NULL,
                                   OV7670_WIDTH, OV7670_HEIGHT, 3,
                                   s_rgb888, 20))
        {
            vofa_send_jpg(1, s_jpeg_len);
        }

        const uint16_t *src = (const uint16_t *)capture_data;
        uint16_t row_buf[320];               // 640B: 输出行缓冲
        uint16_t row0[OV7670_WIDTH];         // 320B: 当前输入行
        uint16_t row1[OV7670_WIDTH];         // 320B: 下一输入行

        ILI9341_SetRegion(0, 0, 319, 239);

        for (int sy = 0; sy < OV7670_HEIGHT; sy++) // sy:source y 行索引
        {
            /* ---- 复制当前行（防止DMA写入导致数据不一致） ---- */
            memcpy(row0, &src[sy * OV7670_WIDTH], OV7670_WIDTH * sizeof(uint16_t));

            /* ================== 输出行 A (dy = sy*2) ================== */
            /* 偶数行：仅水平插值 */
            for (int dx = 0; dx < 320; dx++)    
            {
                int sx = dx >> 1;  // sx:source x 列索引
                uint16_t p = row0[sx];
                if (dx & 1)  /* 奇数列：与右侧像素水平平均 */
                {
                    p = (sx + 1 < OV7670_WIDTH)
                            ? RGB565_Avg2(p, row0[sx + 1])
                            : p;
                }
                row_buf[dx] = p;
            }
            ILI9341_WritePixels(row_buf, 320);

            /* ================== 输出行 B (dy = sy*2 + 1) ================== */
            /* 奇数行：水平 + 垂直双线性插值 */
            if (sy + 1 < OV7670_HEIGHT)
            {
                memcpy(row1, &src[(sy + 1) * OV7670_WIDTH],
                    OV7670_WIDTH * sizeof(uint16_t));

                for (int dx = 0; dx < 320; dx++)
                {
                    int sx = dx >> 1;
                    if (dx & 1)
                    {
                        /* 四角双线性平均 */
                        uint16_t tr = (sx + 1 < OV7670_WIDTH)
                                        ? row0[sx + 1] : row0[sx];
                        uint16_t br = (sx + 1 < OV7670_WIDTH)
                                        ? row1[sx + 1] : row1[sx];
                        row_buf[dx] = RGB565_Avg4(row0[sx], tr, row1[sx], br);
                    }
                    else
                    {
                        /* 垂直平均 */
                        row_buf[dx] = RGB565_Avg2(row0[sx], row1[sx]);
                    }
                }
            }
            /* 最后一行无下一行可插值，直接复用 row_buf（当前仍为行A数据） */
            ILI9341_WritePixels(row_buf, 320);
        }
    }
}
