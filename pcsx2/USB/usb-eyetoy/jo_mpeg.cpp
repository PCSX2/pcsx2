/* public domain Simple, Minimalistic, No Allocations MPEG writer - http://jonolick.com
 *
 * Latest revisions:
 * 	1.02 (22-03-2017) Fixed AC encoding bug.
 *                    Fixed color space bug (thx r- lyeh!)
 * 	1.01 (18-10-2016) warning fixes
 * 	1.00 (25-09-2016) initial release
 *
 * Basic usage:
 *	char *frame = new char[width*height*4]; // 4 component. RGBX format, where X is unused
 *	FILE *fp = fopen("foo.mpg", "wb");
 *	jo_write_mpeg(fp, frame, width, height, 60);  // frame 0
 *	jo_write_mpeg(fp, frame, width, height, 60);  // frame 1
 *	jo_write_mpeg(fp, frame, width, height, 60);  // frame 2
 *	...
 *	fclose(fp);
 *
 * Notes:
 * 	Only supports 24, 25, 30, 50, or 60 fps
 *
 * 	I don't know if decoders support changing of fps, or dimensions for each frame.
 * 	Movie players *should* support it as the spec allows it, but ...
 *
 * 	MPEG-1/2 currently has no active patents as far as I am aware.
 * 	
 *	http://dvd.sourceforge.net/dvdinfo/mpeghdrs.html
 *	http://www.cs.cornell.edu/dali/api/mpegvideo-c.html
 * */

#include "PrecompiledHeader.h"
#include <stdio.h>
#include <math.h>
#include <memory.h>

#include "jo_mpeg.h"

// Huffman tables
static const unsigned char s_jo_HTDC_Y[9][2] = {{4,3}, {0,2}, {1,2}, {5,3}, {6,3}, {14,4}, {30,5}, {62,6}, {126,7}};
static const unsigned char s_jo_HTDC_C[9][2] = {{0,2}, {1,2}, {2,2}, {6,3}, {14,4}, {30,5}, {62,6}, {126,7}, {254,8}};
static const unsigned char s_jo_HTAC[32][40][2] = {
{{6,3},{8,5},{10,6},{12,8},{76,9},{66,9},{20,11},{58,13},{48,13},{38,13},{32,13},{52,14},{50,14},{48,14},{46,14},{62,15},{62,15},{58,15},{56,15},{54,15},{52,15},{50,15},{48,15},{46,15},{44,15},{42,15},{40,15},{38,15},{36,15},{34,15},{32,15},{48,16},{46,16},{44,16},{42,16},{40,16},{38,16},{36,16},{34,16},{32,16},},
{{6,4},{12,7},{74,9},{24,11},{54,13},{44,14},{42,14},{62,16},{60,16},{58,16},{56,16},{54,16},{52,16},{50,16},{38,17},{36,17},{34,17},{32,17}},
{{10,5},{8,8},{22,11},{40,13},{40,14}},
{{14,6},{72,9},{56,13},{38,14}},
{{12,6},{30,11},{36,13}},  {{14,7},{18,11},{36,14}},  {{10,7},{60,13},{40,17}},
{{8,7},{42,13}},  {{14,8},{34,13}},  {{10,8},{34,14}},  {{78,9},{32,14}},  {{70,9},{52,17}},  {{68,9},{50,17}},  {{64,9},{48,17}},  {{28,11},{46,17}},  {{26,11},{44,17}},  {{16,11},{42,17}},
{{62,13}}, {{52,13}}, {{50,13}}, {{46,13}}, {{44,13}}, {{62,14}}, {{60,14}}, {{58,14}}, {{56,14}}, {{54,14}}, {{62,17}}, {{60,17}}, {{58,17}}, {{56,17}}, {{54,17}},
};
static const float s_jo_quantTbl[64] = {
	0.015625f,0.005632f,0.005035f,0.004832f,0.004808f,0.005892f,0.007964f,0.013325f,
	0.005632f,0.004061f,0.003135f,0.003193f,0.003338f,0.003955f,0.004898f,0.008828f,
	0.005035f,0.003135f,0.002816f,0.003013f,0.003299f,0.003581f,0.005199f,0.009125f,
	0.004832f,0.003484f,0.003129f,0.003348f,0.003666f,0.003979f,0.005309f,0.009632f,
	0.005682f,0.003466f,0.003543f,0.003666f,0.003906f,0.004546f,0.005774f,0.009439f,
	0.006119f,0.004248f,0.004199f,0.004228f,0.004546f,0.005062f,0.006124f,0.009942f,
	0.008883f,0.006167f,0.006096f,0.005777f,0.006078f,0.006391f,0.007621f,0.012133f,
	0.016780f,0.011263f,0.009907f,0.010139f,0.009849f,0.010297f,0.012133f,0.019785f,
};
static const unsigned char s_jo_ZigZag[] = { 0,1,5,6,14,15,27,28,2,4,7,13,16,26,29,42,3,8,12,17,25,30,41,43,9,11,18,24,31,40,44,53,10,19,23,32,39,45,52,54,20,22,33,38,46,51,55,60,21,34,37,47,50,56,59,61,35,36,48,49,57,58,62,63 };

