
#include "Main.h"

static loggingInfo* li;

EXPORT_C_(void) USBsetSettingsDir(const char* dir)
{
	setConfigDir(li, dir);
}

EXPORT_C_(void) USBsetLogDir(const char* dir)
{
	setLogDir(li, dir);
}

EXPORT_C_(s32) USBinit()
{
	setLogName("MultiNullUSB");
	openLog(li);
	doLog(li, "USBInit");
	return 0;
}

EXPORT_C_(void) USBshutdown()
{
	doLog(li, "USBShutdown");
	closeLog(li);
}

EXPORT_C_(s32) USBopen(void *pDsp)
{
	doLog(li, "USBOpen");
	return 0;
}

EXPORT_C_(void) USBclose()
{
	doLog(li, "USBClose");
}

EXPORT_C_(u8) USBread8(u32 addr)
{
	doLog(li, "USBRead8 %lx", addr);
	return 0;
}

EXPORT_C_(u16) USBread16(u32 addr)
{
	doLog(li, "USBRead16 %lx", addr);
	return 0;
}

EXPORT_C_(u32) USBread32(u32 addr)
{
	doLog(li, "USBRead32 %lx", addr);
	return 0;
}

EXPORT_C_(void) USBwrite8(u32 addr,  u8 value)
{
	doLog(li, "USBWrite8 %lx %x", addr, value);
}

EXPORT_C_(void) USBwrite16(u32 addr, u16 value)
{
	doLog(li, "USBWrite16 %lx %x", addr, value);
}

EXPORT_C_(void) USBwrite32(u32 addr, u32 value)
{
	doLog(li, "USBWrite32 %lx %x", addr, value);
}

EXPORT_C_(void) USBirqCallback(USBcallback callback)
{
	doLog(li, "USBIrqCallback");
}

static int _USBirqHandler(void)
{
	doLog(li, "_USBIrqHandler");
	return 0;
}

EXPORT_C_(USBhandler) USBirqHandler(void)
{
	doLog(li, "USBIrqHandler");
	return (USBhandler)_USBirqHandler;
}

EXPORT_C_(void) USBsetRAM(void *mem)
{
	doLog(li, "USBSetRAM");
}

EXPORT_C_(s32) USBtest()
{
	doLog(li, "USBTest");
	return 0;
}

EXPORT_C_(s32) USBfreeze(int mode, freezeData *data)
{
	doLog(li, "USBFreeze");
	return 0;
}
