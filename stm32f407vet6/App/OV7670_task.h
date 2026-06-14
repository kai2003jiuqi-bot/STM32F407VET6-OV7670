/**
 * @file    OV7670_task.h
 * @brief   OV7670 图像采集 + JPEG 编码 + VOFA+ 发送任务
 */
#ifndef OV7670_TASK_H
#define OV7670_TASK_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#define OV7670_TASK_STACK_SIZE  (256 * 20)

/** 帧完成信号队列 (用于 ISR → 任务通信) */
extern xQueueHandle OV7670QueueHandle;

/** 初始化: 创建队列 → 启动摄像头 → 创建任务 */
void OV7670_Task_Init(void);

/** 任务入口: 等待帧信号 → JPEG 编码 → VOFA+ 发送 */
void OV7670_Task(void *p);

#endif
