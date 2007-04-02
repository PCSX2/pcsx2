/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h> 
#include <stdio.h> 

#include "PsxCommon.h"


u32 gpuStatus = 0x14802000;
int gpuMode = 0;
u8  gpuCmd;
int gpuDataC = 0;
int gpuDataP;
u32 gpuDataRet;
u32 gpuData[256];
u32 gpuInfo[8] = {0, 0, 0, 0, 0, 0, 0, 2};
int imageTransfer;
s16 imTX, imTXc, imTY, imTYc;
s16 imageX0, imageX1;
s16 imageY0, imageY1;

// Draw Primitives

void primPolyF3(unsigned char * baseAddr) {	
/*    PolyF3 prim;
    unsigned int c;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyF3));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;

    c = MAKERGB15(prim.r, prim.g, prim.b);

    if (prim.code & 2) {
	SetST(textABR);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    }
    triangle(drawvram, prim.x0, prim.y0, prim.x1, prim.y1, prim.x2, prim.y2, c);
    if (prim.code & 2) drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);

    pslog ("PolyF3 %d,%d;%d,%d;%d,%d (st=%d)",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,(prim.code&2)!=0);*/
}

void primPolyFT3(unsigned char * baseAddr) {
/*    PolyFT3 prim;
    V3D_f v[3];
    Cache *tex;
    unsigned int c,id;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyFT3));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;

//    c = MAKERGB24(prim.r, prim.g, prim.b);
    c = ((prim.r*2 + prim.g*2 + prim.b*2) / 3); if (c > 0xff) c = 0xff;

    id = (prim.clut << 16) | prim.tpage;
    tex = CacheGet(id, MIN(prim.u0, MIN(prim.u1, prim.u2)),
		       MIN(prim.v0, MIN(prim.v1, prim.v2)),
		       MAX(prim.u0, MIN(prim.u1, prim.u2)),
		       MAX(prim.v0, MIN(prim.v1, prim.v2)));

    v[0].x = prim.x0; v[0].y = prim.y0; v[0].z = 0;
    v[0].u = prim.u0; v[0].v = prim.v0; v[0].c = c;
    v[1].x = prim.x1; v[1].y = prim.y1; v[1].z = 0;
    v[1].u = prim.u1; v[1].v = prim.v1; v[1].c = c;
    v[2].x = prim.x2; v[2].y = prim.y2; v[2].z = 0;
    v[2].u = prim.u2; v[2].v = prim.v2; v[2].c = c;

    if (prim.code & 2) {
	SetST((id&0x6)>>5);
	triangle3d_f(drawvram, POLYTYPE_ATEX_MASK_TRANS, tex->tex, &v[0], &v[1], &v[2]);
    }
    else triangle3d_f(drawvram, POLYTYPE_ATEX_MASK_LIT, tex->tex, &v[0], &v[1], &v[2]);

    pslog ("PolyFT3 %d,%d;%d,%d;%d,%d (st=%d)",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,(prim.code&2)!=0);*/
}

void primPolyF4(unsigned char *baseAddr) {
/*    PolyF4 prim;
    unsigned int c;
    int point[8];

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyF4));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;
    prim.x3 += offsX; prim.y3 += offsY;

    c = MAKERGB15(prim.r, prim.g, prim.b);

    point[0] = prim.x0; point[1] = prim.y0;
    point[2] = prim.x1; point[3] = prim.y1;
    point[4] = prim.x3; point[5] = prim.y3;
    point[6] = prim.x2; point[7] = prim.y2;

    if (prim.code & 2) {
	SetST(textABR);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    }
    polygon(drawvram, 4, point, c);
    if (prim.code & 2) drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);

    pslog ("PolyF4 %d,%d;%d,%d;%d,%d;%d,%d (st=%d)",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,prim.x3,prim.y3,(prim.code&2)!=0);*/
}

