#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "log.h"
#include "bootCount.h"
#include "ili9341_display.h"
#include "OV7670.h"
#include "rgb565_utils.h"

TaskHandle_t loopTaskHandle = NULL;
TaskHandle_t OV7670TaskHandle = NULL;

xQueueHandle OV7670QueueHandle = NULL;


void loop(void *p);
void OV7670_task(void *p);

void app_main()
{
    printf("\r\n\r\n\r\n\r\n\r\n\r\n\r\n");
    log_info("sys", "OV7670 Demo");
    log_info("sys", "Boot Count: %d", BOOTCOUNT_Get());

    // 初始化
    ILI9341_Init();
    OV7670_Init();

    // 必须先创建队列再 Start，否则 ISR 可能在队列创建前触发
    OV7670QueueHandle = xQueueCreate(1, sizeof(uint8_t));

    OV7670_Start();

    xTaskCreate(loop, "loop", 256 * 1, NULL, 1, &loopTaskHandle);
    xTaskCreate(OV7670_task, "OV7670", 256 * 20, NULL, 1, &OV7670TaskHandle);
    vTaskStartScheduler();
}

void loop(void *p)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief 栈溢出钩子——触发时打印任务名并死循环
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
