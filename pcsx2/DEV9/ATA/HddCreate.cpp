// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/FileSystem.h"

#include <fmt/format.h>
#include "HddCreate.h"

#if _WIN32
#include "common/RedtapeWindows.h"
#include <winioctl.h>
#include <io.h>
#elif defined(__POSIX__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include "common/Console.h"
#include "common/StringUtil.h"

void HddCreate::Start()
{
	Init();
	WriteImage(filePath, neededSize, 1024);
	Cleanup();
}

void HddCreate::WriteImage(const std::string& hddPath, u64 fileBytes, u64 zeroSizeBytes)
{
	constexpr int buffsize = 4 * 1024;
	const u8 buff[buffsize] = {0}; // 4kb.

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

	bool sparseSupported = false;
#ifdef _WIN32
	// Handle owned by CFile.
	HANDLE nativeFile = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(newImage.get())));

	if (nativeFile == INVALID_HANDLE_VALUE)
	{
		Console.Error("DEV9: HddCreate: failed to get handle");
		newImage.reset();
		FileSystem::DeleteFilePath(hddPath.c_str());
		errored.store(true);
		SetError();
		return;
	}

	// Check if we support sparse files.
	DWORD dwFlags;
	if (GetVolumeInformationByHandleW(nativeFile, nullptr, 0, nullptr, nullptr, &dwFlags, nullptr, 0) == FALSE)
	{
		Console.Error("DEV9: HddCreate: failed to check sparse");
		newImage.reset();
		FileSystem::DeleteFilePath(hddPath.c_str());
		errored.store(true);
		SetError();
		return;
	}

	if (dwFlags & FILE_SUPPORTS_SPARSE_FILES)
	{
		// Sparse files supported.
		FILE_SET_SPARSE_BUFFER sparseSetting;
		sparseSetting.SetSparse = true;
		FILE_ZERO_DATA_INFORMATION sparseRange;
		sparseRange.FileOffset.QuadPart = 0;
		sparseRange.BeyondFinalZero.QuadPart = fileBytes;
		DWORD dwTemp;

		if ((DeviceIoControl(nativeFile, FSCTL_SET_SPARSE, &sparseSetting, sizeof(sparseSetting), nullptr, 0, &dwTemp, nullptr) == FALSE) ||
			(DeviceIoControl(nativeFile, FSCTL_SET_ZERO_DATA, &sparseRange, sizeof(sparseRange), nullptr, 0, &dwTemp, nullptr) == FALSE))
		{
			Console.Error("DEV9: HddCreate: Failed to set sparse");
			newImage.reset();
			FileSystem::DeleteFilePath(hddPath.c_str());
			errored.store(true);
			SetError();
			return;
		}

		sparseSupported = true;
	}

	// Set filesize.
	LARGE_INTEGER seekStart;
	seekStart.QuadPart = 0;
	LARGE_INTEGER seekEnd;
	seekEnd.QuadPart = fileBytes;

	if ((SetFilePointerEx(nativeFile, seekEnd, nullptr, FILE_BEGIN) == FALSE) ||
		(SetEndOfFile(nativeFile) == FALSE) ||
		(SetFilePointerEx(nativeFile, seekStart, nullptr, FILE_BEGIN) == FALSE))
	{
		Console.Error("DEV9: HddCreate: Failed to set size");
		newImage.reset();
		FileSystem::DeleteFilePath(hddPath.c_str());
		errored.store(true);
		SetError();
		return;
	}

