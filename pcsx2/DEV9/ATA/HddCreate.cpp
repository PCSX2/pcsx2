/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include <fstream>
#include <fmt/format.h>
#include "HddCreate.h"

void HddCreate::Start()
{
	Init();
	WriteImage(filePath, neededSize);
	Cleanup();
}

void HddCreate::WriteImage(ghc::filesystem::path hddPath, u64 reqSizeBytes)
{
	constexpr int buffsize = 4 * 1024;
	u8 buff[buffsize] = {0}; //4kb

	if (ghc::filesystem::exists(hddPath))
	{
		errored.store(true);
		SetError();
		return;
	}

	std::fstream newImage = ghc::filesystem::fstream(hddPath, std::ios::out | std::ios::binary);

	if (newImage.fail())
	{
		errored.store(true);
		SetError();
		return;
	}

	//Size file
	newImage.seekp(reqSizeBytes - 1, std::ios::beg);
	const char zero = 0;
	newImage.write(&zero, 1);

	if (newImage.fail())
	{
		newImage.close();
		ghc::filesystem::remove(filePath);
		errored.store(true);
		SetError();
		return;
	}

	lastUpdate = std::chrono::steady_clock::now();

	newImage.seekp(0, std::ios::beg);

	//Round up
	const s32 reqMiB = (reqSizeBytes + ((1024 * 1024) - 1)) / (1024 * 1024);
	for (s32 iMiB = 0; iMiB < reqMiB; iMiB++)
	{
		//Round down
		const s32 req4Kib = std::min<s32>(1024, (reqSizeBytes / 1024) - (u64)iMiB * 1024) / 4;
		for (s32 i4kb = 0; i4kb < req4Kib; i4kb++)
		{
			newImage.write((char*)buff, buffsize);
			if (newImage.fail())
			{
				newImage.close();
				ghc::filesystem::remove(filePath);
				errored.store(true);
				SetError();
				return;
			}
		}

		if (req4Kib != 256)
		{
			const s32 remainingBytes = reqSizeBytes - (((u64)iMiB) * (1024 * 1024) + req4Kib * 4096);
			newImage.write((char*)buff, remainingBytes);
			if (newImage.fail())
			{
				newImage.close();
				ghc::filesystem::remove(filePath);
				errored.store(true);
				SetError();
				return;
			}
		}

		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() >= 100 || (iMiB + 1) == reqMiB)
		{
			lastUpdate = now;
			SetFileProgress(newImage.tellp());
		}
		if (canceled.load())
		{
			newImage.close();
			ghc::filesystem::remove(filePath);
			errored.store(true);
			SetError();
			return;
		}
	}
	newImage.flush();
	newImage.close();
}

void HddCreate::SetFileProgress(u64 currentSize)
{
	Console.WriteLn(fmt::format("{} / {} Bytes", currentSize, neededSize).c_str());
}

void HddCreate::SetError()
{
	Console.WriteLn("Failed to create HDD file");
}

void HddCreate::SetCanceled()
{
	canceled.store(true);
}
