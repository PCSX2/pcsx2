/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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
#include "gui/App.h"
#include "gui/AppCommon.h"
#include "gui/MSWstuff.h"

#include "gui/Dialogs/ModalPopups.h"

#include "System/SysThreads.h"

#include "PathDefs.h"
#include "gui/AppConfig.h"

using namespace pxSizerFlags;
/* This dialog currently assumes the IPC server is started when launching a
 * game, as such we can allow the IPC Settings window to change the slot in a
 * volatile fashion so that it returns to the default but you can change it at
 * each restart of the emulator to allow for multiple emulator sessions.
 * If we change this behaviour we will need to change that accordingly.
 */

// --------------------------------------------------------------------------------------
//  IPCDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::IPCDialog::IPCDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, _("IPC Settings"), pxDialogFlags())
{
	wxTextCtrl* ipc_slot = new wxTextCtrl(this, wxID_ANY, wxString::Format(wxT("%u"), IPCSettings::slot), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	ipc_slot->Bind(wxEVT_TEXT_ENTER, &Dialogs::IPCDialog::OnConfirm, this);

	*this += new wxStaticText(this, wxID_ANY, _("IPC Slot"));
	*this += ipc_slot;
}

void Dialogs::IPCDialog::OnConfirm(wxCommandEvent& evt)
{
	wxTextCtrl* obj = static_cast<wxTextCtrl*>(evt.GetEventObject());
	if (obj != nullptr)
	{
		IPCSettings::slot = (unsigned int)atoi(obj->GetValue().ToUTF8().data());
		Destroy();
	}
}