typedef struct {
	unsigned char *buf_ptr;
	int buf, cnt;
} jo_bits_t;

static void jo_writeBits(jo_bits_t *b, int value, int count) {
	b->cnt += count;
	b->buf |= value << (24 - b->cnt);
	while(b->cnt >= 8) {
		unsigned char c = (b->buf >> 16) & 255;
		//putc(c, b->fp);
		*(b->buf_ptr) = c & 0xff;
		b->buf_ptr++;
		b->buf <<= 8;
		b->cnt -= 8;
	}
}

static void jo_DCT(float *d0, float *d1, float *d2, float *d3, float *d4, float *d5, float *d6, float *d7) {
	float tmp0 = *d0 + *d7;
	float tmp7 = *d0 - *d7;
	float tmp1 = *d1 + *d6;
	float tmp6 = *d1 - *d6;
	float tmp2 = *d2 + *d5;
	float tmp5 = *d2 - *d5;
	float tmp3 = *d3 + *d4;
	float tmp4 = *d3 - *d4;

	// Even part
	float tmp10 = tmp0 + tmp3;	// phase 2
	float tmp13 = tmp0 - tmp3;
	float tmp11 = tmp1 + tmp2;
	float tmp12 = tmp1 - tmp2;

	*d0 = tmp10 + tmp11; 		// phase 3
	*d4 = tmp10 - tmp11;

	float z1 = (tmp12 + tmp13) * 0.707106781f; // c4
	*d2 = tmp13 + z1; 		// phase 5
	*d6 = tmp13 - z1;

	// Odd part
	tmp10 = tmp4 + tmp5; 		// phase 2
	tmp11 = tmp5 + tmp6;
	tmp12 = tmp6 + tmp7;

	// The rotator is modified from fig 4-8 to avoid extra negations.
	float z5 = (tmp10 - tmp12) * 0.382683433f; // c6
	float z2 = tmp10 * 0.541196100f + z5; // c2-c6
	float z4 = tmp12 * 1.306562965f + z5; // c2+c6
	float z3 = tmp11 * 0.707106781f; // c4

	float z11 = tmp7 + z3;		// phase 5
	float z13 = tmp7 - z3;

	*d5 = z13 + z2;			// phase 6
	*d3 = z13 - z2;
	*d1 = z11 + z4;
	*d7 = z11 - z4;
}

