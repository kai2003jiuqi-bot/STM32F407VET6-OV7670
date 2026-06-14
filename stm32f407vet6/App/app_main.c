#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "bootCount.h"
#include "ili9341_display.h"
#include "OV7670.h"

TaskHandle_t loopTaskHandle = NULL;
void loop(void *p);

void app_main()
{
    log_info("sys", "OV7670 Demo");
    log_info("sys", "Boot Count: %d", BOOTCOUNT_Get());

    /* ---- LCD 初始化 ---- */
    ILI9341_Init();

    OV7670_Init();
    OV7670_Start();

    xTaskCreate(loop, "loop", 256 * 1, NULL, 1, &loopTaskHandle);
    vTaskStartScheduler();
}

void loop(void *p)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
