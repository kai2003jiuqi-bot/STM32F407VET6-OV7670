/**
 * @file    jpeg_encoder.c
 * @brief   轻量 JPEG 编码器实现 (Baseline, YCbCr 4:4:4, RGB565 输入)
 *
 * @details 参考 ITU T.81 (JPEG) 规范实现 Baseline 编码。
 *          使用浮点 DCT (FPU)，标准量化表和 Huffman 表。
 *          输出通过回调交付，不在堆上分配内存。
 *
 *          编码步骤：
 *          1. 写入 SOI、APP0、DQT、SOF0、DHT、SOS 标记
 *          2. 逐 8x8 MCU 处理:
 *             a. RGB565 → YCbCr 色彩空间转换
 *             b. 电平移位 (减 128)
 *             c. 2D FDCT (浮点)
 *             d. 量化 + Zigzag
 *             e. DPCM DC + Run-Length AC → Huffman 编码
 *          3. 刷新位流 → 写入 EOI
 *
 * @note    本实现固定使用 4:4:4 采样（无下采样），
 *          宽度和高度需为 8 的倍数。
 */
#include "jpeg_encoder.h"
#include <math.h>
#include <string.h>

/*===========================================================================*/
/*                        常量与标准表                                        */
/*===========================================================================*/

/** 最小可编码单元大小 (4:4:4 下为 8x8) */
#define MCU_SIZE    8

/* Zigzag 顺序: 将 8x8 矩阵映射到 64 元素一维序列 */
static const uint8_t s_zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/** 标准亮度量化表 (对应 quality=50) */
static const uint8_t s_std_luminance_qtable[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

/** 标准色度量化表 (对应 quality=50) */
static const uint8_t s_std_chrominance_qtable[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

/*===========================================================================*/
/*             标准 Huffman 表 (BITS + HUFFVAL, 来自 ITU T.81 Annex K.3)     */
/*===========================================================================*/

/* 每个 BITS 数组有 16 个元素，表示 1~16 位各有多少个码字 */

/* --- DC 亮度 --- */
static const uint8_t s_dc_y_bits[16]  = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t s_dc_y_val[12]   = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

/* --- DC 色度 --- */
static const uint8_t s_dc_c_bits[16]  = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const uint8_t s_dc_c_val[12]   = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

/* --- AC 亮度 --- */
static const uint8_t s_ac_y_bits[16]  = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D };
static const uint8_t s_ac_y_val[162]  = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
    0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA
};

/* --- AC 色度 --- */
static const uint8_t s_ac_c_bits[16]  = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const uint8_t s_ac_c_val[162]  = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0,
    0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26,
    0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
    0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
    0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xF9, 0xFA
};

/*===========================================================================*/
/*              Huffman 表运行时构建 (按 symbol 值索引)                       */
/*===========================================================================*/

/** Huffman 码字 */
typedef struct {
    uint16_t code;    /**< 编码值 (左对齐, 高位对齐) */
    uint8_t  len;     /**< 码字长度 (bit)            */
} huff_code_t;

/** 最大可能的 symbol 值 (AC 表用到 0xF0+) */
#define HUFF_LOOKUP_SIZE  256

/**
 * @brief 从 BITS + HUFFVAL 构建 symbol→code 查找表
 *
 * @param bits    16 元素的 BITS 表
 * @param val     按顺序排列的 HUFFVAL 数组
 * @param nval    HUFFVAL 元素数
 * @param lookup  输出查找表 (必须 HUFF_LOOKUP_SIZE 大小)
 */
static void huff_build_lookup(const uint8_t bits[16], const uint8_t *val,
                              int nval, huff_code_t *lookup)
{
    /* 初始化为无效 (len=0 表示未定义) */
    for (int i = 0; i < HUFF_LOOKUP_SIZE; i++)
        lookup[i].len = 0;

    int code = 0;
    int idx = 0;
    for (int bitslen = 1; bitslen <= 16; bitslen++) {
        for (int j = 0; j < bits[bitslen - 1]; j++) {
            uint8_t sym = val[idx];
            lookup[sym].code = (uint16_t)(code << (16 - bitslen));
            lookup[sym].len  = (uint8_t)bitslen;
            idx++;
            code++;
        }
        code <<= 1;
    }
}

/*===========================================================================*/
/*              量化表生成 (按 quality 缩放)                                  */
/*===========================================================================*/

/**
 * @brief 根据 quality 缩放标准量化表
 * @param dst   输出量化表 (64 元素)
 * @param src   标准量化表 (64 元素)
 * @param q     quality 1~100
 */
