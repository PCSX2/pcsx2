#include "USB//usb-python2/usb-python2.h"

namespace usb_python2
{
	extern std::thread mPatchSpdifAudioThread;
	extern std::atomic<bool> mPatchSpdifAudioThreadIsRunning;

	class Python2Patch
	{
	public:
		static void PatchSpdifAudioThread(void* ptr);
	};
} // namespace usb_python2