static int jo_processDU(jo_bits_t *bits, float A[64], const unsigned char htdc[9][2], int DC) {
	for(int dataOff=0; dataOff<64; dataOff+=8) {
		jo_DCT(&A[dataOff], &A[dataOff+1], &A[dataOff+2], &A[dataOff+3], &A[dataOff+4], &A[dataOff+5], &A[dataOff+6], &A[dataOff+7]);
	}
	for(int dataOff=0; dataOff<8; ++dataOff) {
		jo_DCT(&A[dataOff], &A[dataOff+8], &A[dataOff+16], &A[dataOff+24], &A[dataOff+32], &A[dataOff+40], &A[dataOff+48], &A[dataOff+56]);
	}
	int Q[64];
	for(int i=0; i<64; ++i) {
		float v = A[i]*s_jo_quantTbl[i];
		Q[s_jo_ZigZag[i]] = (int)(v < 0 ? ceilf(v - 0.5f) : floorf(v + 0.5f));
	}

	DC = Q[0] - DC;
	int aDC = DC < 0 ? -DC : DC;
	int size = 0;
	int tempval = aDC;
	while(tempval) {
		size++;
		tempval >>= 1;
	}
	jo_writeBits(bits, htdc[size][0], htdc[size][1]);
	if(DC < 0) aDC ^= (1 << size) - 1;
	jo_writeBits(bits, aDC, size);

	int endpos = 63;
	for(; (endpos>0)&&(Q[endpos]==0); --endpos) { /* do nothing */ }
	for(int i = 1; i <= endpos;) {
		int run = 0;
		while (Q[i]==0 && i<endpos) {
			++run;
			++i;
		}
		int AC = Q[i++];
		int aAC = AC < 0 ? -AC : AC;
		int code = 0, size = 0;
		if (run<32 && aAC<=40) {
			code = s_jo_HTAC[run][aAC-1][0];
			size = s_jo_HTAC[run][aAC-1][1];
			if (AC < 0) code += 1;
		}
		if(!size) {
			jo_writeBits(bits, 1, 6);
			jo_writeBits(bits, run, 6);
			if (AC < -127) {
				jo_writeBits(bits, 128, 12);
			} else if(AC > 127) {
				jo_writeBits(bits, 0, 12);
			}
			code = AC & 0xFFF;
			size = 12;
		}
		jo_writeBits(bits, code, size);
	}
	jo_writeBits(bits, 2, 2);

	return Q[0];
}

void write_ipu_header(jo_bits_t* bits, int width, int height) {
	jo_writeBits(bits, 0x69, 8);
	jo_writeBits(bits, 0x70, 8);
	jo_writeBits(bits, 0x75, 8);
	jo_writeBits(bits, 0x6D, 8);

	jo_writeBits(bits, 0x00, 8);
	jo_writeBits(bits, 0x00, 8);
	jo_writeBits(bits, 0x00, 8);
	jo_writeBits(bits, 0x00, 8);

	jo_writeBits(bits, width & 0xFF,  8);
	jo_writeBits(bits, width >> 8,    8);
	jo_writeBits(bits, height & 0xFF, 8);
	jo_writeBits(bits, height >> 8,   8);

	jo_writeBits(bits, 0x01, 8);
	jo_writeBits(bits, 0x00, 8);
	jo_writeBits(bits, 0x00, 8);
	jo_writeBits(bits, 0x00, 8);
}

