#include "../osdebugout.h"
#include "../configuration.h"
#include "../deviceproxy.h"
#include "../usb-pad/padproxy.h"
#include "../usb-mic/audiodeviceproxy.h"

#include "config.h"

void SysMessage_stderr(const char *fmt, ...)
{
	va_list arglist;

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
}