void primPolyFT4(unsigned char * baseAddr) {
/*    PolyFT4 prim;
    V3D_f v[4];
    Cache *tex;
    unsigned int c, id;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyFT4));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;
    prim.x3 += offsX; prim.y3 += offsY;

//    c = MAKERGB24(prim.r,prim.g,prim.b);
    c = (prim.r*2 + prim.g*2 + prim.b*2) / 3; if (c > 0xff) c = 0xff;

    id = (prim.clut << 16) | prim.tpage;
    tex = CacheGet(id, MIN(prim.u0, MIN(prim.u1, MIN(prim.u2, prim.u3))),
		       MIN(prim.v0, MIN(prim.v1, MIN(prim.v2, prim.v3))),
		       MAX(prim.u0, MAX(prim.u1, MAX(prim.u2, prim.u3))),
		       MAX(prim.v0, MAX(prim.v1, MAX(prim.v2, prim.v3))));

    v[0].x = prim.x0; v[0].y = prim.y0; v[0].z = 0;
    v[0].u = prim.u0; v[0].v = prim.v0; v[0].c = c;
    v[1].x = prim.x1; v[1].y = prim.y1; v[1].z = 0;
    v[1].u = prim.u1; v[1].v = prim.v1; v[1].c = c;
    v[2].x = prim.x2; v[2].y = prim.y2; v[2].z = 0;
    v[2].u = prim.u2; v[2].v = prim.v2; v[2].c = c;
    v[3].x = prim.x3; v[3].y = prim.y3; v[3].z = 0;
    v[3].u = prim.u3; v[3].v = prim.v3; v[3].c = c;

    if (prim.code & 2) {
	SetST((id&0x6)>>5);
	quad3d_f(drawvram, POLYTYPE_ATEX_MASK_TRANS, tex->tex, &v[0], &v[1], &v[3], &v[2]);
    }
    else quad3d_f(drawvram, POLYTYPE_ATEX_MASK_LIT, tex->tex, &v[0], &v[1], &v[3], &v[2]);

    pslog ("PolyFT4 %d,%d;%d,%d;%d,%d;%d,%d (st=%d) c=%x",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,prim.x3,prim.y3,(prim.code&2)!=0,c);*/
}

void primPolyG3(unsigned char *baseAddr) {	
/*    PolyG3 prim;
    V3D_f v[3];
    unsigned int c0,c1,c2;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyG3));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;

    c0 = MAKERGB24(prim.r0, prim.g0, prim.b0);
    c1 = MAKERGB24(prim.r1, prim.g1, prim.b1);
    c2 = MAKERGB24(prim.r2, prim.g2, prim.b2);

    v[0].x = prim.x0; v[0].y = prim.y0; v[0].z = 0; v[0].c = c0;
    v[1].x = prim.x1; v[1].y = prim.y1; v[1].z = 0; v[1].c = c1;
    v[2].x = prim.x2; v[2].y = prim.y2; v[2].z = 0; v[2].c = c2;

    triangle3d_f(drawvram, POLYTYPE_GRGB, NULL, &v[0], &v[1], &v[2]);

    pslog("polyG3 %d,%d;%d,%d;%d,%d (st=%d)",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,(prim.code&2)!=0);*/
}

void primPolyGT3(unsigned char *baseAddr) {	
/*    PolyGT3 prim;
    V3D_f v[3];
    Cache *tex;
    unsigned int c0,c1,c2,id;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyGT3));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;

//    c0 = MAKERGB24(prim.r0, prim.g0, prim.b0);
//    c1 = MAKERGB24(prim.r1, prim.g1, prim.b1);
//    c2 = MAKERGB24(prim.r2, prim.g2, prim.b2);
    c0 = (prim.r0*2 + prim.g0*2 + prim.b0*2) / 3; if (c0 > 0xff) c0 = 0xff;
    c1 = (prim.r1*2 + prim.g1*2 + prim.b1*2) / 3; if (c1 > 0xff) c1 = 0xff;
    c2 = (prim.r2*2 + prim.g2*2 + prim.b2*2) / 3; if (c2 > 0xff) c2 = 0xff;

    id = (prim.clut << 16) | prim.tpage;
    tex = CacheGet(id, MIN(prim.u0, MIN(prim.u1, prim.u2)),
		       MIN(prim.v0, MIN(prim.v1, prim.v2)),
		       MAX(prim.u0, MAX(prim.u1, prim.u2)),
		       MAX(prim.v0, MAX(prim.v1, prim.v2)));

    v[0].x = prim.x0; v[0].y = prim.y0; v[0].z = 0;
    v[0].u = prim.u0; v[0].v = prim.v0; v[0].c = c0;
    v[1].x = prim.x1; v[1].y = prim.y1; v[1].z = 0;
    v[1].u = prim.u1; v[1].v = prim.v1; v[1].c = c1;
    v[2].x = prim.x2; v[2].y = prim.y2; v[2].z = 0;
    v[2].u = prim.u2; v[2].v = prim.v2; v[2].c = c2;

    if (prim.code & 2) {
	SetST((id&0x6)>>5);
	triangle3d_f(drawvram, POLYTYPE_ATEX_MASK_TRANS, tex->tex, &v[0], &v[1], &v[2]);
    }
    else triangle3d_f(drawvram, POLYTYPE_ATEX_MASK_LIT, tex->tex, &v[0], &v[1], &v[2]);

    pslog("polyGT3 %d,%d;%d,%d;%d,%d (st=%d)",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,(prim.code&2)!=0);*/
}

