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

	fileThread = std::thread(&HddCreate::WriteImage, this, filePath, neededSize);

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

		const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count() >= 100 || (iMiB + 1) == neededSize)
		{
			lastUpdate = now;
			SetFileProgress(iMiB + 1);
		}
		if (canceled.load())
		{
			newImage.close();
			ghc::filesystem::remove(filePath);
			SetError();
			return;
		}
	}
	newImage.flush();
	newImage.close();
}

void HddCreate::SetFileProgress(int currentSize)
{
	written.store(currentSize);
}

void HddCreate::SetError()
{
	errored.store(true);
}
