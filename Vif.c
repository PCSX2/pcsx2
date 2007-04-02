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

#include <math.h>
#include <string.h>

#include "Common.h"
#include "ix86/ix86.h"
#include "Vif.h"
#include "VUmicro.h"

#include <assert.h>

VIFregisters *_vifRegs;
u32* _vifMaskRegs = NULL;
PCSX2_ALIGNED16(u32 g_vifRow0[4]);
PCSX2_ALIGNED16(u32 g_vifCol0[4]);
PCSX2_ALIGNED16(u32 g_vifRow1[4]);
PCSX2_ALIGNED16(u32 g_vifCol1[4]);
u32* _vifRow = NULL;

vifStruct *_vif;

static int n;
static int i;

__inline static int _limit( int a, int max ) 
{
	return ( a > max ? max : a );
}

#define _UNPACKpart( offnum, func )                         \
	if ( ( size > 0 ) && ( _vifRegs->offset == offnum ) ) { \
		func;                                               \
		size--;                                             \
		_vifRegs->offset++;                                  \
	}

#define _UNPACKpart_nosize( offnum, func )                \
	if ( ( _vifRegs->offset == offnum ) ) { \
		func;                                               \
		_vifRegs->offset++;                                  \
	}

static void _writeX( u32 *dest, u32 data )
{
	//int n;

	switch ( _vif->cl ) {
		case 0:  n =  0; break;
		case 1:  n =  8; break;
		case 2:  n = 16; break;
		default: n = 24; break;
	}
/*#ifdef VIF_LOG
	VIF_LOG("_writeX %x = %x (writecycle=%d; mask %x; mode %d)\n", (u32)dest-(u32)VU1.Mem, data, _vif->cl, ( _vifRegs->mask >> n ) & 0x3,_vifRegs->mode);
#endif*/
	switch ( ( _vifRegs->mask >> n ) & 0x3 ) {
		case 0:
			if((_vif->cmd & 0x6F) == 0x6f) {
				//SysPrintf("Phew X!\n");
				*dest = data;
				break;
			}
			if (_vifRegs->mode == 1) {
				*dest = data + _vifRegs->r0;
			} else 
			if (_vifRegs->mode == 2) {
				_vifRegs->r0 = data + _vifRegs->r0;
				*dest = _vifRegs->r0;
			} else {
				*dest = data;
			}
			break;
		case 1: *dest = _vifRegs->r0; break;
		case 2: 
			switch ( _vif->cl ) {
				case 0:  *dest = _vifRegs->c0; break;
				case 1:  *dest = _vifRegs->c1; break;
				case 2:  *dest = _vifRegs->c2; break;
				default: *dest = _vifRegs->c3; break;
			}
			break;
	}

/*#ifdef VIF_LOG
	VIF_LOG("_writeX-done : Data %x : Row %x\n", *dest, _vifRegs->r0);
#endif*/
}

static void _writeY( u32 *dest, u32 data )
{
	//int n;
	switch ( _vif->cl ) {
		case 0:  n =  2; break;
		case 1:  n = 10; break;
		case 2:  n = 18; break;
		default: n = 26; break;
	}
/*#ifdef VIF_LOG
	VIF_LOG("_writeY %x = %x (writecycle=%d; mask %x; mode %d)\n", (u32)dest-(u32)VU1.Mem, data, _vif->cl, ( _vifRegs->mask >> n ) & 0x3,_vifRegs->mode);
#endif*/
	switch ( ( _vifRegs->mask >> n ) & 0x3 ) {
		case 0:
			if((_vif->cmd & 0x6F) == 0x6f) {
				//SysPrintf("Phew Y!\n");
				*dest = data;
				break;
			}
			if (_vifRegs->mode == 1) {
				*dest = data + _vifRegs->r1;
			} else 
			if (_vifRegs->mode == 2) {
				_vifRegs->r1 = data + _vifRegs->r1;
				*dest = _vifRegs->r1;
			} else {
				*dest = data;
			}
			break;
		case 1: *dest = _vifRegs->r1; break;
		case 2: 
			switch ( _vif->cl )
         {
				case 0:  *dest = _vifRegs->c0; break;
				case 1:  *dest = _vifRegs->c1; break;
				case 2:  *dest = _vifRegs->c2; break;
				default: *dest = _vifRegs->c3; break;
			}
			break;
	}

/*#ifdef VIF_LOG
	VIF_LOG("_writeY-done : Data %x : Row %x\n", *dest, _vifRegs->r1);
#endif*/
}