static void make_qtable(uint16_t *dst, const uint8_t *src, int q)
{
    int scale;
    if (q < 50)
        scale = 5000 / q;
    else
        scale = 200 - q * 2;

    if (scale < 1)  scale = 1;
    if (scale > 999) scale = 999;

    for (int i = 0; i < 64; i++) {
        int v = (src[i] * scale + 50) / 100;
        if (v < 1)   v = 1;
        if (v > 255) v = 255;
        dst[i] = (uint16_t)v;
    }
}

/*===========================================================================*/
/*              位流写入器 (带字节填充)                                       */
/*===========================================================================*/

/** 位流缓冲区大小 (大部分 JPEG 编码由此输出) */
#define BITSTREAM_SIZE  2048

typedef struct {
    uint8_t  buf[BITSTREAM_SIZE]; /**< 输出缓冲区 */
    size_t   len;                 /**< 已写入字节数 */
    int      bit_pos;             /**< 当前字节内有效位数 (0~7) */
    uint8_t  byte_buf;            /**< 当前正在累积的字节 */
    jpeg_write_cb cb;             /**< 输出回调 */
    void    *ctx;                 /**< 回调上下文 */
} bitstream_t;

/** 刷新缓冲区到回调 */
static int bs_flush(bitstream_t *bs)
{
    if (bs->len > 0) {
        size_t ret = bs->cb(bs->ctx, bs->buf, bs->len);
        if (ret != bs->len) return -1;
        bs->len = 0;
    }
    return 0;
}

/** 写入一个原始字节 (不带位填充) */
static int bs_write_byte(bitstream_t *bs, uint8_t b)
{
    if (bs->len >= BITSTREAM_SIZE) {
        if (bs_flush(bs)) return -1;
    }
    bs->buf[bs->len++] = b;
    return 0;
}

/** 写入 2 字节大端 */
static int bs_write_be16(bitstream_t *bs, uint16_t v)
{
    if (bs_write_byte(bs, (uint8_t)(v >> 8)))  return -1;
    if (bs_write_byte(bs, (uint8_t)(v & 0xFF))) return -1;
    return 0;
}

/** 写入 JPEG 标记 (0xFF + 类型) */
static int bs_write_marker(bitstream_t *bs, uint8_t type)
{
    if (bs_write_byte(bs, 0xFF)) return -1;
    return bs_write_byte(bs, type);
}

/**
 * @brief 写入 bits 比特 (MSB 优先)，带字节填充
 *
 * JPEG 规定压缩数据中的 0xFF 必须后跟 0x00 字节填充。
 * 每写完一个完整的字节就检查是否需要填充。
 */
static void bs_write_bits(bitstream_t *bs, uint32_t bits, int nbits)
{
    while (nbits > 0) {
        int take = 8 - bs->bit_pos;
        if (take > nbits) take = nbits;

        bs->byte_buf <<= take;
        bs->byte_buf |= (uint8_t)((bits >> (nbits - take)) & ((1u << take) - 1));
        bs->bit_pos += take;
        nbits -= take;

        if (bs->bit_pos >= 8) {
            /* 字节填充: 0xFF 后跟 0x00 */
            if (bs->len >= BITSTREAM_SIZE) {
                /* 如果满了，先刷出去 */
            }
            bs->buf[bs->len++] = bs->byte_buf;
            if (bs->byte_buf == 0xFF) {
                bs->buf[bs->len++] = 0x00;
            }
            if (bs->len >= BITSTREAM_SIZE - 4) {
                bs_flush(bs);
            }
            bs->bit_pos = 0;
            bs->byte_buf = 0;
        }
    }
}

/** 刷新剩余比特 (补 0 到字节边界) */
static int bs_flush_bits(bitstream_t *bs)
{
    if (bs->bit_pos > 0) {
        bs_write_bits(bs, 0, 8 - bs->bit_pos);
    }
    return bs_flush(bs);
}

/*===========================================================================*/
/*              2D FDCT (浮点, 行列分离)                                      */
/*===========================================================================*/

/** DCT 常量: 1/(2*sqrt(2)) 的 float */
static float fdct_c0(void) { return 0.35355339f; }  /* 1/(2*sqrt(2)) */
static float fdct_c(int u) { return 0.5f; }

/**
 * @brief 8 点 1D DCT (type-II)
 * @param v  输入/输出数组 (8 float)
 */
