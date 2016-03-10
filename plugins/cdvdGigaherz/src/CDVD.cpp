/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
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

#include <stdio.h>

#include "CDVD.h"
#include "resource.h"
#include "Shlwapi.h"

#include "svnrev.h"

#include <assert.h>

void (*newDiscCB)();

HANDLE hThread_keepAlive = INVALID_HANDLE_VALUE;
HANDLE hNotify_keepAlive = INVALID_HANDLE_VALUE;
DWORD  pidThreadKeepAlive = 0;

#define STRFY(x) #x
#define TOSTR(x) STRFY(x)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// State Information                                                         //

int strack;
int etrack;
track tracks[100];

int curDiskType;
int curTrayStatus;

int csector;
int cmode;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Plugin Interface                                                          //

char *LibName       = "cdvdGigaherz "
#ifdef PCSX2_DEBUG
	" Debug "
#endif
	"(" TOSTR(SVN_REV)
#if SVN_MODS
	"/modded"
#endif
	")";

const unsigned char version = PS2E_CDVD_VERSION;
const unsigned char revision = 0;
const unsigned char build = 9;

HINSTANCE hinst;

BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,  // handle to DLL module
  DWORD fdwReason,     // reason for calling function
  LPVOID lpvReserved   // reserved
)
{
	if(fdwReason==DLL_PROCESS_ATTACH) {
		hinst=hinstDLL;
	}
	return TRUE;
}

char* CALLBACK PS2EgetLibName() {
	return LibName;
}

u32 CALLBACK PS2EgetLibType() {
	return PS2E_LT_CDVD;
}

