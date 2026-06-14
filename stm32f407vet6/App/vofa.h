/**
 * @file    vofa.h
 * @brief   Vofa+ FireWater 协议图像格式定义
 *
 * @details Vofa+ (Visual Open Framework for Assistant) 是一款 PC 端
 *          数据可视化工具, FireWater 是其高速串行数据传输协议。
 *
 *          图像传输协议格式:
 *          @code
 *          "image:<通道ID>,<数据长度>,<宽度>,<高度>,<格式编码>\\n"
 *          <原始二进制数据>
 *          @endcode
 *
 *          对于自包含格式 (JPEG/PNG/BMP等), 宽高可设为 -1,
 *          尺寸信息从图像文件头中解析。
 *
 *          本工程使用 UART 以 Format_JPG (27) 格式发送图像。
 *
 * @version  1.0
 * @date     2026-06-02
 *
 * @note     所有 Format_* 常量定义来自 Vofa+ FireWater 协议规范
 *
 * @see      https://www.vofa.plus/
 *
 * @copyright Copyright (c) 2026
 ******************************************************************************/
#ifndef __VOFA_H__
#define __VOFA_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Vofa+ FireWater 图像格式枚举
 *
 * @note   Format_Invalid (0) 到 Format_Grayscale8 (24) 为像素格式,
 *         需要指定宽度和高度。
 *         Format_BMP (25) 及以上为自包含格式, 宽高设为 -1。
 */
enum ImgFormat {
    /* ========== 像素格式 (需指定宽高) ========== */
    Format_Invalid,                     /**< 0  - 无效格式                      */
    Format_Mono,                        /**< 1  - 单色 (1-bit)                 */
    Format_MonoLSB,                     /**< 2  - 单色 LSB优先                 */
    Format_Indexed8,                    /**< 3  - 8-bit 索引色                 */
    Format_RGB32,                       /**< 4  - 32-bit RGB                   */
    Format_ARGB32,                      /**< 5  - 32-bit ARGB                  */
    Format_ARGB32_Premultiplied,        /**< 6  - 32-bit ARGB (预乘 alpha)     */
    Format_RGB16,                       /**< 7  - 16-bit RGB                   */
    Format_ARGB8565_Premultiplied,      /**< 8  - ARGB 8565 (预乘 alpha)       */
    Format_RGB666,                      /**< 9  - 18-bit RGB                   */
    Format_ARGB6666_Premultiplied,      /**< 10 - ARGB 6666 (预乘 alpha)       */
    Format_RGB555,                      /**< 11 - 16-bit RGB555                */
    Format_ARGB8555_Premultiplied,      /**< 12 - ARGB 8555 (预乘 alpha)       */
    Format_RGB888,                      /**< 13 - 24-bit RGB                   */
    Format_RGB444,                      /**< 14 - 12-bit RGB444                */
    Format_ARGB4444_Premultiplied,      /**< 15 - ARGB 4444 (预乘 alpha)       */
    Format_RGBX8888,                    /**< 16 - 32-bit RGBX                  */
    Format_RGBA8888,                    /**< 17 - 32-bit RGBA                  */
    Format_RGBA8888_Premultiplied,      /**< 18 - 32-bit RGBA (预乘 alpha)     */
    Format_BGR30,                       /**< 19 - 10:10:10 BGR (高位填充)     */
    Format_A2BGR30_Premultiplied,       /**< 20 - A2BGR30 (预乘 alpha)         */
    Format_RGB30,                       /**< 21 - 10:10:10 RGB (高位填充)     */
    Format_A2RGB30_Premultiplied,       /**< 22 - A2RGB30 (预乘 alpha)         */
    Format_Alpha8,                      /**< 23 - 8-bit Alpha 通道             */
    Format_Grayscale8,                  /**< 24 - 8-bit 灰度图                 */

    /* ========== 自包含格式 (宽高设为 -1) ========== */
    Format_BMP,     /**< 25 — BMP 位图 (Windows Bitmap)                       */
    Format_GIF,     /**< 26 — GIF 动图 (Graphics Interchange Format)          */
    Format_JPG,     /**< 27 — JPEG 图像 (Joint Photographic Experts Group)    */
    Format_PNG,     /**< 28 — PNG 图像 (Portable Network Graphics)            */
    Format_PBM,     /**< 29 — PBM 位图 (Portable BitMap)                      */
    Format_PGM,     /**< 30 — PGM 灰度图 (Portable GrayMap)                   */
    Format_PPM,     /**< 31 — PPM 彩色图 (Portable PixMap)                    */
    Format_XBM,     /**< 32 — XBM 位图 (X BitMap)                             */
    Format_XPM,     /**< 33 — XPM 像素图 (X PixMap)                           */
    Format_SVG,     /**< 34 — SVG 矢量图 (Scalable Vector Graphics)           */
};

#ifdef __cplusplus
}
#endif

#endif /* __VOFA_H__ */