static void _writeZ( u32 *dest, u32 data )
{
	//int n;
	switch ( _vif->cl ) {
		case 0:  n =  4; break;
		case 1:  n = 12; break;
		case 2:  n = 20; break;
		default: n = 28; break;
	}
/*#ifdef VIF_LOG
	VIF_LOG("_writeZ %x = %x (writecycle=%d; mask %x; mode %d)\n", (u32)dest-(u32)VU1.Mem, data, _vif->cl, ( _vifRegs->mask >> n ) & 0x3,_vifRegs->mode);
#endif*/
	switch ( ( _vifRegs->mask >> n ) & 0x3 ) {
		case 0:
			if((_vif->cmd & 0x6F) == 0x6f) {
				//SysPrintf("Phew Z!\n");
				*dest = data;
				break;
			}
			if (_vifRegs->mode == 1) {
				*dest = data + _vifRegs->r2;
			} else 
			if (_vifRegs->mode == 2) {
				_vifRegs->r2 = data + _vifRegs->r2;
				*dest = _vifRegs->r2;
			} else {
				*dest = data;
			}
			break;
		case 1: *dest = _vifRegs->r2; break;
		case 2: 
			switch ( _vif->cl )
         {
				case 0:  *dest = _vifRegs->c0; break;
				case 1:  *dest = _vifRegs->c1; break;
				case 2:  *dest = _vifRegs->c2; break;
				default: *dest = _vifRegs->c3; break;
			}
			break;
	}
}

static void _writeW( u32 *dest, u32 data )
{
	//int n;
	switch ( _vif->cl ) {
		case 0:  n =  6; break;
		case 1:  n = 14; break;
		case 2:  n = 22; break;
		default: n = 30; break;
	}
/*#ifdef VIF_LOG
	VIF_LOG("_writeW %x = %x (writecycle=%d; mask %x; mode %d)\n", (u32)dest-(u32)VU1.Mem, data, _vif->cl, ( _vifRegs->mask >> n ) & 0x3,_vifRegs->mode);
#endif*/
	switch ( ( _vifRegs->mask >> n ) & 0x3 ) {
		case 0:
			if((_vif->cmd & 0x6F) == 0x6f) {
				//SysPrintf("Phew W!\n");
				*dest = data;
				break;
			}
			if (_vifRegs->mode == 1) {
				*dest = data + _vifRegs->r3;
			} else 
			if (_vifRegs->mode == 2) {
				_vifRegs->r3 = data + _vifRegs->r3;
				*dest = _vifRegs->r3;
			} else {
				*dest = data;
			}
			break;
		case 1: *dest = _vifRegs->r3; break;
		case 2: 
			switch ( _vif->cl ) {
				case 0:  *dest = _vifRegs->c0; break;
				case 1:  *dest = _vifRegs->c1; break;
				case 2:  *dest = _vifRegs->c2; break;
				default: *dest = _vifRegs->c3; break;
			}
			break;
	}
}

static void writeX( u32 *dest, u32 data ) {
	if (_vifRegs->code & 0x10000000) { _writeX(dest, data); return; }
	if((_vif->cmd & 0x6F) == 0x6f) {
		//SysPrintf("Phew X!\n");
		*dest = data;
		return;
	}
	if (_vifRegs->mode == 1) {
		*dest = data + _vifRegs->r0;
	} else 
	if (_vifRegs->mode == 2) {
		_vifRegs->r0 = data + _vifRegs->r0;
		*dest = _vifRegs->r0;
	} else {
		*dest = data;
	}
/*#ifdef VIF_LOG
	VIF_LOG("writeX %8.8x : Mode %d, r0 = %x, data %8.8x\n", *dest,_vifRegs->mode,_vifRegs->r0,data);
#endif*/
}

static void writeY( u32 *dest, u32 data ) {
	if (_vifRegs->code & 0x10000000) { _writeY(dest, data); return; }
	if((_vif->cmd & 0x6F) == 0x6f) {
		//SysPrintf("Phew Y!\n");
		*dest = data;
		return;
	}
	if (_vifRegs->mode == 1) {
		*dest = data + _vifRegs->r1;
	} else 
	if (_vifRegs->mode == 2) {
		_vifRegs->r1 = data + _vifRegs->r1;
		*dest = _vifRegs->r1;
	} else {
		*dest = data;
	}
/*#ifdef VIF_LOG
	VIF_LOG("writeY %8.8x : Mode %d, r1 = %x, data %8.8x\n", *dest,_vifRegs->mode,_vifRegs->r1,data);
#endif*/
}