static void fdct_1d(float v[8])
{
    float tmp[8];
    for (int u = 0; u < 8; u++) {
        float sum = 0.0f;
        for (int x = 0; x < 8; x++) {
            sum += v[x] * cosf((float)((2 * x + 1) * u) * 3.14159265358979f / 16.0f);
        }
        tmp[u] = sum * ((u == 0) ? fdct_c0() : fdct_c(u));
    }
    memcpy(v, tmp, sizeof(tmp));
}

/**
 * @brief 2D FDCT: 对 8x8 块先按行再按列做 1D DCT
 * @param block  输入数据 (减 128 后的值), 输出 DCT 系数
 */
static void fdct_2d(float block[64])
{
    float tmp[8];

    /* 按行 */
    for (int y = 0; y < 8; y++) {
        memcpy(tmp, &block[y * 8], sizeof(tmp));
        fdct_1d(tmp);
        memcpy(&block[y * 8], tmp, sizeof(tmp));
    }

    /* 按列 */
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++)
            tmp[y] = block[y * 8 + x];
        fdct_1d(tmp);
        for (int y = 0; y < 8; y++)
            block[y * 8 + x] = tmp[y];
    }
}

/*===========================================================================*/
/*              RGB565 → YCbCr 色彩空间转换                                   */
/*===========================================================================*/

/**
 * @brief 读取 8x8 RGB565 块并转换为 YCbCr
 *
 * RGB565 到 YCbCr (601 标准, 整数近似):
 *   Y  =  0.299R + 0.587G + 0.114B
 *   Cb = -0.169R - 0.331G + 0.500B + 128
 *   Cr =  0.500R - 0.419G - 0.081B + 128
 *
 * 使用整数运算: Y  = (77*R + 150*G + 29*B) >> 8
 *              Cb = ((-43*R - 85*G + 128*B) >> 8) + 128
 *              Cr = ((128*R - 107*G - 21*B) >> 8) + 128
 *
 * 然后再减 128 做电平移位 (DC 归零)
 */
static void rgb565_to_ycbcr_block(const uint16_t *src, int stride,
                                  float yblock[64],
                                  float cbblock[64],
                                  float crblock[64])
{
    for (int i = 0; i < 64; i++) {
        int sy = (i / 8) * stride + (i % 8);
        uint16_t p = src[sy];

        int r5 = (p >> 11) & 0x1F;
        int g6 = (p >> 5)  & 0x3F;
        int b5 =  p        & 0x1F;

        /* 5/6 bit → 8 bit (左对齐) */
        int r8 = (r5 << 3) | (r5 >> 2);
        int g8 = (g6 << 2) | (g6 >> 4);
        int b8 = (b5 << 3) | (b5 >> 2);

        /* Y  =  0.299R + 0.587G + 0.114B, 范围 0~255 */
        int y = (77 * r8 + 150 * g8 + 29 * b8) >> 8;
        /* Cb = -0.169R - 0.331G + 0.500B + 128, 范围 0~255 */
        int cb = ((-43 * r8 - 85 * g8 + 128 * b8) >> 8) + 128;
        /* Cr =  0.500R - 0.419G - 0.081B + 128, 范围 0~255 */
        int cr = ((128 * r8 - 107 * g8 - 21 * b8) >> 8) + 128;

        /* 电平移位 (减 128), 使 DC 系数中心在 0 */
        yblock[i]  = (float)(y  - 128);
        cbblock[i] = (float)(cb - 128);
        crblock[i] = (float)(cr - 128);
    }
}

/*===========================================================================*/
/*              Huffman 编码核心                                              */
/*===========================================================================*/

/** DC 编码辅助：返回值的类别 (位宽) */
static int dc_category(int diff)
{
    if (diff < 0) diff = -diff;
    if (diff == 0) return 0;
    int cat = 0;
    while (diff) { diff >>= 1; cat++; }
    return cat;
}

/** AC 编码辅助：返回非零系数的类别 (位宽) */
static int ac_category(int val)
{
    if (val < 0) val = -val;
    int cat = 0;
    while (val) { val >>= 1; cat++; }
    return cat;
}

/**
 * @brief 编码一个块的 DC 系数
 * @param bs    位流
 * @param dc    当前 DC 系数
 * @param pred  前一块的 DC 系数 (更新)
 * @param ht    DC Huffman 表
 */
