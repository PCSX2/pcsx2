#include <sys/soundcard.h>

#if SOUND_VERSION < 0x040000
# error "OSSv4 is not available in sys/soundcard.h"
#endif

int main()
{
	return 0;
}
