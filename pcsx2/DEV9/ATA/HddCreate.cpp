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
#include "HddCreate.h"

void HddCreate::Start()
{
	fileThread = std::thread(&HddCreate::WriteImage, this, filePath, neededSize);
	fileThread.join();
}

void HddCreate::WriteImage(ghc::filesystem::path hddPath, int reqSizeMiB)
{
	constexpr int buffsize = 4 * 1024;
	u8 buff[buffsize] = {0}; //4kb

	if (ghc::filesystem::exists(hddPath))
	{
		SetError();
		return;
	}

	std::fstream newImage = ghc::filesystem::fstream(hddPath, std::ios::out | std::ios::binary);

	if (newImage.fail())
	{
		SetError();
		return;
	}

	//Size file
	newImage.seekp(((u64)reqSizeMiB) * 1024 * 1024 - 1, std::ios::beg);
	const char zero = 0;
	newImage.write(&zero, 1);

	if (newImage.fail())
	{
		newImage.close();
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}

	lastUpdate = std::chrono::steady_clock::now();

	newImage.seekp(0, std::ios::beg);

	for (int iMiB = 0; iMiB < reqSizeMiB; iMiB++)
	{
		for (int i4kb = 0; i4kb < 256; i4kb++)
		{
			newImage.write((char*)buff, buffsize);
			if (newImage.fail())
			{
				newImage.close();
				ghc::filesystem::remove(filePath);
				SetError();
				return;
			}
		}
		SetFileProgress(iMiB + 1);
	}
	newImage.flush();
	newImage.close();

	SetDone();
}

void HddCreate::SetFileProgress(int currentSize)
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count() >= 1)
	{
		lastUpdate = now;
		fprintf(stdout, "%i / %i MiB\n", currentSize, neededSize);
	}
}

void HddCreate::SetError()
{
	fprintf(stderr, "Unable to create file\n");
	errored.store(true);
	completed.store(true);
}

void HddCreate::SetDone()
{
	fprintf(stdout, "%i / %i MiB\n", neededSize, neededSize);
	fprintf(stdout, "Done\n");
	completed.store(true);
}