static void writeZ( u32 *dest, u32 data ) {
	if (_vifRegs->code & 0x10000000) { _writeZ(dest, data); return; }
	if((_vif->cmd & 0x6F) == 0x6f) {
		//SysPrintf("Phew Z!\n");
		*dest = data;
		return;
	}
	if (_vifRegs->mode == 1) {
		*dest = data + _vifRegs->r2;
	} else 
	if (_vifRegs->mode == 2) {
		_vifRegs->r2 = data + _vifRegs->r2;
		*dest = _vifRegs->r2;
	} else {
		*dest = data;
	}
/*#ifdef VIF_LOG
	VIF_LOG("writeZ %8.8x : Mode %d, r2 = %x, data %8.8x\n", *dest,_vifRegs->mode,_vifRegs->r2,data);
#endif*/
}

static void writeW( u32 *dest, u32 data ) {
	if (_vifRegs->code & 0x10000000) { _writeW(dest, data); return; }
	if((_vif->cmd & 0x6F) == 0x6f) {
		//SysPrintf("Phew X!\n");
		*dest = data;
		return;
	}
	if (_vifRegs->mode == 1) {
		*dest = data + _vifRegs->r3;
	} else 
	if (_vifRegs->mode == 2) {
		_vifRegs->r3 = data + _vifRegs->r3;
		*dest = _vifRegs->r3;
	} else {
		*dest = data;
	}
/*#ifdef VIF_LOG
	VIF_LOG("writeW %8.8x : Mode %d, r3 = %x, data %8.8x\n", *dest,_vifRegs->mode,_vifRegs->r3,data);
#endif*/
}

void UNPACK_S_32(u32 *dest, u32 *data) {
		writeX(dest++, *data);
		writeY(dest++, *data);
		writeZ(dest++, *data);
		writeW(dest++, *data++);
}

int  UNPACK_S_32part(u32 *dest, u32 *data, int size) {
	u32 *_data = data;
	while (size > 0) {
		_UNPACKpart(0, writeX(dest++, *data) );
		_UNPACKpart(1, writeY(dest++, *data) );
		_UNPACKpart(2, writeZ(dest++, *data) );
		_UNPACKpart(3, writeW(dest++, *data++) );
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;
	}

	return (u32)data - (u32)_data;
}

#define _UNPACK_S_16(format) \
	format *sdata = (format*)data; \
	 \
 \
		writeX(dest++, *sdata); \
		writeY(dest++, *sdata); \
		writeZ(dest++, *sdata); \
		writeW(dest++, *sdata++);

void UNPACK_S_16s( u32 *dest, u32 *data ) {
	_UNPACK_S_16( s16 );
}

void UNPACK_S_16u( u32 *dest, u32 *data ) {
	_UNPACK_S_16( u16 );
}

#define _UNPACK_S_16part(format) \
	format *sdata = (format*)data; \
	while (size > 0) { \
		_UNPACKpart(0, writeX(dest++, *sdata) ); \
		_UNPACKpart(1, writeY(dest++, *sdata) ); \
		_UNPACKpart(2, writeZ(dest++, *sdata) ); \
		_UNPACKpart(3, writeW(dest++, *sdata++) ); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0; \
	} \
	return (u32)sdata - (u32)data;

int  UNPACK_S_16spart(u32 *dest, u32 *data, int size) {
	_UNPACK_S_16part(s16);
}

int  UNPACK_S_16upart(u32 *dest, u32 *data, int size) {
	_UNPACK_S_16part(u16);
}

#define _UNPACK_S_8(format) \
	format *cdata = (format*)data; \
	 \
 \
		writeX(dest++, *cdata); \
		writeY(dest++, *cdata); \
		writeZ(dest++, *cdata); \
		writeW(dest++, *cdata++);

void UNPACK_S_8s(u32 *dest, u32 *data) {
	_UNPACK_S_8(s8);
}

void UNPACK_S_8u(u32 *dest, u32 *data) {
	_UNPACK_S_8(u8);
}

#define _UNPACK_S_8part(format) \
	format *cdata = (format*)data; \
	while (size > 0) { \
		_UNPACKpart(0, writeX(dest++, *cdata) ); \
		_UNPACKpart(1, writeY(dest++, *cdata) ); \
		_UNPACKpart(2, writeZ(dest++, *cdata) ); \
		_UNPACKpart(3, writeW(dest++, *cdata++) ); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0; \
	} \
	return (u32)cdata - (u32)data;

