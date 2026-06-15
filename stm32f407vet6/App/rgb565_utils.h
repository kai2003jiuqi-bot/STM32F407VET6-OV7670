/**
 * @file    rgb565_utils.h
 * @brief   RGB565 色彩格式的提取、打包和双线性插值辅助函数
 */
#ifndef RGB565_UTILS_H
#define RGB565_UTILS_H

#include <stdint.h>




uint16_t RGB565_Avg2(uint16_t p0, uint16_t p1);
uint16_t RGB565_Avg4(uint16_t p0, uint16_t p1, uint16_t p2, uint16_t p3);

#endif /* RGB565_UTILS_H */
