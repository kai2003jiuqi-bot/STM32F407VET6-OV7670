/*
 * OV7670.h
 * OV7670 DCMI DMA Driver
 * Created on: Aug 5, 2024
 *     Author: K.Rudenko
 */

#ifndef OV7670_H_
#define OV7670_H_

/******************************************************************************
 *                                 INCLUDES                                   *
 ******************************************************************************/

#include "main.h"
#include "gpio.h"
#include "log.h"

/******************************************************************************
 *                            CONFIGURATION MACRO                             *
 ******************************************************************************/

#define OV7670_WIDTH                             (160U)
#define OV7670_HEIGHT                            (120U)

#define OV7670_STREAM_MODE_BY_FRAME              0
#define OV7670_STREAM_MODE_BY_LINE               1
#define OV7670_STREAM_MODE                       OV7670_STREAM_MODE_BY_LINE

/* Delay API */
#define OV7670_DELAY(ms)                         HAL_Delay(ms)

/* GPIO pin mapping */
#define OV7670_GPIO_PORT_PWDN                    OV7670_PWDN_GPIO_Port
#define OV7670_GPIO_PIN_PWDN                     OV7670_PWDN_Pin
#define OV7670_GPIO_PORT_RET                     OV7670_RST_GPIO_Port
#define OV7670_GPIO_PIN_RET                      OV7670_RST_Pin

#define OV7670_SCCB_ADDR              (0x42U)

/******************************************************************************
 *                           OV7670 REGISTERS                                 *
 ******************************************************************************/