int  UNPACK_S_8spart(u32 *dest, u32 *data, int size) {
	_UNPACK_S_8part(s8);
}

int  UNPACK_S_8upart(u32 *dest, u32 *data, int size) {
	_UNPACK_S_8part(u8);
}

void UNPACK_V2_32( u32 *dest, u32 *data ) {
		writeX(dest++, *data++);
		writeY(dest++, *data);
		writeZ(dest++, *data);
		writeW(dest++, *data++);
	
}

int  UNPACK_V2_32part( u32 *dest, u32 *data, int size ) {
	u32 *_data = data;
	while (size > 0) { 
		_UNPACKpart(0, writeX(dest++, *data++));
		_UNPACKpart(1, writeY(dest++, *data++));
		_UNPACKpart_nosize(2, writeZ(dest++, 0));
		_UNPACKpart_nosize(3, writeW(dest++, 0));
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;
	}
	return (u32)data - (u32)_data;
}

#define _UNPACK_V2_16(format) \
	format *sdata = (format*)data; \
	 \
 \
		writeX(dest++, *sdata++); \
		writeY(dest++, *sdata); \
		writeZ(dest++, *sdata); \
		writeW(dest++, *sdata++); \
	

void UNPACK_V2_16s(u32 *dest, u32 *data) {
	_UNPACK_V2_16(s16);
}

void UNPACK_V2_16u(u32 *dest, u32 *data) {
	_UNPACK_V2_16(u16);
}

#define _UNPACK_V2_16part(format) \
	format *sdata = (format*)data; \
	\
	 while(size > 0) {	\
		_UNPACKpart(0, writeX(dest++, *sdata++)); \
		_UNPACKpart(1, writeY(dest++, *sdata++)); \
		_UNPACKpart_nosize(2,writeZ(dest++, 0)); \
		_UNPACKpart_nosize(3,writeW(dest++, 0)); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;	\
	 }	\
	return (u32)sdata - (u32)data;

int  UNPACK_V2_16spart(u32 *dest, u32 *data, int size) {
	_UNPACK_V2_16part(s16);
}

int  UNPACK_V2_16upart(u32 *dest, u32 *data, int size) {
	_UNPACK_V2_16part(u16);
}

#define _UNPACK_V2_8(format) \
	format *cdata = (format*)data; \
	 \
 \
		writeX(dest++, *cdata++); \
		writeY(dest++, *cdata); \
		writeZ(dest++, *cdata); \
		writeW(dest++, *cdata++); 

void UNPACK_V2_8s(u32 *dest, u32 *data) {
	_UNPACK_V2_8(s8);
}

void UNPACK_V2_8u(u32 *dest, u32 *data) {
	_UNPACK_V2_8(u8);
}

#define _UNPACK_V2_8part(format) \
	format *cdata = (format*)data; \
	 while(size > 0) {	\
		_UNPACKpart(0, writeX(dest++, *cdata++)); \
		_UNPACKpart(1, writeY(dest++, *cdata++)); \
		_UNPACKpart_nosize(2,writeZ(dest++, 0)); \
		_UNPACKpart_nosize(3,writeW(dest++, 0)); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;	\
	 }	\
	return (u32)cdata - (u32)data;

int  UNPACK_V2_8spart(u32 *dest, u32 *data, int size) {
	_UNPACK_V2_8part(s8);
}

int  UNPACK_V2_8upart(u32 *dest, u32 *data, int size) {
	_UNPACK_V2_8part(u8);
}

void UNPACK_V3_32(u32 *dest, u32 *data) {
		writeX(dest++, *data++);
		writeY(dest++, *data++);
		writeZ(dest++, *data);
		writeW(dest++, *data++);
}

int  UNPACK_V3_32part(u32 *dest, u32 *data, int size) {
	u32 *_data = data;
	while (size > 0) {
		_UNPACKpart(0, writeX(dest++, *data++); );
		_UNPACKpart(1, writeY(dest++, *data++); );
		_UNPACKpart(2, writeZ(dest++, *data++); );
		_UNPACKpart_nosize(3, writeW(dest++, 0); );
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;
	}
	return (u32)data - (u32)_data;
}