static void encode_dc(bitstream_t *bs, int dc, int *pred,
                      const huff_code_t *ht)
{
    int diff = dc - *pred;
    *pred = dc;

    int cat = dc_category(diff);
    /* 写 Huffman 码 (Category) */
    bs_write_bits(bs, ht[cat].code, ht[cat].len);

    /* 写附加比特 */
    if (cat > 0) {
        if (diff > 0) {
            bs_write_bits(bs, (uint32_t)diff, cat);
        } else {
            bs_write_bits(bs, (uint32_t)(diff + (1 << cat) - 1), cat);
        }
    }
}

/**
 * @brief 编码一个块的 AC 系数 (Zigzag 后)
 * @param bs    位流
 * @param zig   64 个 AC 系数 (索引 1~63)
 * @param ht    AC Huffman 表
 */
static void encode_ac(bitstream_t *bs, const int16_t zig[64],
                      const huff_code_t *ht)
{
    int run = 0;
    for (int k = 1; k < 64; k++) {
        int16_t v = zig[k];
        if (v == 0) {
            run++;
        } else {
            while (run >= 16) {
                /* ZRL: 16 个零 */
                bs_write_bits(bs, ht[0xF0].code, ht[0xF0].len);
                run -= 16;
            }
            int cat = ac_category(v);
            int sym = (run << 4) | cat;
            bs_write_bits(bs, ht[sym].code, ht[sym].len);

            if (v > 0) {
                bs_write_bits(bs, (uint32_t)v, cat);
            } else {
                bs_write_bits(bs, (uint32_t)(v + (1 << cat) - 1), cat);
            }
            run = 0;
        }
    }
    /* 如果块尾还有零: EOB */
    if (run > 0 || zig[63] == 0) {
        bs_write_bits(bs, ht[0x00].code, ht[0x00].len);
    }
}

/*===========================================================================*/
/*              JPEG 标记写入                                                */
/*===========================================================================*/

/** 写入 DQT (Define Quantization Table) */
static int write_dqt(bitstream_t *bs, const uint16_t qt[2][64])
{
    /* 标记 + 长度 (2 + 65*2) */
    int len = 2 + 2 * 65;
    if (bs_write_marker(bs, 0xDB)) return -1;   /* DQT */
    if (bs_write_be16(bs, (uint16_t)len)) return -1;

    /* 表 0: 亮度, precision=0 (8bit) */
    if (bs_write_byte(bs, 0x00)) return -1;
    for (int i = 0; i < 64; i++)
        if (bs_write_byte(bs, (uint8_t)qt[0][s_zigzag[i]])) return -1;

    /* 表 1: 色度 */
    if (bs_write_byte(bs, 0x01)) return -1;
    for (int i = 0; i < 64; i++)
        if (bs_write_byte(bs, (uint8_t)qt[1][s_zigzag[i]])) return -1;

    return 0;
}

/** 写入 SOF0 (Start of Frame, Baseline) */
static int write_sof0(bitstream_t *bs, int w, int h)
{
    /* 标记 + 长度(17) + 精度(1) + 高(2) + 宽(2) + 组件数(1) + 每组件3字节 */
    if (bs_write_marker(bs, 0xC0)) return -1;  /* SOF0 */
    if (bs_write_be16(bs, 17)) return -1;      /* 段长度 */
    if (bs_write_byte(bs, 8)) return -1;       /* 精度 8bit */
    if (bs_write_be16(bs, (uint16_t)h)) return -1;
    if (bs_write_be16(bs, (uint16_t)w)) return -1;
    if (bs_write_byte(bs, 3)) return -1;       /* 3 个组件: Y, Cb, Cr */

    /* Y: ID=1, 采样因子 1x1 (4:4:4), QT 表 0 */
    if (bs_write_byte(bs, 1)) return -1;
    if (bs_write_byte(bs, 0x11)) return -1;    /* 水平1x垂直1 */
    if (bs_write_byte(bs, 0)) return -1;

    /* Cb: ID=2 */
    if (bs_write_byte(bs, 2)) return -1;
    if (bs_write_byte(bs, 0x11)) return -1;
    if (bs_write_byte(bs, 1)) return -1;

    /* Cr: ID=3 */
    if (bs_write_byte(bs, 3)) return -1;
    if (bs_write_byte(bs, 0x11)) return -1;
    if (bs_write_byte(bs, 1)) return -1;

    return 0;
}

