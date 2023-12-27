// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "deci2.h"

struct DECI2_TTYP_HEADER{
	DECI2_HEADER	h;		//+00
	u32		flushreq;	//+08
	//u8	data[0];	//+0C // Not used, so commented out (cottonvibes)
};			//=0C

void sendTTYP(u16 protocol, u8 source, char *data){
	static char tmp[2048];
	((DECI2_TTYP_HEADER*)tmp)->h.length		=sizeof(DECI2_TTYP_HEADER)+strlen(data);
	((DECI2_TTYP_HEADER*)tmp)->h._pad		=0;
	((DECI2_TTYP_HEADER*)tmp)->h.protocol	=protocol +(source=='E' ? PROTO_ETTYP : PROTO_ITTYP);
	((DECI2_TTYP_HEADER*)tmp)->h.source		=source;
	((DECI2_TTYP_HEADER*)tmp)->h.destination='H';
	((DECI2_TTYP_HEADER*)tmp)->flushreq		=0;
	if (((DECI2_TTYP_HEADER*)tmp)->h.length>2048)
		Msgbox::Alert(L"TTYP: Buffer overflow");
	else
		memcpy(&tmp[sizeof(DECI2_TTYP_HEADER)], data, strlen(data));
	//writeData(tmp);
}