void primPolyG4(unsigned char * baseAddr) {
/*    PolyG4 prim;
    V3D_f v[4];
    unsigned int c0,c1,c2,c3;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyG4));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;
    prim.x3 += offsX; prim.y3 += offsY;

    c0 = MAKERGB24(prim.r0,prim.g0,prim.b0);
    c1 = MAKERGB24(prim.r1,prim.g1,prim.b1);
    c2 = MAKERGB24(prim.r2,prim.g2,prim.b2);
    c3 = MAKERGB24(prim.r3,prim.g3,prim.b3);

    v[0].x = prim.x0; v[0].y = prim.y0; v[0].z = 0; v[0].c = c0;
    v[1].x = prim.x1; v[1].y = prim.y1; v[1].z = 0; v[1].c = c1;
    v[2].x = prim.x2; v[2].y = prim.y2; v[2].z = 0; v[2].c = c2;
    v[3].x = prim.x3; v[3].y = prim.y3; v[3].z = 0; v[3].c = c3;

    if (prim.code & 2) return;
    quad3d_f(drawvram, POLYTYPE_GRGB, NULL, &v[0], &v[1], &v[3],  &v[2]);

    pslog ("PolyG4 %d,%d;%d,%d;%d,%d;%d,%d (st=%d)",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,prim.x3,prim.y3,(prim.code&2)!=0);*/
}

void primPolyGT4(unsigned char *baseAddr) {	
/*    PolyGT4 prim;
    V3D_f v[4];
    Cache *tex;
    unsigned int c0,c1,c2,c3,id;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(PolyGT4));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;
    prim.x2 += offsX; prim.y2 += offsY;
    prim.x3 += offsX; prim.y3 += offsY;

//    c0 = MAKERGB24(prim.r0,prim.g0,prim.b0);
//    c1 = MAKERGB24(prim.r1,prim.g1,prim.b1);
//    c2 = MAKERGB24(prim.r2,prim.g2,prim.b2);
//    c3 = MAKERGB24(prim.r3,prim.g3,prim.b3);
    c0 = (prim.r0*2 + prim.g0*2 + prim.b0*2) / 3; if (c0 > 0xff) c0 = 0xff;
    c1 = (prim.r1*2 + prim.g1*2 + prim.b1*2) / 3; if (c1 > 0xff) c1 = 0xff;
    c2 = (prim.r2*2 + prim.g2*2 + prim.b2*2) / 3; if (c2 > 0xff) c2 = 0xff;
    c3 = (prim.r3*2 + prim.g3*2 + prim.b3*2) / 3; if (c3 > 0xff) c3 = 0xff;

    id = (prim.clut << 16) | prim.tpage;
    tex = CacheGet(id, MIN(prim.u0, MIN(prim.u1, MIN(prim.u2, prim.u3))),
		       MIN(prim.v0, MIN(prim.v1, MIN(prim.v2, prim.v3))),
		       MAX(prim.u0, MAX(prim.u1, MAX(prim.u2, prim.u3))),
		       MAX(prim.v0, MAX(prim.v1, MAX(prim.v2, prim.v3))));

    v[0].x = prim.x0; v[0].y = prim.y0; v[0].z = 0;
    v[0].u = prim.u0; v[0].v = prim.v0; v[0].c = c0;
    v[1].x = prim.x1; v[1].y = prim.y1; v[1].z = 0;
    v[1].u = prim.u1; v[1].v = prim.v1; v[1].c = c1;
    v[2].x = prim.x2; v[2].y = prim.y2; v[2].z = 0;
    v[2].u = prim.u2; v[2].v = prim.v2; v[2].c = c2;
    v[3].x = prim.x3; v[3].y = prim.y3; v[3].z = 0;
    v[3].u = prim.u3; v[3].v = prim.v3; v[3].c = c3;

    if (prim.code & 2) {
	SetST((id&0x6)>>5);
	quad3d_f(drawvram, POLYTYPE_ATEX_MASK_TRANS, tex->tex, &v[0], &v[1], &v[3], &v[2]);
    }
    else quad3d_f(drawvram, POLYTYPE_ATEX_MASK_LIT, tex->tex, &v[0], &v[1], &v[3], &v[2]);

    pslog ("PolyGT4 %d,%d;%d,%d;%d,%d;%d,%d (st=%d)",
	prim.x0,prim.y0,prim.x1,prim.y1,prim.x2,prim.y2,prim.x3,prim.y3,(prim.code&2)!=0);*/
}

void primLineF2(unsigned char *baseAddr) {
/*    LineF2 prim;
    unsigned int c;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(LineF2));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;

    c = MAKERGB15(prim.r, prim.g, prim.b);

    if (prim.code & 2) {
	SetST(textABR);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    }
    line(drawvram, prim.x0, prim.y0, prim.x1, prim.y1, c);
    if (prim.code & 2) drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);

    pslog ("LineF2 : %d,%d - %d,%d : color = %x\n",
	prim.x0,prim.y0,prim.x1,prim.y1,c);*/
}

