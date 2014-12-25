
#include "Main.h"

static loggingInfo* li;

EXPORT_C_(void) FWsetSettingsDir(const char* dir)
{
	setConfigDir(li, dir);
}

EXPORT_C_(void) FWsetLogDir(const char* dir)
{
	setLogDir(li, dir);
}

EXPORT_C_(s32) FWinit()
{
	setLogName("MultiNullFW");
	openLog(li);
	doLog(li, "FWInit");
	return 0;
}

EXPORT_C_(void) FWshutdown()
{
	doLog(li, "FWShutdown");
	closeLog(li);
}

EXPORT_C_(s32) FWopen(void *pDsp)
{
	doLog(li, "FWOpen");
	return 0;
}

EXPORT_C_(void) FWclose()
{
	doLog(li, "FWClose");
}

EXPORT_C_(u32) FWread32(u32 addr)
{
	doLog(li, "FWRead32 0x%x", addr);
	return 0;
}

EXPORT_C_(void) FWwrite32(u32 addr, u32 value)
{
	doLog(li, "FWWrite32 0x%x 0x%x", addr, value);
}

EXPORT_C_(void) FWirqCallback(void (*callback)())
{
	doLog(li, "FWIrqCallback");
}

EXPORT_C_(s32) FWtest()
{
	doLog(li, "FWTest");
	return 0;
}

EXPORT_C_(s32) FWfreeze(int mode, freezeData *data)
{
	doLog(li, "FWFreeze");
	return 0;
}