#define OV7670_REG_GAIN                (0x0U)   /* Def: 0x0; R/W: RW; AGC – Gain control gain setting */
#define OV7670_REG_BLUE                (0x1U)   /* Def: 0x80; R/W: RW; AWB – Blue channel gain setting */
#define OV7670_REG_RED                 (0x2U)   /* Def: 0x80; R/W: RW; AWB – Red channel gain setting */
#define OV7670_REG_VREF                (0x3U)   /* Def: 0x0; R/W: RW; Vertical Frame Control */
#define OV7670_REG_COM1                (0x4U)   /* Def: 0x0; R/W: RW; Common Control 1 */
#define OV7670_REG_BAVE                (0x5U)   /* Def: 0x0; R/W: RW; U/B Average Level */
#define OV7670_REG_GbAVE               (0x6U)   /* Def: 0x0; R/W: RW; Y/Gb Average Level */
#define OV7670_REG_AECHH               (0x7U)   /* Def: 0x0; R/W: RW; Exposure Value - AEC MSB 5 bits */
#define OV7670_REG_RAVE                (0x8U)   /* Def: 0x0; R/W: RW; V/R Average Level */
#define OV7670_REG_COM2                (0x9U)   /* Def: 0x1; R/W: RW; Common Control 2  */
#define OV7670_REG_PID                 (0x0AU)  /* Def: 0x76; R/W: R; Product ID Number MSB (Read only) */
#define OV7670_REG_VER                 (0x0BU)  /* Def: 0x73; R/W: R; Product ID Number LSB (Read only) */
#define OV7670_REG_COM3                (0x0CU)  /* Def: 0x0; R/W: RW; Common Control 3 */
#define OV7670_REG_COM4                (0x0DU)  /* Def: 0x0; R/W: RW; Common Control 4 */
#define OV7670_REG_COM5                (0x0EU)  /* Def: 0x1; R/W: RW; Common Control 5 */
#define OV7670_REG_COM6                (0x0FU)  /* Def: 0x43; R/W: RW; Common Control 6 */
#define OV7670_REG_AECH                (0x10U)  /* Def: 0x40; R/W: RW; Exposure Value */
#define OV7670_REG_CLKRC               (0x11U)  /* Def: 0x80; R/W: RW; Internal Clock */
#define OV7670_REG_COM7                (0x12U)  /* Def: 0x0; R/W: RW; Common Control 7 */
#define OV7670_REG_COM8                (0x13U)  /* Def: 0x8F; R/W: RW; Common Control 8 */
#define OV7670_REG_COM9                (0x14U)  /* Def: 0x4A; R/W: RW; Common Control 9 */
#define OV7670_REG_COM10               (0x15U)  /* Def: 0x0; R/W: RW; Common Control 10 */
#define OV7670_REG_HSTART              (0x17U)  /* Def: 0x11; R/W: RW; Output Format - Horizontal Frame (HREF column) start high 8-bit (low 3 bits are at HREF[2:0]) */
#define OV7670_REG_HSTOP               (0x18U)  /* Def: 0x61; R/W: RW; Output Format - Horizontal Frame (HREF column) end high 8-bit (low 3 bits are at HREF[5:3]) */
#define OV7670_REG_VSTRT               (0x19U)  /* Def: 0x3; R/W: RW; Output Format - Vertical Frame (row) start high 8-bit (low 2 bits are at VREF[1:0]) */
#define OV7670_REG_VSTOP               (0x1AU)  /* Def: 0x7B; R/W: RW; Output Format - Vertical Frame (row) end high 8-bit (low 2 bits are at VREF[3:2]) */
#define OV7670_REG_PSHFT               (0x1BU)  /* Def: 0x0; R/W: RW; Data Format - Pixel Delay Select (delays timing of the D[7:0] data relative to HREF in pixel units) */
#define OV7670_REG_MIDH                (0x1CU)  /* Def: 0x7F; R/W: R; Manufacturer ID Byte – High (Read only = 0x7F) */
#define OV7670_REG_MIDL                (0x1DU)  /* Def: 0xA2; R/W: R; Manufacturer ID Byte – Low (Read only = 0xA2) */
#define OV7670_REG_MVFP                (0x1EU)  /* Def: 0x1; R/W: RW; Mirror/VFlip Enable */
#define OV7670_REG_LAEC                (0x1FU)  /* Def: 0x0; R/W: RW; Reserved */
#define OV7670_REG_ADCCTR0             (0x20U)  /* Def: 0x4; R/W: RW; ADC Control */
#define OV7670_REG_ADCCTR1             (0x21U)  /* Def: 0x2; R/W: RW; Bit[7:0]: Reserved */
#define OV7670_REG_ADCCTR2             (0x22U)  /* Def: 0x1; R/W: RW; Bit[7:0]: Reserved */
#define OV7670_REG_ADCCTR3             (0x23U)  /* Def: 0x0; R/W: RW; Bit[7:0]: Reserved */
#define OV7670_REG_AEW                 (0x24U)  /* Def: 0x75; R/W: RW; AGC/AEC - Stable Operating Region (Upper Limit) */
#define OV7670_REG_AEB                 (0x25U)  /* Def: 0x63; R/W: RW; AGC/AEC - Stable Operating Region (Lower Limit) */
#define OV7670_REG_VPT                 (0x26U)  /* Def: 0xD4; R/W: RW; AGC/AEC Fast Mode Operating Region */
#define OV7670_REG_BBIAS               (0x27U)  /* Def: 0x80; R/W: RW; B Channel Signal Output Bias (effective only when COM6[3] = 1)  */
#define OV7670_REG_GbBIAS              (0x28U)  /* Def: 0x80; R/W: RW; Gb Channel Signal Output Bias (effective only when COM6[3] = 1) */
#define OV7670_REG_EXHCH               (0x2AU)  /* Def: 0x0; R/W: RW; Dummy Pixel Insert MSB */
#define OV7670_REG_EXHCL               (0x2BU)  /* Def: 0x0; R/W: RW; Dummy Pixel Insert LSB */
#define OV7670_REG_RBIAS               (0x2CU)  /* Def: 0x80; R/W: RW; R Channel Signal Output Bias (effective only when COM6[3] = 1) */
#define OV7670_REG_ADVFL               (0x2DU)  /* Def: 0x0; R/W: RW; LSB of Insert Dummy Lines in Vertical Direction (1 bit equals 1 line) */
#define OV7670_REG_ADVFH               (0x2EU)  /* Def: 0x0; R/W: RW; MSB of Insert Dummy Lines in Vertical Direction */
#define OV7670_REG_YAVE                (0x2FU)  /* Def: 0x0; R/W: RW; Y/G Channel Average Value */
#define OV7670_REG_HSYST               (0x30U)  /* Def: 0x8; R/W: RW; HSYNC Rising Edge Delay (low 8 bits) */
#define OV7670_REG_HSYEN               (0x31U)  /* Def: 0x30; R/W: RW; HSYNC Falling Edge Delay (low 8 bits) */
#define OV7670_REG_HREF                (0x32U)  /* Def: 0x80; R/W: RW; HREF Control */
#define OV7670_REG_CHLF                (0x33U)  /* Def: 0x8; R/W: RW; Array Current Control */
#define OV7670_REG_ARBLM               (0x34U)  /* Def: 0x11; R/W: RW; Array Reference Control */
#define OV7670_REG_ADC                 (0x37U)  /* Def: 0x3F; R/W: RW; ADC Control */
#define OV7670_REG_ACOM                (0x38U)  /* Def: 0x1; R/W: RW; ADC and Analog Common Mode Control */
#define OV7670_REG_OFON                (0x39U)  /* Def: 0x0; R/W: RW; ADC Offset Control */
#define OV7670_REG_TSLB                (0x3AU)  /* Def: 0x0D; R/W: RW; Line Buffer Test Option */
#define OV7670_REG_COM11               (0x3BU)  /* Def: 0x0; R/W: RW; Common Control 11 */
#define OV7670_REG_COM12               (0x3CU)  /* Def: 0x68; R/W: RW; Common Control 12 */
#define OV7670_REG_COM13               (0x3DU)  /* Def: 0x88; R/W: RW; Common Control 13 */
#define OV7670_REG_COM14               (0x3EU)  /* Def: 0x0; R/W: RW; Common Control 14 */
#define OV7670_REG_EDGE                (0x3FU)  /* Def: 0x0; R/W: RW; Edge Enhancement Adjustment */
#define OV7670_REG_COM15               (0x40U)  /* Def: 0xC0; R/W: RW; Common Control 15 */
#define OV7670_REG_COM16               (0x41U)  /* Def: 0x8; R/W: RW; Common Control 16 */
#define OV7670_REG_COM17               (0x42U)  /* Def: 0x0; R/W: RW; Common Control 17 */
#define OV7670_REG_AWBC1               (0x43U)  /* Def: 0x14; R/W: RW; Reserved */
#define OV7670_REG_AWBC2               (0x44U)  /* Def: 0xF0; R/W: RW; Reserved */
#define OV7670_REG_AWBC3               (0x45U)  /* Def: 0x45; R/W: RW; Reserved */
#define OV7670_REG_AWBC4               (0x46U)  /* Def: 0x61; R/W: RW; Reserved */
#define OV7670_REG_AWBC5               (0x47U)  /* Def: 0x51; R/W: RW; Reserved */
#define OV7670_REG_AWBC6               (0x48U)  /* Def: 0x79; R/W: RW; Reserved */
#define OV7670_REG_REG4B               (0x4BU)  /* Def: 0x0; R/W: RW; Register 4B */
#define OV7670_REG_DNSTH               (0x4CU)  /* Def: 0x0; R/W: RW; De-noise Strength */
#define OV7670_REG_MTX1                (0x4FU)  /* Def: 0x40; R/W: RW; Matrix Coefficient 1 */
#define OV7670_REG_MTX2                (0x50U)  /* Def: 0x34; R/W: RW; Matrix Coefficient 2 */
#define OV7670_REG_MTX3                (0x51U)  /* Def: 0x0C; R/W: RW; Matrix Coefficient 3 */
#define OV7670_REG_MTX4                (0x52U)  /* Def: 0x17; R/W: RW; Matrix Coefficient 4 */
#define OV7670_REG_MTX5                (0x53U)  /* Def: 0x29; R/W: RW; Matrix Coefficient 5 */
#define OV7670_REG_MTX6                (0x54U)  /* Def: 0x40; R/W: RW; Matrix Coefficient 6 */
#define OV7670_REG_BRIGHT              (0x55U)  /* Def: 0x0; R/W: RW; Brightness Control */
#define OV7670_REG_CONTRAS             (0x56U)  /* Def: 0x40; R/W: RW; Contrast Control */
#define OV7670_REG_CONTRAS_CENTER      (0x57U)  /* Def: 0x80; R/W: RW; Contrast Center */
#define OV7670_REG_MTXS                (0x58U)  /* Def: 0x1E; R/W: RW; Matrix Coefficient Sign for Coefficient 5 to 0 */
#define OV7670_REG_LCC1                (0x62U)  /* Def: 0x0; R/W: RW; Lens Correction Option 1 - X Coordinate of Lens Correction Center Relative to Array Center */
#define OV7670_REG_LCC2                (0x63U)  /* Def: 0x0; R/W: RW; Lens Correction Option 2 - Y Coordinate of Lens Correction Center Relative to Array Center */
#define OV7670_REG_LCC3                (0x64U)  /* Def: 0x50; R/W: RW; Lens Correction Option 3 */
#define OV7670_REG_LCC4                (0x65U)  /* Def: 0x30; R/W: RW; Lens Correction Option 4 - Radius of the circular section where no compensation applies */
#define OV7670_REG_LCC5                (0x66U)  /* Def: 0x0; R/W: RW; Lens Correction Control 5 */
#define OV7670_REG_MANU                (0x67U)  /* Def: 0x80; R/W: RW; Manual U Value (effective only when register TSLB[4] is high) */
#define OV7670_REG_MANV                (0x68U)  /* Def: 0x80; R/W: RW; Manual V Value (effective only when register TSLB[4] is high) */
#define OV7670_REG_GFIX                (0x69U)  /* Def: 0x0; R/W: RW; Fix Gain Control */
#define OV7670_REG_GGAIN               (0x6AU)  /* Def: 0x0; R/W: RW; G Channel AWB Gain */
#define OV7670_REG_DBLV                (0x6BU)  /* Def: 0x0A; R/W: RW; PLL control */
#define OV7670_REG_AWBCTR3             (0x6CU)  /* Def: 0x2; R/W: RW; AWB Control 3 */
#define OV7670_REG_AWBCTR2             (0x6DU)  /* Def: 0x55; R/W: RW; AWB Control 2 */
#define OV7670_REG_AWBCTR1             (0x6EU)  /* Def: 0xC0; R/W: RW; AWB Control 1 */
#define OV7670_REG_AWBCTR0             (0x6FU)  /* Def: 0x9A; R/W: RW; AWB Control 0 */
#define OV7670_REG_SCALING_XSC         (0x70U)  /* Def: 0x3A; R/W: RW; Bit[7]: Test_pattern[0] - works with test_pattern[1] test_pattern (SCALING_XSC[7], SCALING_YSC[7]): */
#define OV7670_REG_SCALING_YSC         (0x71U)  /* Def: 0x35; R/W: RW; Bit[7]: Test_pattern[1] - works with test_pattern[0] test_pattern (SCALING_XSC[7], SCALING_YSC[7]): */
#define OV7670_REG_SCALING_DCWCTR      (0x72U)  /* Def: 0x11; R/W: RW; DCW Control */
#define OV7670_REG_SCALING_PCLK_DIV    (0x73U)  /* Def: 0x0; R/W: RW; Register 73 */
#define OV7670_REG_REG74               (0x74U)  /* Def: 0x0; R/W: RW; Register 74 */
#define OV7670_REG_REG75               (0x75U)  /* Def: 0x0F; R/W: RW; Register 75 */
#define OV7670_REG_REG76               (0x76U)  /* Def: 0x1; R/W: RW; Register 76 */
#define OV7670_REG_REG77               (0x77U)  /* Def: 0x10; R/W: RW; Register 77 */
#define OV7670_REG_SLOP                (0x7AU)  /* Def: 0x24; R/W: RW; Gamma Curve Highest Segment Slope - calculated as follows: SLOP[7:0] = (0x100 - GAM15[7:0]) x 4/3 */
#define OV7670_REG_GAM1                (0x7BU)  /* Def: 0x4; R/W: RW; Gamma Curve 1st Segment Input End Point 0x04 Output Value */
#define OV7670_REG_GAM2                (0x7CU)  /* Def: 0x7; R/W: RW; Gamma Curve 2nd Segment Input End Point 0x08 Output Value */
#define OV7670_REG_GAM3                (0x7DU)  /* Def: 0x10; R/W: RW; Gamma Curve 3rd Segment Input End Point 0x10 Output Value */
#define OV7670_REG_GAM4                (0x7EU)  /* Def: 0x28; R/W: RW; Gamma Curve 4th Segment Input End Point 0x20 Output Value */
#define OV7670_REG_GAM5                (0x7FU)  /* Def: 0x36; R/W: RW; Gamma Curve 5th Segment Input End Point 0x28 Output Value */
#define OV7670_REG_GAM6                (0x80U)  /* Def: 0x44; R/W: RW; Gamma Curve 6th Segment Input End Point 0x30 Output Value */
#define OV7670_REG_GAM7                (0x81U)  /* Def: 0x52; R/W: RW; Gamma Curve 7th Segment Input End Point 0x38 Output Value */
#define OV7670_REG_GAM8                (0x82U)  /* Def: 0x60; R/W: RW; Gamma Curve 8th Segment Input End Point 0x40 Output Value */
#define OV7670_REG_GAM9                (0x83U)  /* Def: 0x6C; R/W: RW; Gamma Curve 9th Segment Input End Point 0x48 Output Value */
#define OV7670_REG_GAM10               (0x84U)  /* Def: 0x78; R/W: RW; Gamma Curve 10th Segment Input End Point 0x50 Output Value */
#define OV7670_REG_GAM11               (0x85U)  /* Def: 0x8C; R/W: RW; Gamma Curve 11th Segment Input End Point 0x60 Output Value */
#define OV7670_REG_GAM12               (0x86U)  /* Def: 0x9E; R/W: RW; Gamma Curve 12th Segment Input End Point 0x70 Output Value */
#define OV7670_REG_GAM13               (0x87U)  /* Def: 0xBB; R/W: RW; Gamma Curve 13th Segment Input End Point 0x90 Output Value */
#define OV7670_REG_GAM14               (0x88U)  /* Def: 0xD2; R/W: RW; Gamma Curve 14th Segment Input End Point 0xB0 Output Value */
#define OV7670_REG_GAM15               (0x89U)  /* Def: 0xE5; R/W: RW; Gamma Curve 15th Segment Input End Point 0xD0 Output Value */
#define OV7670_REG_RGB444              (0x8CU)  /* Def: 0x0; R/W: RW; Register 8C */
#define OV7670_REG_DM_LNL              (0x92U)  /* Def: 0x0; R/W: RW; Dummy Line low 8 bits */
#define OV7670_REG_DM_LNH              (0x93U)  /* Def: 0x0; R/W: RW; Dummy Line high 8 bits */
#define OV7670_REG_LCC6                (0x94U)  /* Def: 0x50; R/W: RW; Lens Correction Option 6 (effective only when LCC5[2] is high) */
#define OV7670_REG_LCC7                (0x95U)  /* Def: 0x50; R/W: RW; Lens Correction Option 7 (effective only when LCC5[2] is high) */
#define OV7670_REG_BD50ST              (0x9DU)  /* Def: 0x99; R/W: RW; 50 Hz Banding Filter Value */
#define OV7670_REG_BD60ST              (0x9EU)  /* Def: 0x7F; R/W: RW; 60 Hz Banding Filter Value */
#define OV7670_REG_HAECC1              (0x9FU)  /* Def: 0xC0; R/W: RW; Histogram-based AEC/AGC Control 1 */
#define OV7670_REG_HAECC2              (0xA0U)  /* Def: 0x90; R/W: RW; Histogram-based AEC/AGC Control 2 */
#define OV7670_REG_SCALING_PCLK_DELAY  (0xA2U)  /* Def: 0x2; R/W: RW; Pixel Clock Delay */
#define OV7670_REG_NT_CTRL             (0xA4U)  /* Def: 0x0; R/W: RW; Register A4 */
#define OV7670_REG_BD50MAX             (0xA5U)  /* Def: 0x0F; R/W: RW; 50Hz Banding Step Limit */
#define OV7670_REG_HAECC3              (0xA6U)  /* Def: 0xF0; R/W: RW; Histogram-based AEC/AGC Control 3 */
#define OV7670_REG_HAECC4              (0xA7U)  /* Def: 0xC1; R/W: RW; Histogram-based AEC/AGC Control 4 */
#define OV7670_REG_HAECC5              (0xA8U)  /* Def: 0xF0; R/W: RW; Histogram-based AEC/AGC Control 5 */
#define OV7670_REG_HAECC6              (0xA9U)  /* Def: 0xC1; R/W: RW; Histogram-based AEC/AGC Control 6 */
#define OV7670_REG_HAECC7              (0xAAU)  /* Def: 0x14; R/W: RW; Bit[7]: AEC algorithm selection */
#define OV7670_REG_BD60MAX             (0xABU)  /* Def: 0x0F; R/W: RW; 60Hz Banding Step Limit */
#define OV7670_REG_STR_OPT             (0xACU)  /* Def: 0x0; R/W: RW; Register AC */
#define OV7670_REG_STR_R               (0xADU)  /* Def: 0x80; R/W: RW; R Gain for LED Output Frame */
#define OV7670_REG_STR_G               (0xAEU)  /* Def: 0x80; R/W: RW; G Gain for LED Output Frame */
#define OV7670_REG_STR_B               (0xAFU)  /* Def: 0x80; R/W: RW; B Gain for LED Output Frame */
#define OV7670_REG_RSRVD               (0xB0U)  /* */
#define OV7670_REG_ABLC1               (0xB1U)  /* Def: 0x0; R/W: RW; */
#define OV7670_REG_THL_ST              (0xB3U)  /* Def: 0x80; R/W: RW; ABLC Target */
#define OV7670_REG_THL_DLT             (0xB5U)  /* Def: 0x4; R/W: RW; ABLC Stable Range */
#define OV7670_REG_AD_CHB              (0xBEU)  /* Def: 0x0; R/W: RW; Blue Channel Black Level Compensation */
#define OV7670_REG_AD_CHR              (0xBFU)  /* Def: 0x0; R/W: RW; Red Channel Black Level Compensation */
#define OV7670_REG_AD_CHGb             (0xC0U)  /* Def: 0x0; R/W: RW; Gb Channel Black Level Compensation */
#define OV7670_REG_AD_CHGr             (0xC1U)  /* Def: 0x0; R/W: RW; Gr Channel Black Level Compensation */
#define OV7670_REG_SATCTR              (0xC9U)  /* Def: 0xC0; R/W: RW; Saturation Control */
#define OV7670_REG_DUMMY               (0xFFU)

extern void OV7670_Init(void);
extern void OV7670_Start(void);
extern void OV7670_Stop(void);

#if (OV7670_STREAM_MODE == OV7670_SRTEAM_MODE_BY_FRAME)
extern void HAL_DCMI_VsyncEventCallback(DCMI_HandleTypeDef *hdcmi);
extern void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi);
#else  /* (OV7670_STREAM_MODE == OV7670_STREAM_MODE_BY_LINE) */
extern void HAL_DCMI_LineEventCallback(DCMI_HandleTypeDef *hdcmi);
#endif /* (OV7670_STREAM_MODE == OV7670_STREAM_MODE_BY_LINE) */

#endif /* OV7670_H_ */