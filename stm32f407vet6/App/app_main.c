#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "bootCount.h"
#include "ili9341_display.h"
#include "OV7670.h"
#include "OV7670_task.h"
#include "rgb565_utils.h"

void app_main()
{
    printf("\r\n\r\n\r\n\r\n\r\n\r\n\r\n");
    log_info("sys", "OV7670 Demo");
    log_info("sys", "Boot Count: %d", BOOTCOUNT_Get());


    // 硬件初始化
    ILI9341_Init();
    OV7670_Init();

    // 软件初始化
    OV7670_Task_Init();   // 创建队列 → 启动摄像头 → 创建任务

    // 启动调度器
    vTaskStartScheduler();
}