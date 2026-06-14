/**
 * @file    rgb565_utils.c
 * @brief   RGB565 双线性插值函数实现
 */
#include "rgb565_utils.h"

uint16_t RGB565_Avg2(uint16_t p0, uint16_t p1)
{
    uint16_t r = (RGB565_R(p0) + RGB565_R(p1)) >> 1;
    uint16_t g = (RGB565_G(p0) + RGB565_G(p1)) >> 1;
    uint16_t b = (RGB565_B(p0) + RGB565_B(p1)) >> 1;
    return RGB565_PACK(r, g, b);
}

uint16_t RGB565_Avg4(uint16_t p0, uint16_t p1, uint16_t p2, uint16_t p3)
{
    uint16_t r = ((uint16_t)RGB565_R(p0) + RGB565_R(p1)
                + RGB565_R(p2) + RGB565_R(p3)) >> 2;
    uint16_t g = ((uint16_t)RGB565_G(p0) + RGB565_G(p1)
                + RGB565_G(p2) + RGB565_G(p3)) >> 2;
    uint16_t b = ((uint16_t)RGB565_B(p0) + RGB565_B(p1)
                + RGB565_B(p2) + RGB565_B(p3)) >> 2;
    return RGB565_PACK(r, g, b);
}
