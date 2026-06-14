#ifndef OV7670_H_
#define OV7670_H_

#include "stm32f4xx_hal.h"

#define OV7670_WIDTH                  160U
#define OV7670_HEIGHT                 120U
#define OV7670_SCCB_ADDR              0x42U

extern uint8_t capture_data[OV7670_WIDTH * OV7670_HEIGHT * 2];

void OV7670_Init(void);
void OV7670_Start(void);
void OV7670_Stop(void);

#endif 