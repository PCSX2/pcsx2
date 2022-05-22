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

#include "common/FileSystem.h"

#include <fmt/format.h>
#include "HddCreate.h"

void HddCreate::Start()
{
	Init();
	WriteImage(filePath, neededSize);
	Cleanup();
}

void HddCreate::WriteImage(std::string hddPath, u64 reqSizeBytes)
{
	constexpr int buffsize = 4 * 1024;
	u8 buff[buffsize] = {0}; //4kb

	if (FileSystem::FileExists(hddPath.c_str()))
	{
		errored.store(true);
		SetError();
		return;
	}

	auto newImage = FileSystem::OpenManagedCFile(hddPath.c_str(), "wb");
	if (!newImage)
	{
		errored.store(true);
		SetError();
		return;
	}

	//Size file
	const char zero = 0;
	bool success = FileSystem::FSeek64(newImage.get(), reqSizeBytes - 1, SEEK_SET) == 0;
	success = success && std::fwrite(&zero, 1, 1, newImage.get()) == 1;
	success = success && FileSystem::FSeek64(newImage.get(), 0, SEEK_SET) == 0;

	if (!success)
	{
		newImage.reset();
		FileSystem::DeleteFilePath(filePath.c_str());
		errored.store(true);
		SetError();
		return;
	}

	lastUpdate = std::chrono::steady_clock::now();

	//Round up
	const s32 reqMiB = (reqSizeBytes + ((1024 * 1024) - 1)) / (1024 * 1024);
	for (s32 iMiB = 0; iMiB < reqMiB; iMiB++)
	{
		//Round down
		const s32 req4Kib = std::min<s32>(1024, (reqSizeBytes / 1024) - (u64)iMiB * 1024) / 4;
		for (s32 i4kb = 0; i4kb < req4Kib; i4kb++)
		{
			if (std::fwrite(buff, buffsize, 1, newImage.get()) != 1)
			{
				newImage.reset();
				FileSystem::DeleteFilePath(filePath.c_str());
				errored.store(true);
				SetError();
				return;
			}
		}

		if (req4Kib != 256)
		{
			const s32 remainingBytes = reqSizeBytes - (((u64)iMiB) * (1024 * 1024) + req4Kib * 4096);
			if (std::fwrite(buff, remainingBytes, 1, newImage.get()) != 1)
			{
				newImage.reset();
				FileSystem::DeleteFilePath(filePath.c_str());
				errored.store(true);
				SetError();
				return;
			}
		}

		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() >= 100 || (iMiB + 1) == reqMiB)
		{
			lastUpdate = now;
			SetFileProgress(static_cast<u64>(FileSystem::FTell64(newImage.get())));
		}
		if (canceled.load())
		{
			newImage.reset();
			FileSystem::DeleteFilePath(filePath.c_str());
			errored.store(true);
			SetError();
			return;
		}
	}
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
