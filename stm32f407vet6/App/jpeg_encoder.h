/**
 * @file    jpeg_encoder.h
 * @brief   轻量 JPEG 编码器 (Baseline, 4:4:4, RGB565 输入)
 *
 * @details 专为 STM32F407 设计的软件 JPEG 编码器。
 *          使用 FPU 进行浮点 DCT，标准 Huffman 表。
 *          输出通过回调交付，无需一次性分配大缓冲区。
 *
 *          典型输出大小 (160x120):
 *            quality=50 → ~5-8 KB
 *            quality=80 → ~8-15 KB
 *            quality=30 → ~3-5 KB
 *
 * @note    只支持 RGB565 输入、YCbCr 4:4:4 采样。
 *          宽度和高度需为 8 的倍数，否则边缘像素会被裁剪。
 */
#ifndef JPEG_ENCODER_H
#define JPEG_ENCODER_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief JPEG 输出回调
 * @param  context  用户上下文指针
 * @param  data     输出的数据块
 * @param  len      数据块长度 (字节)
 * @return 实际处理的字节数，应等于 len；0 表示失败
 */
typedef size_t (*jpeg_write_cb)(void *context, const void *data, size_t len);

/**
 * @brief  将 RGB565 图像编码为 JPEG
 *
 * @param  image    RGB565 像素数组 (w × h 个像素，每像素 2 字节)
 * @param  width    图像宽度 (像素，需为 8 的倍数)
 * @param  height   图像高度 (像素，需为 8 的倍数)
 * @param  quality  JPEG 质量 (1~100, 越大质量越好文件越大)
 * @param  cb       输出回调函数 (每编码出一块数据就调用一次)
 * @param  context  传给回调的用户上下文
 *
 * @return 0 成功，-1 失败
 *
 * @note   编码过程中的临时内存 (Huffman 位流缓冲 ~1KB)
 *         在栈上分配，无需堆内存。
 */
int jpeg_encode_rgb565(const uint16_t *image, int width, int height,
                       int quality, jpeg_write_cb cb, void *context);

#endif /* JPEG_ENCODER_H */