/** 写入 DHT (Define Huffman Table) — 4 个表 */
static int write_dht_one(bitstream_t *bs, const uint8_t bits[16],
                         const uint8_t *val, int nval, int table_id)
{
    int len = 2 + 16 + nval;
    if (bs_write_byte(bs, 0xFF)) return -1;
    if (bs_write_byte(bs, 0xC4)) return -1;    /* DHT 标记 */
    if (bs_write_be16(bs, (uint16_t)len)) return -1;

    /* 表 ID + 类型 (0=DC, 16=AC) */
    if (bs_write_byte(bs, (uint8_t)(table_id & 0x1F))) return -1;
    for (int i = 0; i < 16; i++)
        if (bs_write_byte(bs, bits[i])) return -1;
    for (int i = 0; i < nval; i++)
        if (bs_write_byte(bs, val[i])) return -1;

    return 0;
}

/**
 * @note 我们把 4 个表合成一个 DHT 标记段。
 *       顺序: DC-Y, AC-Y, DC-C, AC-C
 */
static int write_dht(bitstream_t *bs)
{
    /* DHT 标记共用, 但为了简化我们分 4 次写独立的 DHT */
    if (write_dht_one(bs, s_dc_y_bits, s_dc_y_val, 12, 0x00)) return -1;
    if (write_dht_one(bs, s_ac_y_bits, s_ac_y_val, 162, 0x10)) return -1;
    if (write_dht_one(bs, s_dc_c_bits, s_dc_c_val, 12, 0x01)) return -1;
    if (write_dht_one(bs, s_ac_c_bits, s_ac_c_val, 162, 0x11)) return -1;
    return 0;
}

/** 写入 SOS (Start of Scan) */
static int write_sos(bitstream_t *bs)
{
    if (bs_write_marker(bs, 0xDA)) return -1;  /* SOS */
    if (bs_write_be16(bs, 12)) return -1;      /* 段长度 */

    if (bs_write_byte(bs, 3)) return -1;       /* 3 个组件 */

    /* Y: ID=1, DC表0, AC表0 */
    if (bs_write_byte(bs, 1)) return -1;
    if (bs_write_byte(bs, 0x00)) return -1;

    /* Cb: ID=2, DC表1, AC表1 */
    if (bs_write_byte(bs, 2)) return -1;
    if (bs_write_byte(bs, 0x11)) return -1;

    /* Cr: ID=3, DC表1, AC表1 */
    if (bs_write_byte(bs, 3)) return -1;
    if (bs_write_byte(bs, 0x11)) return -1;

    /* 其它 SOS 参数 */
    if (bs_write_byte(bs, 0)) return -1;       /* 谱选择开始 */
    if (bs_write_byte(bs, 63)) return -1;      /* 谱选择结束 */
    if (bs_write_byte(bs, 0)) return -1;       /* 逐位逼近 */

    return 0;
}

/*===========================================================================*/
/*              主编码函数                                                    */
/*===========================================================================*/

