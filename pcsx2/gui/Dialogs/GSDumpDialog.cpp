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
#include "Utilities/pxStreams.h"
#include "Resources/NoIcon.h"
#include "GS.h"

#include "PathDefs.h"
#include "AppConfig.h"
#include "Plugins.h"

#include <wx/mstream.h>
#include <wx/listctrl.h>
#include <wx/filepicker.h>
#include <wx/radiobut.h>
#include <wx/button.h>
#include <wx/treectrl.h>
#include <wx/checkbox.h>
#include <wx/dir.h>
#include <wx/image.h>
#include <wx/wfstream.h>

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
	//TODO: switch all of that to a pxThread
	pxInputStream dump_file(*m_selected_dump, new wxFFileInputStream(*m_selected_dump));

	if (!dump_file.IsOk())
		return;

	char freeze_data[sizeof(int) * 2];
	u32 crc = 0, ss = 0;
	// TODO: get that from the GUI
	int renderer_override = 0;
	char regs[8192];

	dump_file.Read(&crc, 4);
	dump_file.Read(&ss, 4);


	char* state_data = (char*)malloc(sizeof(char) * ss);
	dump_file.Read(state_data, ss);
	dump_file.Read(&regs, 8192);

	int ssi = ss;
	freezeData fd = {0, (s8*)state_data};
	std::vector<GSData> dump;

	while (dump_file.Tell() <= dump_file.Length())
	{
		GSType id = Transfer;
		dump_file.Read(&id, 1);
		switch (id)
		{
			case Transfer:
			{
				GSTransferPath id_transfer;
				dump_file.Read(&id_transfer, 1);
				s32 size = 0;
				dump_file.Read(&size, 4);
				char* transfer_data = (char*)malloc(size);
				dump_file.Read(transfer_data, size);
				GSData data = {id, transfer_data, size, id_transfer};
				dump.push_back(data);
				break;
			}
			case VSync:
			{
				u8 vsync = 0;
				dump_file.Read(&vsync, 1);
				GSData data = {id, (char*)&vsync, 1, Dummy};
				dump.push_back(data);
				break;
			}
			case ReadFIFO2:
			{
				u32 fifo = 0;
				dump_file.Read(&fifo, 4);
				GSData data = {id, (char*)&fifo, 4, Dummy};
				dump.push_back(data);
				break;
			}
			case Registers:
			{
				char regs_tmp[8192];
				dump_file.Read(&regs, 8192);
				GSData data = {id, regs_tmp, 8192, Dummy};
				dump.push_back(data);
				break;
			}
		}
	}

	GSinit();
	GSsetBaseMem((void*)regs);
	if (GSopen2((void*)pDsp, renderer_override) != 0)
		return;

	GSsetGameCRC((int)crc, 0);


	if (GSfreeze(0, &fd) == -1)
	{
		//DumpTooOld = true;
		//Running = false;
	}
	GSvsync(1);
	GSreset();
	GSsetBaseMem((void*)regs);
	GSfreeze(0, &fd);


	while (0!=1)
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

void Dialogs::GSDumpDialog::ProcessDumpEvent(GSData event, char* regs)
{
    switch (event.id)
	{
		case Transfer:
		{
			switch (event.path)
			{
				case Path1Old:
				{
					u32* data = (u32*)malloc(16384);
					int addr = 16384 - event.length;
					memcpy(data, event.data + addr, event.length);
					GSgifTransfer1(data, addr);
					free(data);
					break;
				}
				case Path1New:
				{
					GSgifTransfer((u32*)event.data, event.length / 16);
					break;
				}
				case Path2:
				{
					GSgifTransfer2((u32*)event.data, event.length / 16);
					break;
				}
				case Path3:
				{
					GSgifTransfer3((u32*)event.data, event.length / 16);
					break;
				}
			}
			break;
		}
		case VSync:
		{
			GSvsync((*((int*)(regs + 4096)) & 0x2000) > 0 ? (u8)1 : (u8)0);
			break;
		}
		case ReadFIFO2:
		{
			u64* arr = (u64*)malloc(*((int*)event.data));
			GSreadFIFO2(arr, *((int*)event.data));
			free(arr);
			break;
		}
		case Registers:
		{	
			memcpy(regs, event.data, 8192);
			break;
		}
	}
}
