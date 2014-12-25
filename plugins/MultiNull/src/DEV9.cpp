
#include "Main.h"

static loggingInfo* li;

EXPORT_C_(void) DEV9setSettingsDir(const char* dir)
{
	setConfigDir(li, dir);
}

EXPORT_C_(void)  DEV9setLogDir(const char* dir)
{
	setLogDir(li, dir);
}

EXPORT_C_(s32) DEV9init()
{
	setLogName("MultiNullDEV9");
	openLog(li);
	doLog(li, "DEV9Init");
	return 0;
}

EXPORT_C_(void) DEV9shutdown()
{
	doLog(li, "DEV9Shutdown");
	closeLog(li);
}

EXPORT_C_(s32) DEV9open(void *pDsp)
{
	doLog(li, "DEV9Open");
	return 0;
}

EXPORT_C_(void) DEV9close()
{
	doLog(li, "DEV9Close");
}

EXPORT_C_(u8) DEV9read8(u32 addr)
{
    doLog(li, "DEV9Read8 %lx", addr);
	return 0;
}

EXPORT_C_(u16) DEV9read16(u32 addr)
{
    doLog(li, "DEV9Read16 %lx", addr);
	return 0;
}

EXPORT_C_(u32 ) DEV9read32(u32 addr)
{
	doLog(li, "DEV9Read32 %lx", addr);
	return 0;
}

EXPORT_C_(void) DEV9write8(u32 addr,  u8 value)
{
	doLog(li, "DEV9Write8 %lx %x", addr, value);
}

EXPORT_C_(void) DEV9write16(u32 addr, u16 value)
{
	doLog(li, "DEV9Write16 %lx %x", addr, value);
}

EXPORT_C_(void) DEV9write32(u32 addr, u32 value)
{
	doLog(li, "DEV9Write32 %lx %x", addr, value);
}

EXPORT_C_(s32) DEV9dmaRead(s32 channel, u32* data, u32 bytesLeft, u32* bytesProcessed)
{
	doLog(li, "DEV9ReadDMA");
	return 0;
}

EXPORT_C_(s32) DEV9dmaWrite(s32 channel, u32* data, u32 bytesLeft, u32* bytesProcessed)
{
	doLog(li, "DEV9WriteDMA");
	return 0;
}

EXPORT_C_(void) DEV9dmaInterrupt(s32 channel) 
{
}

EXPORT_C_(void) DEV9readDMA8Mem(u32 *pMem, int size)
{
	doLog(li, "DEV9ReadDMA2");
}

EXPORT_C_(void) DEV9writeDMA8Mem(u32* pMem, int size)
{
	doLog(li, "DEV9WriteDMA2");
}

EXPORT_C_(void) DEV9irqCallback(DEV9callback callback)
{
	doLog(li, "DEV9IrqCallback");
}

int _DEV9irqHandler(void)
{
	doLog(li, "_DEV9IrqHandler");
	return 0;
}

EXPORT_C_(DEV9handler) DEV9irqHandler(void)
{
	doLog(li, "DEV9IrqHandler");
	return (DEV9handler)_DEV9irqHandler;
}

EXPORT_C_(s32) DEV9test()
{
	doLog(li, "DEV9Test");
	return 0;
}

EXPORT_C_(s32) DEV9freeze(int mode, freezeData *data)
{
	doLog(li, "DEV9Freeze");
	return 0;
}