#define _UNPACK_V3_16(format) \
	format *sdata = (format*)data; \
	 \
 \
		writeX(dest++, *sdata++); \
		writeY(dest++, *sdata++); \
		writeZ(dest++, *sdata); \
		writeW(dest++, *sdata++); 

void UNPACK_V3_16s(u32 *dest, u32 *data) {
	_UNPACK_V3_16(s16);
}

void UNPACK_V3_16u(u32 *dest, u32 *data) {
	_UNPACK_V3_16(u16);
}

#define _UNPACK_V3_16part(format) \
	format *sdata = (format*)data; \
	 \
	 while(size > 0) {	\
		_UNPACKpart(0, writeX(dest++, *sdata++)); \
		_UNPACKpart(1, writeY(dest++, *sdata++)); \
		_UNPACKpart(2, writeZ(dest++, *sdata++)); \
		_UNPACKpart_nosize(3,writeW(dest++, 0)); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;	\
	 }	\
	return (u32)sdata - (u32)data;

int  UNPACK_V3_16spart(u32 *dest, u32 *data, int size) {
	_UNPACK_V3_16part(s16);
}

int  UNPACK_V3_16upart(u32 *dest, u32 *data, int size) {
	_UNPACK_V3_16part(u16);
}

#define _UNPACK_V3_8(format) \
	format *cdata = (format*)data; \
	 \
 \
		writeX(dest++, *cdata++); \
		writeY(dest++, *cdata++); \
		writeZ(dest++, *cdata); \
		writeW(dest++, *cdata++); 

void UNPACK_V3_8s(u32 *dest, u32 *data) {
	_UNPACK_V3_8(s8);
}

void UNPACK_V3_8u(u32 *dest, u32 *data) {
	_UNPACK_V3_8(u8);
}

#define _UNPACK_V3_8part(format) \
	format *cdata = (format*)data; \
	 while(size > 0) {	\
		_UNPACKpart(0, writeX(dest++, *cdata++)); \
		_UNPACKpart(1, writeY(dest++, *cdata++)); \
		_UNPACKpart(2, writeZ(dest++, *cdata++)); \
		_UNPACKpart_nosize(3,writeW(dest++, 0)); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;	\
	 }	\
	return (u32)cdata - (u32)data;

int  UNPACK_V3_8spart(u32 *dest, u32 *data, int size) {
	_UNPACK_V3_8part(s8);
}

int  UNPACK_V3_8upart(u32 *dest, u32 *data, int size) {
	_UNPACK_V3_8part(u8);
}

void UNPACK_V4_32( u32 *dest, u32 *data ) {
		writeX(dest++, *data++);
		writeY(dest++, *data++);
		writeZ(dest++, *data++);
		writeW(dest++, *data++);
}

int  UNPACK_V4_32part(u32 *dest, u32 *data, int size) {
	u32 *_data = data;
	while (size > 0) {
		_UNPACKpart(0, writeX(dest++, *data++) );
		_UNPACKpart(1, writeY(dest++, *data++) );
		_UNPACKpart(2, writeZ(dest++, *data++) );
		_UNPACKpart(3, writeW(dest++, *data++) );
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;
	}
	return (u32)data - (u32)_data;
}

#define _UNPACK_V4_16(format) \
	format *sdata = (format*)data; \
	 \
  \
		writeX(dest++, *sdata++); \
		writeY(dest++, *sdata++); \
		writeZ(dest++, *sdata++); \
		writeW(dest++, *sdata++);

void UNPACK_V4_16s(u32 *dest, u32 *data) {
	_UNPACK_V4_16(s16);
}

void UNPACK_V4_16u(u32 *dest, u32 *data) {
	_UNPACK_V4_16(u16);
}

#define _UNPACK_V4_16part(format) \
	format *sdata = (format*)data; \
	while (size > 0) { \
		_UNPACKpart(0, writeX(dest++, *sdata++) ); \
		_UNPACKpart(1, writeY(dest++, *sdata++) ); \
		_UNPACKpart(2, writeZ(dest++, *sdata++) ); \
		_UNPACKpart(3, writeW(dest++, *sdata++) ); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0; \
	} \
	return (u32)sdata - (u32)data;

int  UNPACK_V4_16spart(u32 *dest, u32 *data, int size) {
	_UNPACK_V4_16part(s16);
}

int  UNPACK_V4_16upart(u32 *dest, u32 *data, int size) {
	_UNPACK_V4_16part(u16);
}

