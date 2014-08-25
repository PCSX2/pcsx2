#include "PrecompiledHeader.h"
#include "BiosDebugData.h"
#include "../Memory.h"

std::vector<EEThread> getEEThreads()
{
	std::vector<EEThread> threads;

	if (CurrentBiosInformation == NULL)
		return threads;

	u32 start = CurrentBiosInformation->threadListAddr & 0x3fffff;
	for (int tid = 0; tid < 256; tid++)
	{
		EEThread thread;

		EEInternalThread* internal = (EEInternalThread*) PSM(start+tid*sizeof(EEInternalThread));
		if (internal->status != THS_BAD)
		{
			thread.tid = tid;
			thread.data = *internal;
			threads.push_back(thread);
		}
	}

	return threads;
}

