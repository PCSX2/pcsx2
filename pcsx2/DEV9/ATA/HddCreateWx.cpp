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

#include "HddCreateWx.h"
#include "gui/App.h"

void HddCreateWx::Init()
{
	//This can be called from the EE Core thread
	//ensure that UI creation/deletaion is done on main thread
	//Also block calling thread untill ui is ready
	if (!wxIsMainThread())
	{
		dialogReady = false;
		wxTheApp->CallAfter([&] { Init(); });
		//Block until done
		std::unique_lock loadLock(dialogMutex);
		dialogCV.wait(loadLock, [&] { return dialogReady; });
		return;
	}

	reqMiB = (neededSize + ((1024 * 1024) - 1)) / (1024 * 1024);
	//This creates a modeless dialog
	progressDialog = new wxProgressDialog(_("Creating HDD file"), _("Creating HDD file"), reqMiB, nullptr, wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME);
	{
		std::lock_guard dialogLock1(dialogMutex);
		dialogReady = true;
	}
	dialogCV.notify_all();
}

void HddCreateWx::SetFileProgress(u64 currentSize)
{
	if (!wxIsMainThread())
	{
		wxTheApp->CallAfter([&, currentSize] { SetFileProgress(currentSize); });
		return;
	}

	wxString msg;

	const int writtenMB = (currentSize + ((1024 * 1024) - 1)) / (1024 * 1024);
	msg.Printf(_("%i / %i MiB"), writtenMB, reqMiB);

	if (!progressDialog->Update(writtenMB, msg))
		SetCanceled();
}

void HddCreateWx::SetError()
{
	if (!wxIsMainThread())
	{
		dialogReady = false;
		wxTheApp->CallAfter([&] { SetError(); });
		//Block until done
		std::unique_lock loadLock(dialogMutex);
		dialogCV.wait(loadLock, [&] { return dialogReady; });
		return;
	}

	wxMessageDialog dialog(nullptr, _("Failed to create HDD file"), _("Info"), wxOK);
	dialog.ShowModal();
	{
		std::lock_guard dialogLock1(dialogMutex);
		dialogReady = true;
	}
	dialogCV.notify_all();
}

void HddCreateWx::Cleanup()
{
	if (!wxIsMainThread())
	{
		dialogReady = false;
		wxTheApp->CallAfter([&] { Cleanup(); });
		//Block until done
		std::unique_lock loadLock(dialogMutex);
		dialogCV.wait(loadLock, [&] { return dialogReady; });
		return;
	}

	delete progressDialog;

	{
		std::lock_guard dialogLock1(dialogMutex);
		dialogReady = true;
	}
	dialogCV.notify_all();
}