void primLineF(unsigned char *baseAddr) {
/*    unsigned int c,x,y,lx=0,ly=0;
    unsigned int *baseAddri = (unsigned int *)baseAddr;
    int i,j;

    if (norender) return;

    for (i=0;i<99;i++)
	if (baseAddri[i]==0x55555555)
	    break;

    i--;

    c = MAKERGB15(baseAddr[0], baseAddr[1], baseAddr[2]);

    if (baseAddr[3] & 2) {
	SetST(textABR);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    }

    for (j=0;j<i;j++) {
	x = baseAddri[j+1]&0xffff;
	y = (baseAddri[j+1]>>16)&0xffff;

	x += offsX;
	y += offsY;

	if (j>0)
	    line(drawvram, x, y, lx, ly, c);

	lx = x;
	ly = y;
    }

    if (baseAddr[3] & 2) drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);

    pslog ("LineF vert = %d : color = %x\n",
	i,c);*/
}

void primLineG2(unsigned char *baseAddr) {
/*    LineG2 prim;
    V3D_f v[2];
    unsigned int c0,c1;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(LineG2));

    prim.x0 += offsX; prim.y0 += offsY;
    prim.x1 += offsX; prim.y1 += offsY;

    c0 = MAKERGB24(prim.r0, prim.g0, prim.b0);
    c1 = MAKERGB24(prim.r1, prim.g1, prim.b1);

    v[0].x = prim.x0; v[0].y = prim.y0; v[0].z = 0; v[0].c = c0;
    v[1].x = prim.x1; v[1].y = prim.y1; v[1].z = 0; v[1].c = c1;

    if (prim.code & 2) {
	SetST(textABR);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    }
    triangle3d_f(drawvram, POLYTYPE_GRGB, NULL, &v[0], &v[1], &v[1]);
    if (prim.code & 2) drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);

    pslog ("LineG2 : %d,%d - %d,%d : color = %x,%x\n",
	prim.x0,prim.y0,prim.x1,prim.y1,c0,c1);*/
}

void primLineG(unsigned char *baseAddr) {
/*    unsigned int c,x,y,lx=0,ly=0,lc=0;
    unsigned int *baseAddri = (unsigned int *)baseAddr;
    int i,j;

    if (norender) return;

    for (i=0;i<99;i++)
	if (baseAddri[i]==0x55555555)
	    break;

    i--;
    i/=2;


    for (j=0;j<i;j++) {
	c = MAKERGB15(baseAddr[0], baseAddr[1], baseAddr[2]);
	x = baseAddri[j+1]&0xffff;
	y = (baseAddri[j+1]>>16)&0xffff;
    
	x += offsX;
	y += offsY;

	if (j>0)
	    line(drawvram, x, y, lx, ly, c);

	lx = x;
	ly = y;
	lc = c;
    }

    pslog ("LineG vert = %d\n",
	i);*/
}

void primTileS(unsigned char * baseAddr) {
/*    RectWH prim;

    memcpy(&prim, baseAddr, sizeof(RectWH));
    drawRect(&prim, ((short*)baseAddr)[4], ((short*)baseAddr)[5]);*/
}

void primSprtS(unsigned char * baseAddr) {
/*    SprtWH prim;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(SprtWH));
    drawSprite(&prim, ((short*)baseAddr)[6], ((short*)baseAddr)[7]);*/
}

void primTile1(unsigned char * baseAddr) {
/*    RectWH prim;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(RectWH));
    drawRect(&prim, 1, 1);*/
}

void primTile8(unsigned char * baseAddr) {
/*    RectWH prim;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(RectWH));
    drawRect(&prim, 8, 8);*/
}

void primSprt8(unsigned char * baseAddr) {
/*    SprtWH prim;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(SprtWH));
    drawSprite((SprtWH *)baseAddr, 8, 8);*/
}

void primTile16(unsigned char * baseAddr) {
/*    RectWH prim;

    memcpy(&prim, baseAddr, sizeof(RectWH));
    drawRect(&prim, 16, 16);*/
}

void primSprt16(unsigned char * baseAddr) {
/*    SprtWH prim;

    memcpy(&prim, baseAddr, sizeof(SprtWH));
    drawSprite(&prim, 16, 16);*/
}

// Command Primitives

void primVoid(unsigned char * baseAddr) {
}

void primClrCache(unsigned char * baseAddr) {
}

void primBlkFill(unsigned char * baseAddr) {
/*    Rect prim;
    unsigned int c;

    if (norender) return;

    memcpy(&prim, baseAddr, sizeof(Rect));

    if ((prim.x > 1023) | (prim.y > 511)) return;
    if (((prim.w + prim.x) > 1024) | ((prim.y + prim.h) > 512)) return;
    if (prim.x < 0) prim.x = 0;
    if (prim.y < 0) prim.y = 0;
    if (prim.w > 1023) prim.w = 1023;
    if (prim.h > 511) prim.h = 511;

    c = MAKERGB15(baseAddr[0],baseAddr[1],baseAddr[2]);

    pslog ("BlkFill %d,%d;%d,%d ,c=%x",
	prim.x,prim.y,prim.w,prim.h,c);

    set_clip(drawvram, 0, 0, 1023, 511);
    rectfill(drawvram, prim.x, prim.y, prim.w + prim.x - 1, prim.h + prim.y - 1, c);
    SetClip();*/
}