#define _UNPACK_V4_8(format) \
	format *cdata = (format*)data; \
	 \
 \
		writeX(dest++, *cdata++); \
		writeY(dest++, *cdata++); \
		writeZ(dest++, *cdata++); \
		writeW(dest++, *cdata++);

void UNPACK_V4_8s(u32 *dest, u32 *data) {
	_UNPACK_V4_8(s8);
}

void UNPACK_V4_8u(u32 *dest, u32 *data) {
	_UNPACK_V4_8(u8);
}

#define _UNPACK_V4_8part(format) \
	format *cdata = (format*)data; \
	while (size > 0) { \
		_UNPACKpart(0, writeX(dest++, *cdata++) ); \
		_UNPACKpart(1, writeY(dest++, *cdata++) ); \
		_UNPACKpart(2, writeZ(dest++, *cdata++) ); \
		_UNPACKpart(3, writeW(dest++, *cdata++) ); \
		if (_vifRegs->offset == 4) _vifRegs->offset = 0; \
	} \
	return (u32)cdata - (u32)data;

int  UNPACK_V4_8spart(u32 *dest, u32 *data, int size) {
	_UNPACK_V4_8part(s8);
}

int  UNPACK_V4_8upart(u32 *dest, u32 *data, int size) {
	_UNPACK_V4_8part(u8);
}

void UNPACK_V4_5(u32 *dest, u32 *data) {
	u16 *sdata = (u16*)data;
	u32 rgba;

	rgba = *sdata++;
	writeX(dest++, (rgba & 0x001f) << 3);
	writeY(dest++, (rgba & 0x03e0) >> 2);
	writeZ(dest++, (rgba & 0x7c00) >> 7);
	writeW(dest++, (rgba & 0x8000) >> 8);
}

int  UNPACK_V4_5part(u32 *dest, u32 *data, int size) {
	u16 *sdata = (u16*)data;
	u32 rgba;

	while (size > 0) {
		rgba = *sdata++;
		_UNPACKpart(0, writeX(dest++, (rgba & 0x001f) << 3); );
		_UNPACKpart(1, writeY(dest++, (rgba & 0x03e0) >> 2); );
		_UNPACKpart(2, writeZ(dest++, (rgba & 0x7c00) >> 7); );
		_UNPACKpart(3, writeW(dest++, (rgba & 0x8000) >> 8); );
		if (_vifRegs->offset == 4) _vifRegs->offset = 0;
	}

	return (u32)sdata - (u32)data;
}

static int cycles;
extern int g_vifCycles;
static int vifqwc = 0;
static int mfifoVIF1rbTransfer() {
	u32 maddr = psHu32(DMAC_RBOR);
	int msize = psHu32(DMAC_RBSR)+16, ret;
	u32 *src;

	/* Check if the transfer should wrap around the ring buffer */
	if ((vif1ch->madr+(vif1ch->qwc << 4)) >= (maddr+msize)) {
		int s1 = (maddr+msize) - vif1ch->madr;
		int s2 = (vif1ch->qwc << 4) - s1;

		/* it does, so first copy 's1' bytes from 'addr' to 'data' */
		src = (u32*)PSM(vif1ch->madr);
		if (src == NULL) return -1;
		if(vif1.vifstalled == 1)
			ret = VIF1transfer(src+vif1.irqoffset, s1/4-vif1.irqoffset, 0);
		else
			ret = VIF1transfer(src, s1>>2, 0); 
		assert(ret == 0 ); // vif stall code not implemented
		if(ret == -2) return ret;
		if(vif1ch->madr > (maddr+msize)) SysPrintf("Copied too much VIF! %x\n", vif1ch->madr);
		/* and second copy 's2' bytes from 'maddr' to '&data[s1]' */
		vif1ch->madr = maddr;
		src = (u32*)PSM(maddr);
		if (src == NULL) return -1;
		ret = VIF1transfer(src, s2>>2, 0); 
		assert(ret == 0 ); // vif stall code not implemented
	} else {
		/* it doesn't, so just transfer 'qwc*4' words 
		   from 'vif1ch->madr' to VIF1 */
		src = (u32*)PSM(vif1ch->madr);
		if (src == NULL) return -1;
		if(vif1.vifstalled == 1)
			ret = VIF1transfer(src+vif1.irqoffset, vif1ch->qwc*4-vif1.irqoffset, 0);
		else
			ret = VIF1transfer(src, vif1ch->qwc << 2, 0); 
		if(ret == -2) return ret;
		vif1ch->madr = psHu32(DMAC_RBOR) + (vif1ch->madr & psHu32(DMAC_RBSR));
		assert(ret == 0 ); // vif stall code not implemented
	}

	//vif1ch->madr+= (vif1ch->qwc << 4);
	

	return ret;
}

