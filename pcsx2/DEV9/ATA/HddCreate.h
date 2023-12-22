// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <atomic>
#include <chrono>
#include <string>

#include "common/Path.h"

class HddCreate
{
public:
	std::string filePath;
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
	void WriteImage(const std::string& hddPath, u64 fileBytes, u64 zeroSizeBytes);
};
