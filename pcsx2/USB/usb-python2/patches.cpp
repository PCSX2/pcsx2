#include "PrecompiledHeader.h"

#include "patches.h"

#ifndef PCSX2_CORE
#include "gui/SysThreads.h"
#else
#include "VMManager.h"
#endif

#include "IopMem.h"
#include "Patch.h"

#include <wx/ffile.h>
#include <wx/fileconf.h>

namespace usb_python2
{
	std::thread mPatchSpdifAudioThread;
	std::atomic<bool> mPatchSpdifAudioThreadIsRunning;
	uint32_t mTargetWriteCmd = 0;
	uint32_t mTargetPatchAddr = 0;

	void Python2Patch::PatchSpdifAudioThread(void* ptr)
	{
		mPatchSpdifAudioThreadIsRunning = true;
		mTargetWriteCmd = 0;
		mTargetPatchAddr = 0;

		ForgetLoadedPatches();

		bool lastLoop = false;
		bool doLoop = true;
		while (doLoop)
		{
			if (
#ifndef PCSX2_CORE
				!GetCoreThread().IsOpen() || GetCoreThread().IsPaused()
#else
				VMManager::GetState() != Running /* Untested */
#endif
				|| psxMemRLUT == NULL || psxMemWLUT == NULL)
				continue;

			if (lastLoop)
				doLoop = false;

			// The GF games I looked all had the required code in this range, but it's possible there exists
			// code in other places.
			for (int i = 0x100000; i < 0x120000; i += 4)
			{
				if (
#ifndef PCSX2_CORE
					!GetCoreThread().IsOpen() || GetCoreThread().IsPaused()
#else
					VMManager::GetState() != Running /* Untested */
#endif
					|| psxMemRLUT == NULL || psxMemWLUT == NULL)
					break;

				// Generic pattern match to find the address where the audio mode is stored
				const auto x = iopMemRead32(i);
				if (mTargetWriteCmd != 0 && (x & 0xff00ffff) == mTargetWriteCmd)
				{
					Console.WriteLn("Patching write @ %08x...", i);

					// Patch write
					IniPatch iPatch = {0};
					iPatch.placetopatch = PPT_CONTINUOUSLY;
					iPatch.cpu = CPU_IOP;
					iPatch.addr = i;
					iPatch.type = WORD_T;
					iPatch.data = 0;
					iPatch.oldData = iopMemRead32(iPatch.addr);
					iPatch.hasOldData = true;
					iPatch.enabled = 1;
					LoadPatchFromMemory(iPatch);
				}
				else if (
					x == 0x00000000 &&
					iopMemRead32(i + 8) == 0x00000000 &&
					iopMemRead32(i + 12) == 0x24020001 &&
					(iopMemRead32(i + 16) & 0xffffff00) == 0x3c010000 &&
					(iopMemRead32(i + 20) & 0xffff0000) == 0xac220000 &&
					(iopMemRead32(i + 24) & 0xffff0000) == 0x08040000 &&
					iopMemRead32(i + 28) == 0x00000000 &&
					(iopMemRead32(i + 32) & 0xffffff00) == 0x3c010000)
				{
					const auto a = iopMemRead32(i + 16);
					const auto b = iopMemRead32(i + 20);
					const auto addr = ((a & 0xff) << 16) | (b & 0xffff);
					const auto writeCmd = iopMemRead32(i + 20) & 0xff00ffff;

					if (writeCmd != mTargetWriteCmd || addr != mTargetPatchAddr)
						Console.WriteLn("Found digital SPDIF/analog audio flag! %08x | %08x %08x | %08x | %08x", i, a, b, addr, writeCmd);

					mTargetWriteCmd = writeCmd;
					mTargetPatchAddr = addr;

					// Find other writes along the way and patch those out
					i += 28;

					// Patch jump
					IniPatch iPatch = {0};
					iPatch.placetopatch = PPT_CONTINUOUSLY;
					iPatch.cpu = CPU_IOP;
					iPatch.addr = i + 4;
					iPatch.type = WORD_T;
					iPatch.data = 0;
					iPatch.oldData = iopMemRead32(iPatch.addr);
					iPatch.hasOldData = true;
					iPatch.enabled = 1;
					LoadPatchFromMemory(iPatch);

					// Always set audio mode to analog (1) instead of digital/SPDIF (0)
					iPatch = {0};
					iPatch.placetopatch = PPT_CONTINUOUSLY;
					iPatch.cpu = CPU_IOP;
					iPatch.addr = mTargetPatchAddr;
					iPatch.type = WORD_T;
					iPatch.data = 1;
					iPatch.oldData = iopMemRead32(iPatch.addr);
					iPatch.hasOldData = true;
					iPatch.enabled = 1;
					LoadPatchFromMemory(iPatch);

					lastLoop = true;
				}
			}
		}

		// Force the patches to be applied right away
		ApplyLoadedPatches(PPT_CONTINUOUSLY);

		mPatchSpdifAudioThreadIsRunning = false;
	}
} // namespace usb_python2