#elif defined(__POSIX__)
	// Handle owned by CFile.
	int nativeFile = fileno(newImage.get());

	if (nativeFile == -1)
	{
		Console.Error("DEV9: HddCreate: failed to get handle");
		newImage.reset();
		FileSystem::DeleteFilePath(hddPath.c_str());
		errored.store(true);
		SetError();
		return;
	}

	// Set filesize.
	if ((ftruncate(nativeFile, fileBytes) == -1) ||
		(lseek(nativeFile, 0, SEEK_SET) == -1))
	{
		Console.Error("DEV9: HddCreate: Failed to set size");
		newImage.reset();
		FileSystem::DeleteFilePath(hddPath.c_str());
		errored.store(true);
		SetError();
		return;
	}

	// Check the blocks allocated to determine if file is spasre.
	// Assume that we don't get a false positive from filesystems only supporting ValidDataLength.
	struct stat fileInfo;
	if (fstat(nativeFile, &fileInfo) == -1)
	{
		Console.Error("DEV9: HddCreate: Failed to check sparse");
		// Set filesize to zero to avoid potential freeze on close.
		// Ignore any error, can't do much if this fails anyway.
		[[maybe_unused]] int i = ftruncate(nativeFile, 0);
		newImage.reset();
		FileSystem::DeleteFilePath(hddPath.c_str());
		errored.store(true);
		SetError();
		return;
	}

	if (fileInfo.st_blocks != static_cast<s64>(fileBytes / 512))
	{
		// Sparse files supported.
		sparseSupported = true;
		// File is automatically sparse.
	}
#endif

	lastUpdate = std::chrono::steady_clock::now();

	// Round up.
	const s32 reqMiB = (fileBytes + ((1024 * 1024) - 1)) / (1024 * 1024);
	const s32 zeroMiB = (zeroSizeBytes + ((1024 * 1024) - 1)) / (1024 * 1024);

	s32 iMiB = 0;
	if (sparseSupported)
		iMiB = reqMiB - zeroMiB;

	for (; iMiB < reqMiB; iMiB++)
	{
		// Round down.
		const s32 req4Kib = std::min<s32>(1024, (fileBytes / 1024) - (u64)iMiB * 1024) / 4;
		for (s32 i4kb = 0; i4kb < req4Kib; i4kb++)
		{
			if (std::fwrite(buff, buffsize, 1, newImage.get()) != 1)
			{
				std::fflush(newImage.get());
				// Set filesize to zero to avoid potential freeze on close.
#ifdef _WIN32
				SetFilePointerEx(nativeFile, seekStart, nullptr, FILE_BEGIN);
				SetEndOfFile(nativeFile);
#elif defined(__POSIX__)
				// Ignore any error, can't do much if this fails anyway.
				[[maybe_unused]] int i = ftruncate(nativeFile, 0);
#endif
				newImage.reset();
				FileSystem::DeleteFilePath(hddPath.c_str());
				errored.store(true);
				SetError();
				return;
			}
		}

		if (req4Kib != 256)
		{
			const s32 remainingBytes = fileBytes - (((u64)iMiB) * (1024 * 1024) + req4Kib * 4096);
			if (std::fwrite(buff, remainingBytes, 1, newImage.get()) != 1)
			{
				std::fflush(newImage.get());
				// Set filesize to zero to avoid potential freeze on close.
#ifdef _WIN32
				SetFilePointerEx(nativeFile, seekStart, nullptr, FILE_BEGIN);
				SetEndOfFile(nativeFile);
#elif defined(__POSIX__)
				// Ignore any error, can't do much if this fails anyway.
				[[maybe_unused]] int i = ftruncate(nativeFile, 0);
#endif
				newImage.reset();
				FileSystem::DeleteFilePath(hddPath.c_str());
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
			std::fflush(newImage.get());
			// Set filesize to zero to avoid potential freeze on close.
#ifdef _WIN32
			SetFilePointerEx(nativeFile, seekStart, nullptr, FILE_BEGIN);
			SetEndOfFile(nativeFile);
#elif defined(__POSIX__)
			// Ignore any error, can't do much if this fails anyway.
			[[maybe_unused]] int i = ftruncate(nativeFile, 0);
#endif
			newImage.reset();
			FileSystem::DeleteFilePath(hddPath.c_str());
			errored.store(true);
			SetError();
			return;
		}
	}
}

void HddCreate::SetFileProgress(u64 currentSize)
{
	Console.WriteLn(fmt::format("DEV9: HddCreate: {} / {} Bytes", currentSize, neededSize).c_str());
}

void HddCreate::SetError()
{
	Console.WriteLn("DEV9: HddCreate: Failed to create HDD file");
}

void HddCreate::SetCanceled()
{
	canceled.store(true);
}