u32 CALLBACK PS2EgetLibVersion2(u32 type) {
	return (version << 16) | (revision << 8) | build;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Utility Functions                                                         //

void SysMessage(char *fmt, ...) {
	va_list list;
	char tmp[512];

	va_start(list,fmt);
	vsprintf(tmp,fmt,list);
	va_end(list);
	MessageBox(0, tmp, "cdvdGigaherz Msg", 0);
}

u8 __inline dec_to_bcd(u8 dec)
{
	return ((dec/10)<<4)|(dec%10);
}

void __inline lsn_to_msf(u8* minute, u8* second, u8* frame, u32 lsn)
{
	*frame = dec_to_bcd(lsn%75);
	lsn/=75;
	*second= dec_to_bcd(lsn%60);
	lsn/=60;
	*minute= dec_to_bcd(lsn%100);
}

void __inline lba_to_msf(s32 lba, u8* m, u8* s, u8* f) {
	lba += 150;
	*m = (u8)(lba / (60*75));
	*s = (u8)((lba / 75) % 60);
	*f = (u8)(lba % 75);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// CDVD processing functions                                                 //

char csrc[20];

bool cdvd_is_open = false;
bool cdvdKeepAlive_is_open = false;
bool disc_has_changed = false;
bool weAreInNewDiskCB = false;

Source *src;

char bfr[2352];
char throwaway[2352];
extern s32 prefetch_last_lba;
extern s32 prefetch_last_mode;

///////////////////////////////////////////////////////////////////////////////
// keepAliveThread throws a read event regularly to prevent drive spin down  //

DWORD CALLBACK keepAliveThread(PVOID param)
{
	printf(" * CDVD: KeepAlive thread started...\n");

	while (cdvdKeepAlive_is_open)
	{
		// Sleep 30 seconds with thread abort check
        if (WaitForSingleObject(hNotify_keepAlive, 30000) != WAIT_TIMEOUT) break;
        if (!cdvdKeepAlive_is_open) {
            break;
        }

		//printf(" * keepAliveThread: polling drive.\n");
		//if (prefetch_last_mode == CDVD_MODE_2048)
			src->ReadSectors2048(prefetch_last_lba, 1, throwaway);
		//else
		//	src->ReadSectors2352(prefetch_last_lba, 1, throwaway);
	}

	printf(" * CDVD: KeepAlive thread finished.\n");
	
	return 0;
}

s32 StartKeepAliveThread()
{
    hNotify_keepAlive = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hNotify_keepAlive == INVALID_HANDLE_VALUE)
        return -1;

	cdvdKeepAlive_is_open = true;
	hThread_keepAlive = CreateThread(NULL, 0, keepAliveThread, NULL, 0, &pidThreadKeepAlive);
	if (hThread_keepAlive == INVALID_HANDLE_VALUE) {
		cdvdKeepAlive_is_open = false;
		return -1;
	}

	SetThreadPriority(hThread_keepAlive, THREAD_PRIORITY_NORMAL);

	return 0;
}

void StopKeepAliveThread()
{
	cdvdKeepAlive_is_open = false;
	PulseEvent(hNotify_keepAlive);
	if (WaitForSingleObject(hThread_keepAlive, 5000) == WAIT_TIMEOUT)
	{
		TerminateThread(hThread_keepAlive, 0);
	}
	CloseHandle(hThread_keepAlive);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// CDVD Pluin Interface                                                      //

void CALLBACK CDVDsetSettingsDir(const char* dir)
{
	CfgSetSettingsDir(dir);
}

s32 CALLBACK CDVDinit()
{
	return 0;
}

s32 CALLBACK CDVDopen(const char* pTitleFilename)
{
	ReadSettings();

	if(source_drive=='-')
	{
		// MSDN : Trailing backslash is required to ensure consistent behavior across
		// various versions of Windows and storage types.
		char temp[]="A:\\";

		for(char d='A';d<='Z';d++)
		{
			temp[0]=d;
			if(GetDriveType(temp)==DRIVE_CDROM)
			{
				source_drive=d;
				break;
			}
		}
	}

	if(source_drive=='@')
	{
		curDiskType=CDVD_TYPE_NODISC;
		return 0;
	}

	if(source_drive=='$')
	{
		printf(" * CDVD: Opening image '%s'...\n",source_file);

		//open device file
		src= TryLoaders(source_file);
	}
	else
	{
		sprintf(csrc,"\\\\.\\%c:",source_drive);

		printf(" * CDVD: Opening drive '%s'...\n",csrc);

		//open device file
		src=new IOCtlSrc(csrc);
	}

	if(!src->IsOK())
	{
		printf(" * CDVD: Error opening source.\n");
		return -1;
	}

	//setup threading manager
	cdvdStartThread();
	StartKeepAliveThread();

	return cdvdRefreshData();
}

void CALLBACK CDVDclose()
{
	StopKeepAliveThread();
	cdvdStopThread();
	//close device
	delete src;
	src=NULL;
}

void CALLBACK CDVDshutdown()
{
	//nothing to do here
}

s32  CALLBACK CDVDgetDualInfo(s32* dualType, u32* _layer1start)
{
	switch(src->GetMediaType())
	{
	case 1:
		*dualType = 1;
		*_layer1start = src->GetLayerBreakAddress() + 1;
		return 0;
	case 2:
		*dualType = 2;
		*_layer1start = src->GetLayerBreakAddress() + 1;
		return 0;
	case 0:
		*dualType = 0;
		*_layer1start = 0;
		return 0;
	}
	return -1;
}

int lastReadInNewDiskCB=0;
char directReadSectorBuffer[2448];

s32  CALLBACK CDVDreadSector(u8* buffer, s32 lsn, int mode)
{
	return cdvdDirectReadSector(lsn,mode,(char*)buffer);
}

s32 CALLBACK CDVDreadTrack(u32 lsn, int mode)
{
	csector=lsn;
	cmode=mode;

	if(weAreInNewDiskCB)
	{
		int ret = cdvdDirectReadSector(lsn,mode,directReadSectorBuffer);
		if(ret==0) lastReadInNewDiskCB=1;
		return ret;
	}

	if(lsn>tracks[0].length) // track 0 is total disc.
	{
		return -1;
	}

	return cdvdRequestSector(lsn,mode);
}

// return can be NULL (for async modes)
u8*  CALLBACK CDVDgetBuffer()
{
	if(lastReadInNewDiskCB)
	{
		lastReadInNewDiskCB=0;
		return (u8*)directReadSectorBuffer;
	}

	u8 *s = (u8*)cdvdGetSector(csector,cmode);

	return s;
}

// return can be NULL (for async modes)
int CALLBACK CDVDgetBuffer2(u8* dest)
{
	int csize = 2352;
	switch(cmode)
	{
	case CDVD_MODE_2048: csize = 2048; break;
	case CDVD_MODE_2328: csize = 2328; break;
	case CDVD_MODE_2340: csize = 2340; break;
	}

	if(lastReadInNewDiskCB)
	{
		lastReadInNewDiskCB=0;

		memcpy(dest, directReadSectorBuffer, csize);
		return 0;
	}

	memcpy(dest, cdvdGetSector(csector,cmode), csize);

	return 0;
}

s32 CALLBACK CDVDreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	int i;
	// the formatted subq command returns:  control/adr, track, index, trk min, trk sec, trk frm, 0x00, abs min, abs sec, abs frm

	if(lsn>tracks[0].length) // track 0 is total disc.
		return -1;

	memset(subq,0,sizeof(cdvdSubQ));

	lsn_to_msf(&subq->discM,&subq->discS,&subq->discF,lsn+150);

	i=strack;
	while(i<=etrack)
	{
		if(lsn<=tracks[i].length)
			break;
		lsn-=tracks[i].length;
		i++;
	}

	if(i>etrack)
		i=etrack;

	lsn_to_msf(&subq->trackM,&subq->trackS,&subq->trackF,lsn);

	subq->mode=1;
	subq->ctrl=tracks[i].type;
	subq->trackNum=i;
	subq->trackIndex=1;

	return 0;
}

s32 CALLBACK CDVDgetTN(cdvdTN *Buffer)
{
	Buffer->strack=strack;
	Buffer->etrack=etrack;
	return 0;
}

s32 CALLBACK CDVDgetTD(u8 Track, cdvdTD *Buffer)
{
	if(Track==0)
	{
		Buffer->lsn = tracks[0].length;
		Buffer->type= 0;
		return 0;
	}

	if(Track<strack) return -1;
	if(Track>etrack) return -1;

	Buffer->lsn = tracks[Track].start_lba;
	Buffer->type= tracks[Track].type;
	return 0;
}

u32 layer1start=-1;

s32 CALLBACK CDVDgetTOC(u8* tocBuff)
{
	//return src->ReadTOC((char*)toc,2048);
	//that didn't work too well...

	if(curDiskType==CDVD_TYPE_NODISC)
		return -1;

	if((curDiskType == CDVD_TYPE_DVDV) ||
	   (curDiskType == CDVD_TYPE_PS2DVD))
	{
		memset(tocBuff, 0, 2048);

		s32 mt=src->GetMediaType();

		if(mt<0)
			return -1;

		if(mt==0) //single layer
		{
			// fake it
			tocBuff[ 0] = 0x04;
			tocBuff[ 1] = 0x02;
			tocBuff[ 2] = 0xF2;
			tocBuff[ 3] = 0x00;
			tocBuff[ 4] = 0x86;
			tocBuff[ 5] = 0x72;

			tocBuff[16] = 0x00; // first sector for layer 0
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;
		}
		else if(mt==1) //PTP
		{
			layer1start = src->GetLayerBreakAddress() + 0x30000;

			// dual sided
			tocBuff[ 0] = 0x24;
			tocBuff[ 1] = 0x02;
			tocBuff[ 2] = 0xF2;
			tocBuff[ 3] = 0x00;
			tocBuff[ 4] = 0x41;
			tocBuff[ 5] = 0x95;

			tocBuff[14] = 0x61; // PTP

			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			tocBuff[20] = (layer1start>>24);
			tocBuff[21] = (layer1start>>16)&0xff;
			tocBuff[22] = (layer1start>> 8)&0xff;
			tocBuff[23] = (layer1start>> 0)&0xff;
		}
		else //OTP
		{
			layer1start = src->GetLayerBreakAddress() + 0x30000;

			// dual sided
			tocBuff[ 0] = 0x24;
			tocBuff[ 1] = 0x02;
			tocBuff[ 2] = 0xF2;
			tocBuff[ 3] = 0x00;
			tocBuff[ 4] = 0x41;
			tocBuff[ 5] = 0x95;

			tocBuff[14] = 0x71; // OTP

			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			tocBuff[24] = (layer1start>>24);
			tocBuff[25] = (layer1start>>16)&0xff;
			tocBuff[26] = (layer1start>> 8)&0xff;
			tocBuff[27] = (layer1start>> 0)&0xff;
		}
	}
	else if(curDiskType == CDVD_TYPE_CDDA ||
			curDiskType == CDVD_TYPE_PS2CDDA ||
			curDiskType == CDVD_TYPE_PS2CD ||
			curDiskType == CDVD_TYPE_PSCDDA ||
			curDiskType == CDVD_TYPE_PSCD)
	{
		// cd toc
		// (could be replaced by 1 command that reads the full toc)
		u8 min, sec, frm,i;
		s32 err;
		cdvdTN diskInfo;
		cdvdTD trackInfo;
		memset(tocBuff, 0, 1024);
		if (CDVDgetTN(&diskInfo) == -1)	{ diskInfo.etrack = 0;diskInfo.strack = 1; }
		if (CDVDgetTD(0, &trackInfo) == -1) trackInfo.lsn = 0;

		tocBuff[0] = 0x41;
		tocBuff[1] = 0x00;

#define itob(n) ((((n)/10)<<4)+((n)%10))

		//Number of FirstTrack
		tocBuff[2] = 0xA0;
		tocBuff[7] = itob(diskInfo.strack);

		//Number of LastTrack
		tocBuff[12] = 0xA1;
		tocBuff[17] = itob(diskInfo.etrack);

		//DiskLength
		lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
		tocBuff[22] = 0xA2;
		tocBuff[27] = itob(min);
		tocBuff[28] = itob(sec);
		tocBuff[29] = itob(frm);

		fprintf(stderr,"Track 0: %d mins %d secs %d frames\n",min,sec,frm);

		for (i=diskInfo.strack; i<=diskInfo.etrack; i++)
		{
			err = CDVDgetTD(i, &trackInfo);
			lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
			tocBuff[i*10+30] = trackInfo.type;
			tocBuff[i*10+32] = err == -1 ? 0 : itob(i);	  //number
			tocBuff[i*10+37] = itob(min);
			tocBuff[i*10+38] = itob(sec);
			tocBuff[i*10+39] = itob(frm);
			fprintf(stderr,"Track %d: %d mins %d secs %d frames\n",i,min,sec,frm);
		}
	}
	else
		return -1;

	return 0;
}

s32 CALLBACK CDVDgetDiskType()
{
	return curDiskType;
}

s32 CALLBACK CDVDgetTrayStatus()
{
	return curTrayStatus;
}

s32 CALLBACK CDVDctrlTrayOpen()
{
	curTrayStatus=CDVD_TRAY_OPEN;
	return 0;
}

s32 CALLBACK CDVDctrlTrayClose()
{
	curTrayStatus=CDVD_TRAY_CLOSE;
	return 0;
}

void CALLBACK CDVDnewDiskCB(void (*callback)())
{
	newDiscCB=callback;
}

void configure();
void CALLBACK CDVDconfigure()
{
	configure();
}

void CALLBACK CDVDabout() {
	SysMessage("%s %d.%d", LibName, revision, build);
}

s32 CALLBACK CDVDtest() {
	return 0;
}