void primMoveImage(unsigned char * baseAddr) {
/*    short *gpuData = ((unsigned short *) baseAddr);
    short imageSX,imageSY,imageDX,imageDY,imageW,imageH;

    imageSX = gpuData[2] & 0x3ff;
    imageSY = gpuData[3] & 0x1ff;

    imageDX = gpuData[4] & 0x3ff;
    imageDY = gpuData[5] & 0x1ff;

    imageW = gpuData[6] & 0xffff;
    imageH = gpuData[7] & 0xffff;
    
    if((imageSY+imageH)>512) imageH=512-imageSY;
    if((imageDY+imageH)>512) imageH=512-imageDY;
    if((imageSX+imageW)>1024) imageW=1024-imageSX;
    if((imageDX+imageW)>1024) imageW=1024-imageDX;

    pslog("MoveImage %d,%d %d,%d %d,%d\n",
	imageSX, imageSY, imageDX, imageDY, imageW, imageH);

    UnsetClip();
    blit(vram, vram, imageSX, imageSY, imageDX, imageDY, imageW, imageH);
    SetClip();*/
}

void primLoadImage(unsigned char * baseAddr) {
	u32 gifTag[16];
    unsigned short *gpuData = ((unsigned short *) baseAddr);

    imageX0 = gpuData[2] & 0x3ff;
    imageY0 = gpuData[3] & 0x1ff;
    imageX1 = gpuData[4];
    imageY1 = gpuData[5];

    imTX=imageX0; imTY=imageY0;
    imTXc=imageX1; imTYc=imageY1;
    imageTransfer = 2;

#ifdef GPU_LOG
    GPU_LOG ("LoadImage %d,%d %d,%d\n", imageX0, imageY0, imageX0 + imageX1, imageY0 + imageY1);
#endif

	gifTag[0] = 3; // nloop = 3
	gifTag[1] = (1 << 28); // regs = 1
	gifTag[2] = 0xe; // ad reg

	gifTag[4] = 0;
	gifTag[5] = ((1024/64) << 16) | 0;
	gifTag[6] = 0x50; // bitbltbuf

	gifTag[8] = 0;
	gifTag[9] = (imageY0 << 16) | imageX0;
	gifTag[10] = 0x51; // trxpos

	gifTag[12] = imageX1;
	gifTag[13] = imageY1;
	gifTag[14] = 0x52; // trxreg

	GSgifTransfer3(gifTag, 4);
}

void primStoreImage(unsigned char * baseAddr) {
/*    unsigned long *gpuData = ((unsigned long *) baseAddr);

    imageX0 = (short)(gpuData[1] & 0x3ff);
    imageY0 = (short)(gpuData[1]>>16) & 0x1ff;
    imageX1 = (short)(gpuData[2] & 0xffff);
    imageY1 = (short)((gpuData[2]>>16) & 0xffff);

    imTX=imageX0; imTY=imageY0;
    imTXc=imageX1; imTYc=imageY1;
    imSize=imageY1*imageX1/2;

    imageTransfer = 3;
    GpuStatus|=0x08000000;

    pslog ("StoreImage %d,%d %d,%d\n"
	,imageX0, imageY0, imageX0 + imageX1, imageY0 + imageY1);*/
}

// Enviroment Primitives

void primTPage(unsigned char * baseAddr) {
/*    unsigned long gdata = ((unsigned long*)baseAddr)[0];

    TPage = (short)(gdata & 0x7ff);
    textAddrX = (gdata&0xf)<<6;
    textAddrY = (((gdata)<<4)&0x100)+(((gdata)>>2)&0x200);
    textTP = (gdata & 0x180) >> 7;
    textABR = (gdata >> 5) & 0x3;
    textREST = (gdata&0x00ffffff)>>9;

    GpuStatus = (GpuStatus & ~0x7ff) | (TPage);*/
}

void primTWindow(unsigned char *baseAddr) {
/*    unsigned long gdata = ((unsigned long*)baseAddr)[0];

    tWinW = gdata&0xf;
    tWinH = (gdata>>4)&0xf;
    tWinX = (gdata>>8)&0xf;
    tWinY = (gdata>>12)&0xf;

    pslog("primTWindow %d,%d %d,%d\n", tWinX, tWinY, tWinW, tWinH);*/
}

void primAreaStart(unsigned char * baseAddr) {
/*    unsigned int gdata = ((unsigned int *)baseAddr)[0];

    clipX = gdata & 0x3ff;
    clipY = (gdata>>10) & 0x1ff;
    SetClip();
    pslog("primAreaStart %dx%d\n", clipX, clipY);
    GpuInfo[3] = gdata & 0xffffff;*/
}

