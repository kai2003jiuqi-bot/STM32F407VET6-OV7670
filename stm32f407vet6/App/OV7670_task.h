/**
 * @file    OV7670_task.h
 * @brief   OV7670 图像采集 + JPEG 编码 + VOFA+ 发送任务
 */
#ifndef OV7670_TASK_H
#define OV7670_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

extern xQueueHandle OV7670QueueHandle;

void OV7670_Task_Init(void);
void OV7670_Task(void *p);

#endif
