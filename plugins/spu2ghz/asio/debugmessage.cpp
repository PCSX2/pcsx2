#include "asiosys.h"

#if DEBUG
#if MAC
#include <TextUtils.h>
void DEBUGGERMESSAGE(char *string)
{
	c2pstr(string);
	DebugStr((unsigned char *)string);
}
#else
#error debugmessage
#endif
#endif
