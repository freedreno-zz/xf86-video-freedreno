/*
 * Copyright © 2012 Rob Clark <robclark@freedesktop.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FREEDRENO_Z1XX_H_
#define FREEDRENO_Z1XX_H_

/* not entirely sure how much delta there is between z160 and z180..
 * for now I'm assuming they are largely similar
 */

enum z1xx_reg {
	G2D_BASE0                = 0x00,
	G2D_CFG0                 = 0x01,
	G2D_CFG1                 = 0x03,
	G2D_SCISSORX             = 0x08,
	G2D_SCISSORY             = 0x09,
	G2D_FOREGROUND           = 0x0a,
	G2D_BACKGROUND           = 0x0b,
	G2D_ALPHABLEND           = 0x0c,
	G2D_BLENDERCFG           = 0x11,
	G2D_CONST0               = 0xb0,
	G2D_CONST1               = 0xb1,
	G2D_CONST2               = 0xb2,
	G2D_CONST3               = 0xb3,
	G2D_CONST4               = 0xb4,
	G2D_CONST5               = 0xb5,
	G2D_CONST6               = 0xb6,
	G2D_CONST7               = 0xb7,
	G2D_GRADIENT             = 0xd0,
	G2D_XY                   = 0xf0,
	G2D_WIDTHHEIGHT          = 0xf1,
	G2D_SXY                  = 0xf2,
	G2D_COLOR                = 0xff,

	VGV3_WRITERAW            = 0x7c,

	GRADW_TEXCFG             = 0xd1,
	GRADW_TEXSIZE            = 0xd2,
	GRADW_TEXBASE            = 0xd3,
};

/* used to write one register.. at most 24 bits or maybe less, register
 * value is OR'd with this
 */

static inline uint32_t REG(enum z1xx_reg reg)
{
	return reg << 24;
}

/* used to write one or more registers: */
static inline uint32_t REGM(enum z1xx_reg reg, uint8_t count)
{
	return REG(VGV3_WRITERAW) | (count << 8) | reg;
}

#endif /* FREEDRENO_Z1XX_H_ */
