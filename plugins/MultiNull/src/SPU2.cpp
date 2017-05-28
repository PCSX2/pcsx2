
#include "Main.h"

static loggingInfo* li;

EXPORT_C_(void) SPU2setSettingsDir(const char* dir)
{
	setConfigDir(li, dir);
}

EXPORT_C_(void)  SPU2setLogDir(const char* dir)
{
	setLogDir(li, dir);
}

EXPORT_C_(s32) SPU2init()
{
	setLogName("MultiNullSPU2");
	openLog(li);
	doLog(li, "SPU2Init");
	return 0;
}

EXPORT_C_(void) SPU2shutdown()
{
	doLog(li, "SPU2Shutdown");
	closeLog(li);
}

EXPORT_C_(s32) SPU2open(void *pDsp)
{
	doLog(li, "SPU2Open");
	return 0;
}

EXPORT_C_(void) SPU2close()
{
	doLog(li, "SPU2Close");
}

EXPORT_C_(void) SPU2async(u32 cycle)
{
}

EXPORT_C_(void) SPU2readDMA4Mem(u16 *pMem, int size)
{
	doLog(li, "SPU2ReadDMA4Mem %x %x", size, pMem);
}

EXPORT_C_(void) SPU2readDMA7Mem(u16* pMem, int size)
{
	doLog(li, "SPU2ReadDMA7Mem %x %x", size, pMem);
}

EXPORT_C_(void) SPU2writeDMA4Mem(u16* pMem, int size)
{
	doLog(li, "SPU2WriteDMA4Mem %x %x", size, pMem);
}

EXPORT_C_(void) SPU2writeDMA7Mem(u16* pMem, int size)
{
	doLog(li, "SPU2WriteDMA7Mem %x %x", size, pMem);
}

EXPORT_C_(void) SPU2interruptDMA4()
{
	doLog(li, "SPU2InterruptDMA4");
}

EXPORT_C_(void) SPU2interruptDMA7()
{
	doLog(li, "SPU2InterruptDMA7");
}

EXPORT_C_(void) SPU2write(u32 mem, u16 value)
{
	doLog(li, "SPU2Write %x %x", mem, value);
}

EXPORT_C_(u16) SPU2read(u32 mem)
{
	doLog(li, "SPU2Read %x", mem);
	return 0;
}

EXPORT_C_(void) SPU2WriteMemAddr(int core, u32 value)
{
	doLog(li, "SPU2WriteMemAddr");
}

EXPORT_C_(u32) SPU2ReadMemAddr(int core)
{
	doLog(li, "SPU2ReadMemAddr");
	return 0;
}

EXPORT_C_(void) SPU2irqCallback(void (*SPU2callback)(), void (*DMA4callback)(), void (*DMA7callback)())
{
	doLog(li, "SPU2IrqCallback");
}

EXPORT_C_(s32) SPU2test()
{
	doLog(li, "SPU2Test");
	return 0;
}

EXPORT_C_(s32) SPU2freeze(int mode, freezeData *data)
{
	doLog(li, "SPU2Freeze");
	return 0;
}
