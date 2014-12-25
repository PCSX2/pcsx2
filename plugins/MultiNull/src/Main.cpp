
#include "Main.h"

EXPORT_C_(char*) PS2EgetLibName()
{
	return "MultiNull";	
}

EXPORT_C_(u32) PS2EgetLibType()
{
	return 0
#ifdef MULTINULL_BUILD_GS
	| PS2E_LT_GS
#endif
#ifdef MULTINULL_BUILD_PAD
	| PS2E_LT_PAD
#endif
#ifdef MULTINULL_BUILD_SPU2
	| PS2E_LT_SPU2
#endif
#ifdef MULTINULL_BUILD_CDVD
	| PS2E_LT_CDVD
#endif
#ifdef MULTINULL_BUILD_DEV9
	| PS2E_LT_DEV9
#endif
#ifdef MULTINULL_BUILD_USB
	| PS2E_LT_USB
#endif
#ifdef MULTINULL_BUILD_FW
	| PS2E_LT_FW
#endif
	;
}

EXPORT_C_(u32) CALLBACK PS2EgetLibVersion2(u32 type)
{
	u32 pluginVersion;
	switch(type)
	{
#ifdef MULTINULL_BUILD_GS
		case PS2E_LT_GS:
			pluginVersion = PS2E_GS_VERSION;
			break;
#endif
#ifdef MULTINULL_BUILD_PAD
		case PS2E_LT_PAD:
			pluginVersion = PS2E_PAD_VERSION;
			break;
#endif
#ifdef MULTINULL_BUILD_SPU2
		case PS2E_LT_SPU2:
			pluginVersion = PS2E_SPU2_VERSION;
			break;
#endif
#ifdef MULTINULL_BUILD_CDVD
		case PS2E_LT_CDVD:
			pluginVersion = PS2E_CDVD_VERSION;
			break;
#endif
#ifdef MULTINULL_BUILD_DEV9
		case PS2E_LT_DEV9:
			pluginVersion = PS2E_DEV9_VERSION;
			break;
#endif
#ifdef MULTINULL_BUILD_USB
		case PS2E_LT_USB:
			pluginVersion = PS2E_USB_VERSION;
			break;
#endif
#ifdef MULTINULL_BUILD_FW
		case PS2E_LT_FW:
			pluginVersion = PS2E_FW_VERSION;
			break;
#endif
		default:
			pluginVersion = 0;
			break;
	}
	return 0
	| (pluginVersion<< 16) //version
	| (0 << 8) //revision
	| (0) //build
	;
}

ENTRY_POINT;