int jpeg_encode_rgb565(const uint16_t *image, int width, int height,
                       int quality, jpeg_write_cb cb, void *context)
{
    if (!image || !cb || width <= 0 || height <= 0 || quality < 1 || quality > 100)
        return -1;

    if (width % 8 != 0 || height % 8 != 0)
        return -1;  /* 当前实现需要 8 的倍数 */

    /*------------------------------------------------------------------------*/
    /*  初始化位流                                                             */
    /*------------------------------------------------------------------------*/
    bitstream_t bs;
    bs.len     = 0;
    bs.bit_pos = 0;
    bs.byte_buf = 0;
    bs.cb = cb;
    bs.ctx = context;

    /*------------------------------------------------------------------------*/
    /*  构建量化表                                                             */
    /*------------------------------------------------------------------------*/
    uint16_t qtable[2][64];
    make_qtable(qtable[0], s_std_luminance_qtable, quality);
    make_qtable(qtable[1], s_std_chrominance_qtable, quality);

    /*------------------------------------------------------------------------*/
    /*  构建 Huffman 查找表 (symbol→code)                                      */
    /*------------------------------------------------------------------------*/
    huff_code_t huff_dc_y[HUFF_LOOKUP_SIZE];
    huff_code_t huff_dc_c[HUFF_LOOKUP_SIZE];
    huff_code_t huff_ac_y[HUFF_LOOKUP_SIZE];
    huff_code_t huff_ac_c[HUFF_LOOKUP_SIZE];

    huff_build_lookup(s_dc_y_bits, s_dc_y_val, 12, huff_dc_y);
    huff_build_lookup(s_dc_c_bits, s_dc_c_val, 12, huff_dc_c);
    huff_build_lookup(s_ac_y_bits, s_ac_y_val, 162, huff_ac_y);
    huff_build_lookup(s_ac_c_bits, s_ac_c_val, 162, huff_ac_c);

    /*------------------------------------------------------------------------*/
    /*  写入 JPEG 标记                                                         */
    /*------------------------------------------------------------------------*/
    /* SOI */
    if (bs_write_marker(&bs, 0xD8)) goto fail;

    /* APP0 (JFIF) */
    if (bs_write_marker(&bs, 0xE0)) goto fail;
    if (bs_write_be16(&bs, 16)) goto fail;              /* 段长度 */
    if (bs_write_byte(&bs, 'J')) goto fail;
    if (bs_write_byte(&bs, 'F')) goto fail;
    if (bs_write_byte(&bs, 'I')) goto fail;
    if (bs_write_byte(&bs, 'F')) goto fail;
    if (bs_write_byte(&bs, 1)) goto fail;                /* 版本号高 = 1 */
    if (bs_write_byte(&bs, 1)) goto fail;                /* 版本号低 = 1 (JFIF 1.01) */
    if (bs_write_byte(&bs, 0)) goto fail;                /* 密度单位 */
    if (bs_write_be16(&bs, 1)) goto fail;                /* X 密度 */
    if (bs_write_be16(&bs, 1)) goto fail;                /* Y 密度 */
    if (bs_write_byte(&bs, 0)) goto fail;                /* 缩略图宽 */
    if (bs_write_byte(&bs, 0)) goto fail;                /* 缩略图高 */

    /* DQT */
    if (write_dqt(&bs, qtable)) goto fail;

    /* SOF0 */
    if (write_sof0(&bs, width, height)) goto fail;

    /* DHT */
    if (write_dht(&bs)) goto fail;

    /* SOS */
    if (write_sos(&bs)) goto fail;

    /*------------------------------------------------------------------------*/
    /*  主循环: 逐 8x8 MCU 编码                                               */
    /*------------------------------------------------------------------------*/
    float block[64];
    int16_t zig[64];
    int pred_dc_y = 0, pred_dc_cb = 0, pred_dc_cr = 0;

    int blocks_y = height / 8;
    int blocks_x = width  / 8;

    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {
            /* 当前 MCU 的左上角坐标 */
            int ox = bx * 8;
            int oy = by * 8;

            float cb_block[64], cr_block[64];

            /* 读取 8x8 并转为 YCbCr */
            rgb565_to_ycbcr_block(image + oy * width + ox, width,
                                  block, cb_block, cr_block);

            /* ---------- 编码 Y 块 ---------- */
            fdct_2d(block);
            /* 量化 + Zigzag */
            for (int i = 0; i < 64; i++) {
                int v = (int)roundf(block[s_zigzag[i]] / (float)qtable[0][s_zigzag[i]]);
                if (v < -1023) v = -1023;
                if (v > 1023)  v = 1023;
                zig[i] = (int16_t)v;
            }
            encode_dc(&bs, zig[0], &pred_dc_y, huff_dc_y);
            encode_ac(&bs, zig, huff_ac_y);

            /* ---------- 编码 Cb 块 ---------- */
            fdct_2d(cb_block);
            for (int i = 0; i < 64; i++) {
                int v = (int)roundf(cb_block[s_zigzag[i]] / (float)qtable[1][s_zigzag[i]]);
                if (v < -1023) v = -1023;
                if (v > 1023)  v = 1023;
                zig[i] = (int16_t)v;
            }
            encode_dc(&bs, zig[0], &pred_dc_cb, huff_dc_c);
            encode_ac(&bs, zig, huff_ac_c);

            /* ---------- 编码 Cr 块 ---------- */
            fdct_2d(cr_block);
            for (int i = 0; i < 64; i++) {
                int v = (int)roundf(cr_block[s_zigzag[i]] / (float)qtable[1][s_zigzag[i]]);
                if (v < -1023) v = -1023;
                if (v > 1023)  v = 1023;
                zig[i] = (int16_t)v;
            }
            encode_dc(&bs, zig[0], &pred_dc_cr, huff_dc_c);
            encode_ac(&bs, zig, huff_ac_c);
        }
    }

    /* 刷新位流 */
    if (bs_flush_bits(&bs)) goto fail;

    /* EOI */
    if (bs_write_marker(&bs, 0xD9)) goto fail;
    if (bs_flush(&bs)) goto fail;

    return 0;

fail:
    bs_flush(&bs);
    return -1;
}
