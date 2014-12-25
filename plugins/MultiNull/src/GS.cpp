
#include "Main.h"

static loggingInfo* li;

EXPORT_C_(void) GSsetSettingsDir(const char* dir)
{
	setConfigDir(li, dir);
}

EXPORT_C_(void) GSsetLogDir(const char* dir)
{
	setLogDir(li, dir);
}

EXPORT_C_(s32) GSinit()
{
	setLogName("MultiNullGS");
	openLog(li);
	doLog(li, "GSInit");
	return 0;
}

EXPORT_C_(void) GSshutdown()
{
	doLog(li, "GSShutdown");
	closeLog(li);
}

EXPORT_C_(s32) GSopen(void *pDsp, char *Title, int multithread)
{
	doLog(li, "GSOpen");
	return 0;
}

EXPORT_C_(s32) GSopen2(void *pDsp, u32 flags)
{
	doLog(li, "GSOpen2");
	return 0;
}

EXPORT_C_(void) GSclose()
{
	doLog(li, "GSClose");
}

EXPORT_C_(void) GSirqCallback(void (*callback)())
{
	doLog(li, "GSIrqCallback");
}

EXPORT_C_(s32) GStest()
{
	doLog(li, "GSTest");
	return 0;
}

EXPORT_C_(s32) GSfreeze(int mode, freezeData *data)
{
	doLog(li, "GSFreeze");
	return 0;
}

EXPORT_C_(void) GSvsync(int field)
{
	doLog(li, "GSVSync");
}

EXPORT_C_(void) GSgetLastTag(u64* ptag)
{
	doLog(li, "GSGetLastTag");
}

EXPORT_C_(void) GSgifTransfer(const u32 *pMem, u32 addr)
{
	doLog(li, "GSGifTransfer");
}

EXPORT_C_(void) GSgifTransfer1(u32 *pMem, u32 addr)
{
	doLog(li, "GSGifTransfer1");
}

EXPORT_C_(void) GSgifTransfer2(u32 *pMem, u32 size)
{
	doLog(li, "GSGifTransfer2");
}

EXPORT_C_(void) GSgifTransfer3(u32 *pMem, u32 size)
{
	doLog(li, "GSGifTransfer3");
}

EXPORT_C_(void) GSgifSoftReset(u32 mask)
{
	doLog(li, "GSSoftReset");
}

EXPORT_C_(void) GSinitReadFIFO(u64 *mem)
{
	doLog(li, "GSInitReadFIFO");
}

EXPORT_C_(void) GSreadFIFO(u64 *mem)
{
	doLog(li, "GSReadFIFO");
}

EXPORT_C_(void) GSinitReadFIFO2(u64 *mem, int qwc)
{
	doLog(li, "GSInitReadFIFO2");
}

EXPORT_C_(void) GSreadFIFO2(u64 *mem, int qwc)
{
	doLog(li, "GSReadFIFO2");
}

EXPORT_C_(void) GSkeyEvent(keyEvent *ev)
{
	doLog(li, "GSKeyEvent");
}

EXPORT_C_(void) GSchangeSaveState(int, const char* filename)
{
	doLog(li, "GSChangeSaveState");
}

EXPORT_C_(void) GSmakeSnapshot(char *pathname)
{
	doLog(li, "GSSnapshot %s", pathname);
}

EXPORT_C_(void) GSmakeSnapshot2(char *pathname, int* snapdone, int savejpg)
{
	doLog(li, "GSSnapshot2 %s", pathname);
}

EXPORT_C_(void) GSsetBaseMem(void*)
{
	doLog(li, "GSSetBaseMem");
}

EXPORT_C_(void) GSsetGameCRC(int crc, int gameoptions)
{
	doLog(li, "GSCRC '%x' Options 0x%x", crc, gameoptions);
}

EXPORT_C_(void) GSsetFrameSkip(int frameskip)
{
	doLog(li, "GSFrameskip %d", frameskip);
}

EXPORT_C_(int) GSsetupRecording(int start, void* pData)
{
	doLog(li, "GSRecord %d", start);
	return 1;
}

EXPORT_C_(void) GSreset()
{
	doLog(li, "GSReset");
}

EXPORT_C_(void) GSwriteCSR(u32 value)
{
	doLog(li, "GSWriteCSR");
}

EXPORT_C_(void) GSgetDriverInfo(GSdriverInfo *info)
{
	doLog(li, "GSDriverInfo");
}