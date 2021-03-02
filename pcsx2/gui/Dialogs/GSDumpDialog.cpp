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
#include "App.h"
#include "AppCommon.h"
#include "MSWstuff.h"

#include "Dialogs/ModalPopups.h"


#include "Utilities/EmbeddedImage.h"
#include "Resources/NoIcon.h"
#include "GS.h"

#include "PathDefs.h"
#include "AppConfig.h"

#include <wx/mstream.h>
#include <wx/listctrl.h>
#include <wx/filepicker.h>
#include <wx/radiobut.h>
#include <wx/button.h>
#include <wx/treectrl.h>
#include <wx/checkbox.h>
#include <wx/dir.h>
#include <wx/image.h>

using namespace pxSizerFlags;

// --------------------------------------------------------------------------------------
//  GSDumpDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::GSDumpDialog::GSDumpDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, _("GSDumpGov"), pxDialogFlags())
	, m_dump_list(new wxListView(this, ID_DUMP_LIST, wxDefaultPosition, wxSize(250, 200)))
	, m_preview_image(new wxStaticBitmap(this, wxID_ANY, wxBitmap(EmbeddedImage<res_NoIcon>().Get())))
	, m_selected_dump(new wxString(""))
{
	const float scale = MSW_GetDPIScale();
	SetMinWidth(scale * 460);

	wxFlexGridSizer& general(*new wxFlexGridSizer(2, StdPadding, StdPadding));
	wxBoxSizer& dump_info(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dump_preview(*new wxBoxSizer(wxVERTICAL));
	wxFlexGridSizer& debugger(*new wxFlexGridSizer(2, StdPadding, StdPadding));
	wxBoxSizer& dumps(*new wxBoxSizer(wxHORIZONTAL));
	wxBoxSizer& dbg_tree(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dbg_actions(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& gif(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dumps_list(*new wxBoxSizer(wxVERTICAL));

	dump_info += new wxRadioButton(this, wxID_ANY, _("None"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("D3D11 HW"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("OGL HW"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("OGL SW"));
	dump_info += new wxButton(this, ID_RUN_DUMP, _("Run"));



	// debugger
	dbg_tree += new wxStaticText(this, wxID_ANY, _("GIF Packets"));
	dbg_tree += new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 200));
	dbg_actions += new wxCheckBox(this, wxID_ANY, _("Debug Mode"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Go to Start"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Step"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Run to Selection"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Go to next VSync"));

	// gif
	gif += new wxStaticText(this, wxID_ANY, _("Packet Content"));
	gif += new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 200));


	debugger += dbg_tree;
	debugger += dbg_actions;

	GetDumpsList();

	dumps_list += new wxStaticText(this, wxID_ANY, _("GS Dumps List"));
	dumps_list += m_dump_list;

	dump_preview += new wxStaticText(this, wxID_ANY, _("Preview"));
	dump_preview += m_preview_image;


	dumps += dumps_list;
	dumps += dump_info;
	dumps += dump_preview;

	general += dumps;
	general += dump_info;
	general += debugger;
	general += gif;

	*this += general;

	SetSizerAndFit(GetSizer());

	Bind(wxEVT_LIST_ITEM_SELECTED, &Dialogs::GSDumpDialog::SelectedDump, this, ID_DUMP_LIST);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::RunDump, this, ID_RUN_DUMP);
}

void Dialogs::GSDumpDialog::GetDumpsList()
{
	wxDir snaps(g_Conf->Folders.Snapshots.ToAscii());
	wxString filename;
	bool cont = snaps.GetFirst(&filename, "*.gs", wxDIR_DEFAULT);
	int i = 0;
	m_dump_list->AppendColumn("Dumps");
	while (cont)
	{
		m_dump_list->InsertItem(i, filename.substr(0, filename.find_last_of(".")));
		i++;
		cont = snaps.GetNext(&filename);
	}
}

void Dialogs::GSDumpDialog::SelectedDump(wxListEvent& evt)
{
	wxString filename_preview = g_Conf->Folders.Snapshots.ToAscii() + ("/" + evt.GetText()) + ".png";
	wxString filename = g_Conf->Folders.Snapshots.ToAscii() + ("/" + evt.GetText()) + ".gs";
	if (wxFileExists(filename_preview))
	{
		auto img = wxImage(filename_preview);
		img.Rescale(250 * MSW_GetDPIScale(), 200 * MSW_GetDPIScale(), wxIMAGE_QUALITY_HIGH);
		m_preview_image->SetBitmap(wxBitmap(img));
		delete m_selected_dump;
		m_selected_dump = new wxString(filename);
	}
	else
	{
		m_preview_image->SetBitmap(EmbeddedImage<res_NoIcon>().Get());
	}
}

void Dialogs::GSDumpDialog::RunDump(wxCommandEvent& event)
{
	GSinit();
	GSsetBaseMem(/*dump regs*/);
	if (GSopen(new IntPtr(&hWnd), "", rendererOverride) != 0)
		return;
	GSsetGameCRC(dump.CRC, 0);
	if (GSfreeze(0, /*freeze_dump*/) == -1)
	{
		DumpTooOld = true;
		Running = false;
	}
	GSVSync(1);
	GSreset();
	GSsetBaseMem(/*dump regs*/);
	GSfreeze(0, /*freeze_dump*/);


	while (Running)
	{
		/* First listen to keys:
		   case 0x1B: Running = false; break; // VK_ESCAPE;
           case 0x77: GSmakeSnapshot(""); break; // VK_F8;
		*/

		/* if DebugMode handle buttons, else:*/

		/*
		   while (gs_idx < dump.Data.Count)
           {
               GSData itm = dump.Data[gs_idx++];
               CurrentGIFPacket = itm;
               Step(itm, pointer);

               if (gs_idx < dump.Data.Count && dump.Data[gs_idx].id == GSType.VSync)
                  break;
               }

               gs_idx = 0;
			}
		*/
	}

	GSclose();
	GSshutdown();
}