static int mfifoVIF1chain() {
	u32 maddr = psHu32(DMAC_RBOR);
	int msize = psHu32(DMAC_RBSR)+16, ret;
	u32 *pMem;
	int mfifoqwc = vif1ch->qwc;

	/* Is QWC = 0? if so there is nothing to transfer */
	if (vif1ch->qwc == 0) return 0;

	if (vif1ch->madr >= maddr &&
		vif1ch->madr <= (maddr+msize)) {
		ret = mfifoVIF1rbTransfer();
	} else {
		pMem = (u32*)dmaGetAddr(vif1ch->madr);
		if (pMem == NULL) return -1;
		if(vif1.vifstalled == 1)
			ret = VIF1transfer(pMem+vif1.irqoffset, vif1ch->qwc*4-vif1.irqoffset, 0);
		else
			ret = VIF1transfer(pMem, vif1ch->qwc << 2, 0); 

		assert(ret == 0 ); // vif stall code not implemented

		//vif1ch->madr+= (vif1ch->qwc << 4);
	}

	cycles+= (mfifoqwc) * BIAS; /* guessing */
	mfifoqwc = 0;
	//vif1ch->qwc = 0;
	//if(vif1.vifstalled == 1) SysPrintf("Vif1 MFIFO stalls not implemented\n");
	//
	vif1.vifstalled = 0;
	return ret;
}

#define spr0 ((DMACh*)&PS2MEM_HW[0xD000])
static int tempqwc = 0;

