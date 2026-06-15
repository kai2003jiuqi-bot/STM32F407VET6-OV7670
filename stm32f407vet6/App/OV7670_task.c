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

#define RGB565_R(v)  (((v) >> 11) & 0x1F)   
#define RGB565_G(v)  (((v) >> 5)  & 0x3F)
#define RGB565_B(v)  ((v)         & 0x1F)
#define RGB565_PACK(r, g, b)  ((uint16_t)(((r) << 11) | ((g) << 5) | (b)))
#define OV7670_TASK_STACK_SIZE  (256 * 20)

xQueueHandle OV7670QueueHandle = NULL;

static uint8_t  jpeg_buf[1024*2]; // 存储压缩后的JPEG图片
static size_t   jpeg_len; // 计算当前压缩图片的长度           
static uint8_t rgb888_buf[OV7670_WIDTH * OV7670_HEIGHT * 3] 
                __attribute__((section(".ccmram")));

static void stb_write_cb(void *context, void *data, int size);
static void rgb565_to_rgb888(const uint16_t *src, uint8_t *dst, int pixels);
static uint16_t RGB565_Avg2(uint16_t p0, uint16_t p1);
static uint16_t RGB565_Avg4(uint16_t p0, uint16_t p1, uint16_t p2, uint16_t p3);
static void vofa_send_jpg(int img_id, size_t jpg_len);

/* ==================================================================== */
/*                          Public API                                  */
/* ==================================================================== */

/**
 * @brief   初始化 OV7670 任务
 */
void OV7670_Task_Init(void)
{
    // OV7670硬件初始化
    OV7670_Init();

    // 启动 OV7670 图像采集
    OV7670_Start();

    // 创建 OV7670 队列
    OV7670QueueHandle = xQueueCreate(1, sizeof(uint8_t));
    if (!OV7670QueueHandle) {
        log_info("OV7670", "Queue create FAILED");
        return;
    }
    
    // 创建 OV7670 任务
    xTaskCreate(OV7670_Task, "OV7670", OV7670_TASK_STACK_SIZE,
                NULL, 1, NULL);
}

/**
 * @brief   OV7670 任务
 */
void OV7670_Task(void *p)
{
    while(1)
    {
        // 等待OV7670触发帧中断
        uint8_t dummy;
        xQueueReceive(OV7670QueueHandle, &dummy, portMAX_DELAY);

        // 将一帧RGB565图像数据转换为RGB888
        rgb565_to_rgb888((const uint16_t *)capture_data, rgb888_buf,
                         OV7670_WIDTH * OV7670_HEIGHT);

        // 将RGB888图像数据压缩为JPEG格式
        jpeg_len = 0;
        if (stbi_write_jpg_to_func(stb_write_cb, NULL,
                                   OV7670_WIDTH, OV7670_HEIGHT, 3,
                                   rgb888_buf, 20))
        {
            // 压缩成功，发送JPEG数据到Vofa+
            vofa_send_jpg(1, jpeg_len);
        }

        // LCD显示图片
        uint16_t row_buf[320];               // 640B: 输出行缓冲
        uint16_t row0[OV7670_WIDTH];         // 320B: 当前输入行
        uint16_t row1[OV7670_WIDTH];         // 320B: 下一输入行

        ILI9341_SetRegion(0, 0, 319, 239);

        for (int sy = 0; sy < OV7670_HEIGHT; sy++) // sy:source y 行索引
        {
            const uint16_t *src = (const uint16_t *)capture_data;
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

/* ==================================================================== */
/*                          Private API                                  */
/* ==================================================================== */

/** 
 * @brief 将 JPEG 输出的一包数据追加到 jpeg_buf 
 */
static void stb_write_cb(void *context, void *data, int size)
{
    (void)context;
    if (jpeg_len + (size_t)size > sizeof(jpeg_buf)) return;
    memcpy(jpeg_buf + jpeg_len, data, (size_t)size);
    jpeg_len += (size_t)size;
}

/** 
 * @brief RGB565 → RGB888 逐像素转换 
 */
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

/**
 * @brief  对2个RGB565像素按通道取平均（用于水平/垂直插值）
 * @param  p0, p1  输入像素
 * @return 平均后的像素
 */
static uint16_t RGB565_Avg2(uint16_t p0, uint16_t p1)
{
    uint16_t r = (RGB565_R(p0) + RGB565_R(p1)) >> 1;
    uint16_t g = (RGB565_G(p0) + RGB565_G(p1)) >> 1;
    uint16_t b = (RGB565_B(p0) + RGB565_B(p1)) >> 1;
    return RGB565_PACK(r, g, b);
}


/**
 * @brief  对4个RGB565像素按通道取平均（用于双线性插值）
 * @param  p0, p1, p2, p3  输入像素（通常为左上、右上、左下、右下）
 * @return 平均后的像素
 */
static uint16_t RGB565_Avg4(uint16_t p0, uint16_t p1, uint16_t p2, uint16_t p3)
{
    uint16_t r = ((uint16_t)RGB565_R(p0) + RGB565_R(p1)
                + RGB565_R(p2) + RGB565_R(p3)) >> 2;
    uint16_t g = ((uint16_t)RGB565_G(p0) + RGB565_G(p1)
                + RGB565_G(p2) + RGB565_G(p3)) >> 2;
    uint16_t b = ((uint16_t)RGB565_B(p0) + RGB565_B(p1)
                + RGB565_B(p2) + RGB565_B(p3)) >> 2;
    return RGB565_PACK(r, g, b);
}

/** 
 * @brief 通过 UART 以 Vofa+ FireWater 协议发送 JPEG 
 */
static void vofa_send_jpg(int img_id, size_t jpg_len)
{
    char header[64];
    int hlen = snprintf(header, sizeof(header),
                        "image:%d,%d,-1,-1,%d\n",
                        img_id, (int)jpg_len, Format_JPG);

    /* 合并 header + JPEG 到 jpeg_buf 头部，一次发送 */
    memmove(jpeg_buf + hlen, jpeg_buf, jpg_len);
    memcpy(jpeg_buf, header, hlen);
    HAL_UART_Transmit_IT(&huart1, jpeg_buf, hlen + jpg_len);
}
