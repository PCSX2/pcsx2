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

#pragma once

#include <atomic>
#include <chrono>

#include "common/Path.h"

#include "ghc/filesystem.h"

class HddCreate
{
public:
	ghc::filesystem::path filePath;
	u64 neededSize;

	std::atomic_bool errored{false};

private:
	std::atomic_bool canceled{false};

	std::chrono::steady_clock::time_point lastUpdate;

public:
	HddCreate(){};

	void Start();

	virtual ~HddCreate(){};

protected:
	virtual void Init(){};
	virtual void Cleanup(){};
	virtual void SetFileProgress(u64 currentSize);
	virtual void SetError();
	void SetCanceled();

private:
	void WriteImage(ghc::filesystem::path hddPath, u64 reqSizeBytes);
};
