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

#if _WIN32
#include <Windows.h>
#elif defined(__POSIX__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

void HddCreate::Start()
{
	//This can be called from the EE Core thread
	//ensure that UI creation/deletaion is done on main thread
	if (!wxIsMainThread())
	{
		wxTheApp->CallAfter([&] { Start(); });
		//Block until done
		std::unique_lock competedLock(completedMutex);
		completedCV.wait(competedLock, [&] { return completed; });
		return;
	}

	//This creates a modeless dialog
	progressDialog = new wxProgressDialog("Creating HDD file", "Creating HDD file", neededSize, nullptr, wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME);

	int zeroSize = 1;
	fileThread = std::thread(&HddCreate::WriteImage, this, filePath, neededSize, zeroSize);

	//This code was written for a modal dialog, however wxProgressDialog is modeless only
	//The idea was block here in a ShowModal() call, and have the worker thread update the UI
	//via CallAfter()

	//Instead, loop here to update UI
	char msg[32] = {0};
	int currentSize;
	while ((currentSize = written.load()) != neededSize && !errored.load())
	{
		snprintf(msg, 32, "%i / %i MiB", written.load(), neededSize);

		if (!progressDialog->Update(currentSize, msg))
			canceled.store(true);

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	fileThread.join();

	if (errored.load())
	{
		wxMessageDialog dialog(nullptr, "Failed to create HDD file", "Info", wxOK);
		dialog.ShowModal();
	}

	delete progressDialog;
	//Signal calling thread to resume
	{
		std::lock_guard ioSignallock(completedMutex);
		completed = true;
	}
	completedCV.notify_all();
}

void HddCreate::WriteImage(ghc::filesystem::path hddPath, int fileMiB, int zeroMiB)
{
	if (ghc::filesystem::exists(hddPath))
	{
		SetError();
		return;
	}

	const u64 fileBytes = ((u64)fileMiB) * 1024 * 1024;
	bool sparseSupported = false;

#ifdef _WIN32
	//Create File
	HANDLE nativeFile = CreateFile(hddPath.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (nativeFile == INVALID_HANDLE_VALUE)
	{
		SetError();
		return;
	}

	//Do we support sparse files
	DWORD dwFlags;
	BOOL ret = GetVolumeInformationByHandleW(nativeFile, nullptr, 0, nullptr, nullptr, &dwFlags, nullptr, 0);

	if (ret == FALSE)
	{
		Console.Error("DEV9: failed to check sparse");
		CloseHandle(nativeFile);
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}

	if (dwFlags & FILE_SUPPORTS_SPARSE_FILES)
	{
		//Sparse files supported
		FILE_SET_SPARSE_BUFFER sparseSetting;
		sparseSetting.SetSparse = true;
		DWORD dwTemp;
		ret = DeviceIoControl(nativeFile, FSCTL_SET_SPARSE, &sparseSetting, sizeof(sparseSetting), nullptr, 0, &dwTemp, nullptr);

		if (ret == FALSE)
		{
			Console.Error("DEV9: Failed to set sparse");
			CloseHandle(nativeFile);
			ghc::filesystem::remove(filePath);
			SetError();
			return;
		}

		FILE_ZERO_DATA_INFORMATION sparseRange;
		sparseRange.FileOffset.QuadPart = 0;
		sparseRange.BeyondFinalZero.QuadPart = fileBytes;
		ret = DeviceIoControl(nativeFile, FSCTL_SET_ZERO_DATA, &sparseRange, sizeof(sparseRange), nullptr, 0, &dwTemp, nullptr);

		if (ret == FALSE)
		{
			Console.Error("DEV9: Failed to set sparse");
			CloseHandle(nativeFile);
			ghc::filesystem::remove(filePath);
			SetError();
			return;
		}

		sparseSupported = true;
	}

	//Set filesize
	LARGE_INTEGER seek;
	seek.QuadPart = fileBytes;
	ret = SetFilePointerEx(nativeFile, seek, nullptr, FILE_BEGIN);

	if (ret == FALSE)
	{
		Console.Error("DEV9: Failed to set size");
		CloseHandle(nativeFile);
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}

	ret = SetEndOfFile(nativeFile);

	if (ret == FALSE)
	{
		Console.Error("DEV9: Failed to set size");
		CloseHandle(nativeFile);
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}

	seek.QuadPart = 0;
	ret = SetFilePointerEx(nativeFile, seek, nullptr, FILE_BEGIN);

	if (ret == FALSE)
	{
		Console.Error("DEV9: Failed to seek");
		CloseHandle(nativeFile);
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}

#elif defined(__POSIX__)
	//Create File
	int nativeFile = open(hddPath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (nativeFile == -1)
	{
		SetError();
		return;
	}

	//Set filesize
	int ret = ftruncate(nativeFile, fileBytes);

	if (ret == -1)
	{
		Console.Error("DEV9: Failed to set size");
		close(nativeFile);
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}

	//We check the blocks allocated
	//Assume that we don't get a false positive from filesystems that only support ValidDataLength
	struct stat fileInfo;
	ret = fstat(nativeFile, &fileInfo);

	if (ret == -1)
	{
		Console.Error("DEV9: Failed to check sparse");
		close(nativeFile);
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}

	if (fileInfo.st_blocks != fileBytes / 512)
	{
		//Sparse files supported
		sparseSupported = true;
		//Sparse automatically
	}

	off_t retOff = lseek(nativeFile, 0, SEEK_SET);

	if (retOff == -1)
	{
		Console.Error("DEV9: Failed to seek");
		close(nativeFile);
		ghc::filesystem::remove(filePath);
		SetError();
		return;
	}
#endif

	lastUpdate = std::chrono::steady_clock::now();

	int iMiB = 0;
	if (sparseSupported)
		iMiB = fileMiB - zeroMiB;

	constexpr int buffsize = 4 * 1024;
	const u8 buff[buffsize]{0};
	for (; iMiB < fileMiB; iMiB++)
	{
		for (int ibuff = 0; ibuff < (1024 * 1024) / buffsize; ibuff++)
		{
#ifdef _WIN32
			BOOL success = WriteFile(nativeFile, buff, buffsize, nullptr, nullptr);
			if (success == FALSE)
			{
				CloseHandle(nativeFile);
				ghc::filesystem::remove(filePath);
				SetError();
				return;
			}
#elif defined(__POSIX__)
			ssize_t written = write(nativeFile, buff, buffsize);
			if (written != buffsize)
			{
				close(nativeFile);
				ghc::filesystem::remove(filePath);
				SetError();
				return;
			}
#endif
		}

		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() >= 100 || (iMiB + 1) == fileMiB)
		{
			lastUpdate = now;
			SetFileProgress(iMiB + 1);
		}
		if (canceled.load())
		{
#ifdef _WIN32
			CloseHandle(nativeFile);
#elif defined(__POSIX__)
			close(nativeFile);
#endif
			ghc::filesystem::remove(filePath);
			SetError();
			return;
		}
	}
#ifdef _WIN32
	FlushFileBuffers(nativeFile);
	CloseHandle(nativeFile);
#elif defined(__POSIX__)
	fsync(nativeFile);
	close(nativeFile);
#endif
}

void HddCreate::SetFileProgress(int currentSize)
{
	written.store(currentSize);
}

void HddCreate::SetError()
{
	errored.store(true);
}
