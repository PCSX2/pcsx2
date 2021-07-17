/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* I2C registers */
#define R511_I2C_CTL 0x40
#define R51x_I2C_W_SID 0x41
#define R51x_I2C_SADDR_3 0x42
#define R51x_I2C_SADDR_2 0x43
#define R51x_I2C_R_SID 0x44
#define R51x_I2C_DATA 0x45
#define R518_I2C_CTL 0x47 /* OV518(+) only */
#define OVFX2_I2C_ADDR 0x00

/* OV519 Camera interface register numbers */
#define OV519_R10_H_SIZE 0x10
#define OV519_R11_V_SIZE 0x11
#define OV519_R12_X_OFFSETL 0x12
#define OV519_R13_X_OFFSETH 0x13
#define OV519_R14_Y_OFFSETL 0x14
#define OV519_R15_Y_OFFSETH 0x15
#define OV519_R16_DIVIDER 0x16
#define OV519_R20_DFR 0x20
#define OV519_R25_FORMAT 0x25
#define OV519_RA0_FORMAT 0xA0
#define OV519_RA0_FORMAT_MPEG 0x42
#define OV519_RA0_FORMAT_JPEG 0x33

/* OV519 System Controller register numbers */
#define OV519_R51_RESET1 0x51
#define OV519_R54_EN_CLK1 0x54
#define OV519_R57_SNAPSHOT 0x57

#define OV519_GPIO_DATA_OUT0 0x71
#define OV519_GPIO_IO_CTRL0 0x72

/* OV7610 registers */
#define OV7610_REG_GAIN 0x00    /* gain setting (5:0) */
#define OV7610_REG_BLUE 0x01    /* blue channel balance */
#define OV7610_REG_RED 0x02     /* red channel balance */
#define OV7610_REG_SAT 0x03     /* saturation */
#define OV8610_REG_HUE 0x04     /* 04 reserved */
#define OV7610_REG_CNT 0x05     /* Y contrast */
#define OV7610_REG_BRT 0x06     /* Y brightness */
#define OV7610_REG_COM_A 0x12   /* misc common regs */
#define OV7610_REG_COM_A_MASK_MIRROR 0x40 /* mirror image */
#define OV7610_REG_COM_C 0x14   /* misc common regs */
#define OV7610_REG_ID_HIGH 0x1c /* manufacturer ID MSB */
#define OV7610_REG_ID_LOW 0x1d  /* manufacturer ID LSB */
#define OV7610_REG_COM_I 0x29   /* misc settings */