void primAreaEnd(unsigned char * baseAddr) {
/*    unsigned long gdata = ((unsigned long*)baseAddr)[0];

    clipW = (short)(gdata & 0x3ff);
    clipH = (short)((gdata>>10) & 0x1ff);
    SetClip();
    pslog("primAreaEnd %dx%d\n", clipW, clipH);
    GpuInfo[4] = gdata & 0xffffff;*/
}

void primDrawOffset(unsigned char * baseAddr) {
/*    unsigned long gdata = ((unsigned long*)baseAddr)[0];

    offsX = (short)(gdata & 0x3ff);
    offsY = (short)((gdata >> 11) & 0x1ff);
    pslog("primDrawOffset %dx%d\n", offsX, offsY);
    GpuInfo[5] = gdata & 0xffffff;*/
}

void primMask(unsigned char * baseAddr) {
/*    unsigned long gdata = ((unsigned long*)baseAddr)[0];

    md = gdata & 1;
    me = gdata & 2;
    GpuStatus = (GpuStatus & ~(3<<11)) | ((gdata&3)<<11);*/
}

// Drawing Helper Functions
/*
void drawSprite(SprtWH *prim, short w, short h) {
    Cache *sprt;
    unsigned int id;

    prim->x+=offsX;
    prim->y+=offsY;

    id = (prim->clut << 16) | TPage;
//    sprt = CacheGet(id, prim->u, prim->v, w + prim->u, h + prim->v);
    sprt = CacheGet(id, 0, 0, 256, 256);

    if ((prim->r >= 0x80) & (prim->g >= 0x80) & (prim->b >= 0x80) & (!(prim->code & 2))) {
	masked_blit(sprt->tex, drawvram, prim->u, prim->v, prim->x, prim->y, w, h);

        pslog ("Sprite : %d,%d - %dx%d (c=%x)- u,v: %d,%d (semi-transparency=%d) tex id=%x",
	    prim->x,prim->y,w,h,0x808080,prim->u,prim->v,(prim->code&2)!=0,id);
    }
    else {
	V3D_f v[4];
	unsigned int c;

//	c = MAKERGB24(prim->r*2,prim->g*2,prim->b*2);
	c = (prim->r*2 + prim->g*2 + prim->b*2) / 3; if (c > 0xff) c = 0xff;

        v[0].x = prim->x; v[0].y = prim->y; v[0].z = 0;
	v[0].u = prim->u; v[0].v = prim->v; v[0].c = c;
        v[1].x = prim->x; v[1].y = prim->y + h; v[1].z = 0;
        v[1].u = prim->u; v[1].v = prim->v + h; v[1].c = c;
        v[2].x = prim->x + w; v[2].y = prim->y; v[2].z = 0;
        v[2].u = prim->u + w; v[2].v = prim->v; v[2].c = c;
        v[3].x = prim->x + w; v[3].y = prim->y + h; v[3].z = 0;
        v[3].u = prim->u + w; v[3].v = prim->v + h; v[3].c = c;

	if (prim->code & 2) {
	    SetST((id&0x6)>>5);
	    quad3d_f(drawvram, POLYTYPE_ATEX_MASK_TRANS, sprt->tex, &v[0], &v[1], &v[3], &v[2]);
	}
        else quad3d_f(drawvram, POLYTYPE_ATEX_MASK_LIT, sprt->tex, &v[0], &v[1], &v[3], &v[2]);

        pslog ("Sprite : %d,%d - %dx%d (c=%x)- u,v: %d,%d (semi-transparency=%d) tex id=%x",
	    prim->x,prim->y,w,h,c,prim->u,prim->v,(prim->code&2)!=0,id);
    }

}

void drawRect(RectWH *prim, short w, short h) {
    unsigned int c;

    prim->x+=offsX;
    prim->y+=offsY;

    c = MAKERGB15(prim->r,prim->g,prim->b);

    if (prim->code & 2) {
	SetST(textABR);
	drawing_mode(DRAW_MODE_TRANS, NULL, 0, 0);
    }
    rectfill(drawvram, prim->x, prim->y, w + prim->x - 1, h + prim->y - 1, c);
    if (prim->code & 2) drawing_mode(DRAW_MODE_SOLID, NULL, 0, 0);

    pslog ("Rectangle %d,%d %dx%d (semi-transparency=%d) c=%x",
	prim->x,prim->y,w,h,(prim->code & 2)!=0,c);
}*/

//

void primNI(unsigned char *bA) {
//    if (bA[3]) pslog("PRIM: *NI* %02x",bA[3]);
}