void mfifoVIF1transfer(int qwc) {
	u32 *ptag;
	int id;
	int done = 0, ret;
	u32 temp = 0;
	cycles = 0;
	vifqwc += qwc;
	g_vifCycles = 0;
	/*if(vifqwc == 0) {
	//#ifdef PCSX2_DEVBUILD
				/*if( vifqwc > 1 )
					SysPrintf("vif mfifo tadr==madr but qwc = %d\n", vifqwc);*/
	//#endif
		
				//INT(10,50);
			/*	return;
			}*/
	/*if ((psHu32(DMAC_CTRL) & 0xC0)) { 
			SysPrintf("DMA Stall Control %x\n",(psHu32(DMAC_CTRL) & 0xC0));
			}*/
/*#ifdef VIF_LOG
	VIF_LOG("mfifoVIF1transfer %x madr %x, tadr %x\n", vif1ch->chcr, vif1ch->madr, vif1ch->tadr);
#endif*/
	
 //if((vif1ch->chcr & 0x100) == 0)SysPrintf("MFIFO VIF1 not ready!\n");
	//while (qwc > 0 && done == 0) {
	 if(vif1ch->qwc == 0){
			if(vif1ch->tadr == spr0->madr) {
	#ifdef PCSX2_DEVBUILD
				/*if( vifqwc > 1 )
					SysPrintf("vif mfifo tadr==madr but qwc = %d\n", vifqwc);*/
	#endif
				//hwDmacIrq(14);
				return;
			}
			
			ptag = (u32*)dmaGetAddr(vif1ch->tadr);
			
			id        = (ptag[0] >> 28) & 0x7;
			vif1ch->qwc  = (ptag[0] & 0xffff);
			vif1ch->madr = ptag[1];
			cycles += 2;
			
			
			
			if (vif1ch->chcr & 0x40) {
				ret = VIF1transfer(ptag+2, 2, 1);  //Transfer Tag
				if (ret == -2) {
					vif1.vifstalled = 1;
					return;        //IRQ set by VIFTransfer
				}
			}
			
			vif1ch->chcr = ( vif1ch->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );
			
	#ifdef VIF_LOG
			VIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx mfifo qwc = %x spr0 madr = %x\n",
					ptag[1], ptag[0], vif1ch->qwc, id, vif1ch->madr, vif1ch->tadr, vifqwc, spr0->madr);
	#endif
			vifqwc--;
			switch (id) {
				case 0: // Refe - Transfer Packet According to ADDR field
					vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));
					vif1.done = 2;										//End Transfer
					if((vif1ch->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						if(vifqwc < vif1ch->qwc) return;
					}
					break;

				case 1: // CNT - Transfer QWC following the tag.
					vif1ch->madr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));						//Set MADR to QW after Tag            
					vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->madr + (vif1ch->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
					vif1.done = 0;
					if((vif1ch->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						if(vifqwc < vif1ch->qwc) return;
					}
					break;

				case 2: // Next - Transfer QWC following tag. TADR = ADDR
					temp = vif1ch->madr;								//Temporarily Store ADDR
					vif1ch->madr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR)); 					  //Set MADR to QW following the tag
					vif1ch->tadr = temp;								//Copy temporarily stored ADDR to Tag
					vif1.done = 0;
					if((vif1ch->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						if(vifqwc < vif1ch->qwc) return;
					}
					break;

				case 3: // Ref - Transfer QWC from ADDR field
				case 4: // Refs - Transfer QWC from ADDR field (Stall Control) 
					vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));							//Set TADR to next tag
					vif1.done = 0;
					if((vif1ch->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						if(vifqwc < vif1ch->qwc) return;
					}
					break;

				case 7: // End - Transfer QWC following the tag
					vif1ch->madr = psHu32(DMAC_RBOR) + ((vif1ch->tadr + 16) & psHu32(DMAC_RBSR));		//Set MADR to data following the tag
					vif1ch->tadr = psHu32(DMAC_RBOR) + ((vif1ch->madr + (vif1ch->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
					vif1.done = 2;										//End Transfer
					if((vif1ch->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						if(vifqwc < vif1ch->qwc) return;
					}
					break;
			}
			
			
			//SysPrintf("VIF1 MFIFO qwc %d vif1 qwc %d, madr = %x, tadr = %x\n", qwc, vif1ch->qwc, vif1ch->madr, vif1ch->tadr);
			
			if((vif1ch->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)){
				vifqwc -= vif1ch->qwc;
			}
	 }

		ret = mfifoVIF1chain();
		if (ret == -1) {
			SysPrintf("dmaChain error %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx\n",
					ptag[1], ptag[0], vif1ch->qwc, id, vif1ch->madr, vif1ch->tadr);
			vif1.done = 1;
			INT(10,cycles+g_vifCycles);
		}
		if(ret == -2){
			//SysPrintf("VIF MFIFO Stall\n");
			vif1.vifstalled = 1;
			INT(10,cycles+g_vifCycles);
			return;
		}
		
		if ((vif1ch->chcr & 0x80) && (ptag[0] >> 31)) {
#ifdef VIF_LOG
			VIF_LOG("dmaIrq Set\n");
#endif
			//SysPrintf("mfifoVIF1transfer: dmaIrq Set\n");
			//vifqwc = 0;
			vif1.done = 1;
		}

//		if( (cpuRegs.interrupt & (1<<1)) && qwc > 0) {
//			SysPrintf("vif1 mfifo interrupt %d\n", qwc);
//		}
	//}

	/*if(vif1.done == 1) {
		vifqwc = 0;
	}*/
	INT(10,cycles+g_vifCycles);
	if(vif1.done == 2) vif1.done = 1;
	if(vifqwc == 0 && vif1.done == 0) hwDmacIrq(14);
	
	//hwDmacIrq(1);
#ifdef SPR_LOG
	SPR_LOG("mfifoVIF1transfer end %x madr %x, tadr %x\n", vif1ch->chcr, vif1ch->madr, vif1ch->tadr);
#endif
}

int vifMFIFOInterrupt()
{
	
	if(!(vif1ch->chcr & 0x100)) return 1;
	if(vif1.vifstalled == 1 && vif1Regs->stat & VIF1_STAT_VIS) {
		if(vif1.irq) {
			vif1Regs->stat|= VIF1_STAT_INT;
			hwIntcIrq(5);
			--vif1.irq;
		}
		
		return 1;
	}

	if(vif1.done != 1 && vifqwc != 0) {
		mfifoVIF1transfer(0);
		if(vif1ch->qwc > 0 && vif1.vifstalled == 0) return 1;
		else return 0;
	} else if(vif1.done != 1) return 1;

	vif1.done = 0;
	vif1ch->chcr &= ~0x100;
	hwDmacIrq(DMAC_VIF1);
//	vif1ch->chcr &= ~0x100;
//	vif1Regs->stat&= ~0x1F000000; // FQC=0
//	hwDmacIrq(DMAC_VIF1);
//
//	if (vif1.irq > 0) {
//		vif1.irq--;
//		hwIntcIrq(5); // VIF1 Intc
//	}
	return 1;
}
