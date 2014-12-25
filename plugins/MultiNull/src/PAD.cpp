
#include "Main.h"

EXPORT_C_(void) PADsetSettingsDir(const char* dir)
{
}

EXPORT_C_(void) PADsetLogDir(const char* dir)
{
}

EXPORT_C_(s32) PADinit(u32 flags)
{
	return 0;
}

EXPORT_C_(void) PADshutdown()
{
}

EXPORT_C_(s32) PADopen(void *pDsp)
{
	return 0;
}

EXPORT_C_(void) PADclose()
{
}

EXPORT_C_(keyEvent*) PADkeyEvent()
{
	return 0;
}

EXPORT_C_(u8) PADstartPoll(int pad)
{
	return 0;
}

EXPORT_C_(u8) PADpoll(u8 value)
{
	return 0;
}

EXPORT_C_(u32) PADquery()
{
	return 3;
}

EXPORT_C_(void) PADupdate(int pad)
{
}

EXPORT_C_(void) PADgsDriverInfo(GSdriverInfo *info)
{
}

EXPORT_C_(s32) PADfreeze(int mode, freezeData *data)
{
	return 0;
}

EXPORT_C_(s32) PADtest()
{
	return 0;
}