unsigned char primTableC[256] = {
    1,1,3,0,0,0,0,0,		// 00
    0,0,0,0,0,0,0,0,		// 08
    0,0,0,0,0,0,0,0,		// 10
    0,0,0,0,0,0,0,0,		// 18
    4,4,4,4,7,7,7,7,		// 20
    5,5,5,5,9,9,9,9,		// 28
    6,6,6,6,9,9,9,9,		// 30
    8,8,8,8,12,12,12,12,	// 38
    3,3,3,3,0,0,0,0,		// 40
    5,5,5,5,6,6,6,6,		// 48
    4,4,4,4,0,0,0,0,		// 50
    7,7,7,7,9,9,9,9,		// 58
    3,3,3,3,4,4,4,4,		// 60
    2,2,2,2,0,0,0,0,		// 68
    2,2,2,2,3,3,3,3,		// 70
    2,2,2,2,3,3,3,3,		// 78
    4,0,0,0,0,0,0,0,		// 80
    0,0,0,0,0,0,0,0,		// 88
    0,0,0,0,0,0,0,0,		// 90
    0,0,0,0,0,0,0,0,		// 98
    3,0,0,0,0,0,0,0,		// a0
    0,0,0,0,0,0,0,0,		// a8
    0,0,0,0,0,0,0,0,		// b0
    0,0,0,0,0,0,0,0,		// b8
    3,0,0,0,0,0,0,0,		// c0
    0,0,0,0,0,0,0,0,		// c8
    0,0,0,0,0,0,0,0,		// d0
    0,0,0,0,0,0,0,0,		// d8
    0,1,1,1,1,1,1,0,		// e0
    0,0,0,0,0,0,0,0,		// e8
    0,0,0,0,0,0,0,0,		// f0
    0,0,0,0,0,0,0,0		// f8
};

void (*primTableJ[256])(unsigned char *) = {
    // 00
    primVoid,primClrCache,primBlkFill,primNI,primNI,primNI,primNI,primNI,
    // 08
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // 10
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // 18
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // 20
    primPolyF3,primPolyF3,primPolyF3,primPolyF3,primPolyFT3,primPolyFT3,primPolyFT3,primPolyFT3,
    // 28
    primPolyF4,primPolyF4,primPolyF4,primPolyF4,primPolyFT4,primPolyFT4,primPolyFT4,primPolyFT4,
    // 30
    primPolyG3,primPolyG3,primPolyG3,primPolyG3,primPolyGT3,primPolyGT3,primPolyGT3,primPolyGT3,
    // 38
    primPolyG4,primPolyG4,primPolyG4,primPolyG4,primPolyGT4,primPolyGT4,primPolyGT4,primPolyGT4,
    // 40
    primLineF2,primLineF2,primLineF2,primLineF2,primNI,primNI,primNI,primNI,
    // 48
    primLineF,primLineF,primLineF,primLineF,primNI,primNI,primNI,primNI,
    // 50
    primLineG2,primLineG2,primLineG2,primLineG2,primNI,primNI,primNI,primNI,
    // 58
    primLineG,primLineG,primLineG,primLineG,primNI,primNI,primNI,primNI,
    // 60
    primTileS,primTileS,primTileS,primTileS,primSprtS,primSprtS,primSprtS,primSprtS,
    // 68
    primTile1,primTile1,primTile1,primTile1,primNI,primNI,primNI,primNI,
    // 70
    primTile8,primTile8,primTile8,primTile8,primSprt8,primSprt8,primSprt8,primSprt8,
    // 78
    primTile16,primTile16,primTile16,primTile16,primSprt16,primSprt16,primSprt16,primSprt16,
    // 80
    primMoveImage,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // 88
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // 90
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // 98
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // a0
    primLoadImage,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // a8
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // b0
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // b8
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // c0
    primStoreImage,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // c8
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // d0
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // d8
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // e0
    primNI,primTPage,primTWindow,primAreaStart,primAreaEnd,primDrawOffset,primMask,primNI,
    // e8
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // f0
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI,
    // f8
    primNI,primNI,primNI,primNI,primNI,primNI,primNI,primNI
};

u32 cd[4];
int cdC = 0;

__inline void writePixel(short x, short y, u16 p) {
	u32 gifTag[16];

	cd[cdC] = ((p & 0x7c00) << 9) |
  		      ((p & 0x03e0) << 6) |
		      ((p & 0x001f) << 3);
	cdC++;
	if (cdC < 4) return;
	cdC = 0;

	gifTag[0] = 1;
	gifTag[1] = 2 << 26;

	GSgifTransfer3(gifTag, 1);
	GSgifTransfer3(cd, 1);
}