unsigned long jo_write_mpeg(unsigned char *mpeg_buf, const unsigned char *raw, int width, int height, int format, int flipx, int flipy) {
	int lastDCY = 128, lastDCCR = 128, lastDCCB = 128;
	unsigned char *head = mpeg_buf;
	jo_bits_t bits = {mpeg_buf};

	write_ipu_header(&bits, width, height);

	jo_writeBits(&bits, 0x00, 8);
	for (int vblock = 0; vblock < (height+15)/16; vblock++) {
		for (int hblock = 0; hblock < (width+15)/16; hblock++) {
			if (vblock == 0 && hblock == 0) {
				jo_writeBits(&bits, 0b01, 2); // macroblock_type = intra+quant
				jo_writeBits(&bits, 8, 5); // quantiser_scale_code = 8
			} else {
				jo_writeBits(&bits, 0b1, 1); // macroblock_address_increment
				jo_writeBits(&bits, 0b1, 1); // macroblock_type = intra
			}

			float Y[256], CBx[256], CRx[256];
			float CB[64], CR[64];

			if (format == JO_RGBX) {
				for (int i=0; i<256; ++i) {
					int y = vblock*16+(i/16);
					int x = hblock*16+(i&15);
					x = x >= width ? width-1 : x;
					y = y >= height ? height-1 : y;
					if (flipx) x = width - 1 - x;
					if (flipy) y = height - 1 - y;
					const unsigned char *c = raw + y*width*4+x*4;
					float r, g, b;
					r = c[0], g = c[1], b = c[2];
					Y[i] = (0.299f*r + 0.587f*g + 0.114f*b) * (219.f/255) + 16;
					CBx[i] = (-0.299f*r - 0.587f*g + 0.886f*b) * (224.f/255) + 128;
					CRx[i] = (0.701f*r - 0.587f*g - 0.114f*b) * (224.f/255) + 128;
				}
				// Downsample Cb,Cr (420 format)
				for (int i=0; i<64; ++i) {
					int j =(i&7)*2 + (i&56)*4;
					CB[i] = (CBx[j] + CBx[j+1] + CBx[j+16] + CBx[j+17]) * 0.25f;
					CR[i] = (CRx[j] + CRx[j+1] + CRx[j+16] + CRx[j+17]) * 0.25f;
				}
			} else
			if (format == JO_BGR24 || format == JO_RGB24) {
				for (int i=0; i<256; ++i) {
					int y = vblock*16+(i/16);
					int x = hblock*16+(i&15);
					x = x >= width ? width-1 : x;
					y = y >= height ? height-1 : y;
					if (flipx) x = width - 1 - x;
					if (flipy) y = height - 1 - y;
					const unsigned char *c = raw + y*width*3+x*3;
					float r, g, b;
					if (format == JO_BGR24) {
						r = c[2], g = c[1], b = c[0];
					} else {
						r = c[0], g = c[1], b = c[2];
					}
					Y[i] = (0.299f*r + 0.587f*g + 0.114f*b) * (219.f/255) + 16;
					CBx[i] = (-0.299f*r - 0.587f*g + 0.886f*b) * (224.f/255) + 128;
					CRx[i] = (0.701f*r - 0.587f*g - 0.114f*b) * (224.f/255) + 128;
				}
				// Downsample Cb,Cr (420 format)
				for (int i=0; i<64; ++i) {
					int j =(i&7)*2 + (i&56)*4;
					CB[i] = (CBx[j] + CBx[j+1] + CBx[j+16] + CBx[j+17]) * 0.25f;
					CR[i] = (CRx[j] + CRx[j+1] + CRx[j+16] + CRx[j+17]) * 0.25f;
				}
			} else
			if (format == JO_YUYV) {
				for (int i=0; i<256; i+=2) {
					int y = vblock*16+(i/16);
					int x = hblock*16+(i&15);
					x = x >= width ? width-1 : x;
					y = y >= height ? height-1 : y;
					if (flipx) x = width - 1 - x;
					if (flipy) y = height - 1 - y;
					const unsigned char *c = raw + y*width*2+x*2-2;
					if (flipx) {
						Y[i+1]  = c[0];
						CB[i/4] = c[1];
						Y[i]    = c[2];
						CR[i/4] = c[3];
					} else {
						Y[i]    = c[2];
						CB[i/4] = c[3];
						Y[i+1]  = c[4];
						CR[i/4] = c[5];
					}
				}
			}

			for (int k1=0; k1<2; ++k1) {
				for (int k2=0; k2<2; ++k2) {
					float block[64];
					for (int i=0; i<64; i+=8) {
						int j = (i&7)+(i&56)*2 + k1*8*16 + k2*8;
						memcpy(block+i, Y+j, 8*sizeof(Y[0]));
					}
					lastDCY = jo_processDU(&bits, block, s_jo_HTDC_Y, lastDCY);
				}
			}
			lastDCCB = jo_processDU(&bits, CB, s_jo_HTDC_C, lastDCCB);
			lastDCCR = jo_processDU(&bits, CR, s_jo_HTDC_C, lastDCCR);
		}
	}
	jo_writeBits(&bits, 0, 7);

	// End of Sequence
	*(bits.buf_ptr++) = 0x00;
	*(bits.buf_ptr++) = 0x00;
	*(bits.buf_ptr++) = 0x01;
	*(bits.buf_ptr++) = 0xb0;

	return bits.buf_ptr - head;
}
