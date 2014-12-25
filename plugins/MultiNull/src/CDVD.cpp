
#include "Main.h"

EXPORT_C_(s32) CDVDinit()
{
	return 0;
}

EXPORT_C_(void) CDVDshutdown()
{
}

EXPORT_C_(s32) CDVDopen(const char* pTitle)
{
	return 0;
}

EXPORT_C_(void) CDVDclose()
{
}

EXPORT_C_(s32) CDVDreadTrack(u32 lsn, int mode)
{
	return -1;
}

EXPORT_C_(u8*) CDVDgetBuffer()
{
	return NULL;
}

EXPORT_C_(s32) CDVDreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	return -1;
}

EXPORT_C_(s32) CDVDgetTN(cdvdTN *Buffer)
{
	return -1;
}

EXPORT_C_(s32) CDVDgetTD(u8 Track, cdvdTD *Buffer)
{
	return -1;
}

EXPORT_C_(s32) CDVDgetTOC(void* toc)
{
	return -1;
}

EXPORT_C_(s32) CDVDgetDiskType()
{
	return CDVD_TYPE_NODISC;
}

EXPORT_C_(s32) CDVDgetTrayStatus()
{
	return CDVD_TRAY_CLOSE;
}

EXPORT_C_(s32) CDVDctrlTrayOpen()
{
	return 0;
}

EXPORT_C_(s32) CDVDctrlTrayClose()
{
	return 0;
}

EXPORT_C_(s32) CDVDtest()
{
	return 0;
}