void GPU_writeData(u32 data) {
	gpuDataRet = data;

	if (imageTransfer == 2) {
	// image transfer to VRAM
		if (imTYc>0 && imTXc>0) {
		    if ((imTY>=0) && (imTY<512) && (imTX>=0) && (imTX<1024)) {
				writePixel(imTX, imTY, (u16)data);
		    }

		    imTX++; imTXc--;
		    if (imTXc<=0) {
	        	imTX = imageX0; imTXc = imageX1;
	        	imTYc--; imTY++;
	    	}
	    	if (imTYc <= 0) { imageTransfer = 0; return; }
     
		    if ((imTY>=0) && (imTY<512) && (imTX>=0) && (imTX<1024)) {
				writePixel(imTX, imTY, (u16)(data>>16));
		    }

		    imTX++; imTXc--;
		    if (imTXc<=0) {
	        	imTX = imageX0; imTXc = imageX1;
	        	imTYc--; imTY++;
	    	}
	    	if (imTYc <= 0) { imageTransfer = 0; return; }
 
		    return;
		}
        else imageTransfer = 0;
	}

#ifdef GPU_LOG
    GPU_LOG("!GPU! DATA = %08x\n", data);
#endif

	if (gpuDataC == 0) {
		gpuCmd = (u8)(data >> 24);
		gpuDataC = primTableC[gpuCmd];
		if (gpuDataC) {
			gpuData[0] = data;
			gpuDataP = 1;
		} else {
#ifdef GPU_LOG
			GPU_LOG("GPU ERROR CMD: %x, data=%x\n", gpuCmd, data);
#endif
			return;
		}
	} else {
		if (gpuDataP >= 256) return;
		gpuData[gpuDataP++] = data;
	}

	if (gpuDataP == gpuDataC) {
		gpuDataP = gpuDataC = 0;
#ifdef GPU_LOG
		GPU_LOG("Gpu(writeData) Command %x\n", gpuCmd);
#endif
		primTableJ[gpuCmd]((u8*)gpuData);
	}
}

void GPU_writeStatus(u32 data) {
//	u32 gifTag[16];

	switch (data >> 24) {
		case 0x00:
/*			gifTag[0] = 2; // nloop = 2
			gifTag[1] = (1 << 28); // regs = 1
			gifTag[2] = 0xe; // ad reg

			gifTag[4] = 0;
			gifTag[5] = ((1024/64) << 16) | 0;
			gifTag[6] = 0x50; // bitbltbuf

			gifTag[8] = 320;
			gifTag[9] = 240;
			gifTag[10] = 0x52; // trxreg

			GSgifTransfer3(gifTag, 3);*/
			GSwrite64(0x12000070, (1024/64) << 9);
			break;

		case 0x10:
			switch (data&7) {
				case 3:
				case 4:
				case 5:
				case 7:
					gpuDataRet = gpuInfo[data&7];
					break;
				default:
#ifdef GPU_LOG
					GPU_LOG("GPU INFO UNK: %x\n", data);
#endif
					break;
			}
			break;
	}
}

u32  GPU_readData() {
	return gpuDataRet;
}

u32  GPU_readStatus() {
	gpuStatus^= 0x80000000;
	return gpuStatus;
}

void GPU_writeDataMem(u32 *pMem, int iSize) {
/*	u32 gifTag[16];
	int i;
	u16 *ptr = (u16*)pMem;*/

#ifdef GPU_LOG
	GPU_LOG("GPUwriteDataMem: %d\n", iSize);
#endif

	while (iSize) {
		GPU_writeData(*pMem);
		pMem++; iSize--;
	}

/*#ifdef GPU_LOG
	GPU_LOG("GPUwriteDataMem: %d\n", iSize);
#endif

	if (iSize > 0x7fff) {
	} else {
		gifTag[0] = iSize & 0x7fff;
		gifTag[1] = 2 << 26;

		GSgifTransfer3(gifTag, 1);
//		GSgifTransfer3(pMem, iSize);
		GSgifTransfer3(ptr1, iSize);
	}
	imageTransfer = 0;*/
}

void GPU_readDataMem(u32 *pMem, int iSize) {
}

void GPU_dmaChain(u32 *baseAddrL, u32 addr) {
    unsigned long dmaMem;
    unsigned char * baseAddrB;
    unsigned char count;
    unsigned char cmd;

    baseAddrB = (unsigned char*) baseAddrL;

    for (;;) {
		count = baseAddrB[addr+3];
		dmaMem = addr+4;
		while (count) {
	 		if (imageTransfer == 2) {
				GPU_writeData(baseAddrL[dmaMem/4]);
				dmaMem+=4;
				count--;
				continue;
	    	}
	    	cmd = baseAddrB[dmaMem+3];
	    	if (primTableC[cmd] == 0) {
				dmaMem+=4;
				count--;
	    	} else {
#ifdef GPU_LOG
				GPU_LOG("Gpu(DmaChain) Command %x : %x\n", cmd, baseAddrL[dmaMem/4]);
#endif
				primTableJ[cmd]((u8*)baseAddrL + dmaMem);
				dmaMem+=(primTableC[cmd]*4);
				count-=primTableC[cmd];
	    	}
		}
		addr = baseAddrL[addr/4]&0xffffff;
		if (addr <= 0 || addr == 0xffffff) break;
		addr&=0x7fffff;
    }
}
