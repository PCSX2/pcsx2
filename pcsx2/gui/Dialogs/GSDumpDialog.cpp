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
	, m_debug_mode(new wxCheckBox(this, wxID_ANY, _("Debug Mode")))
	, m_renderer_overrides(new wxRadioBox())
	, m_gif_list(new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 200), wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS))
	, m_gif_packet(new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 200), wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS))
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

	wxArrayString rdoverrides;
	rdoverrides.Add("None");
	rdoverrides.Add("D3D11 HW");
	rdoverrides.Add("OGL HW");
	rdoverrides.Add("OGL SW");
	m_renderer_overrides->Create(this, wxID_ANY, "Renderer overrides", wxDefaultPosition, wxSize(300, 120), rdoverrides, 2);
	dump_info += m_renderer_overrides;
	dump_info += new wxButton(this, ID_RUN_DUMP, _("Run"));



	// debugger
	dbg_tree += new wxStaticText(this, wxID_ANY, _("GIF Packets"));
	dbg_tree += m_gif_list;
	dbg_actions += m_debug_mode;
	dbg_actions += new wxButton(this, ID_RUN_START, _("Go to Start"));
	dbg_actions += new wxButton(this, ID_RUN_STEP, _("Step"));
	dbg_actions += new wxButton(this, ID_RUN_CURSOR, _("Run to Selection"));
	dbg_actions += new wxButton(this, ID_RUN_VSYNC, _("Go to next VSync"));

	// gif
	gif += new wxStaticText(this, wxID_ANY, _("Packet Content"));
	gif += m_gif_packet;


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
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::ToStart, this, ID_RUN_START);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::StepPacket, this, ID_RUN_STEP);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::ToCursor, this, ID_RUN_CURSOR);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::ToVSync, this, ID_RUN_VSYNC);
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
	// XXX: check the numbers are correct
	int renderer_override = m_renderer_overrides->GetSelection();
	char regs[8192];

	dump_file.Read(&crc, 4);
	dump_file.Read(&ss, 4);


	char* state_data = (char*)malloc(sizeof(char) * ss);
	dump_file.Read(state_data, ss);
	dump_file.Read(&regs, 8192);

	int ssi = ss;
	freezeData fd = {0, (s8*)state_data};
	std::vector<GSData> dump;

	while (dump_file.Tell() < dump_file.Length())
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

	if (m_debug_mode->GetValue())
		GenPacketList(dump);

	return;

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

	size_t i = 0;
	int RunTo = 0;
	size_t debug_idx = 0;

	while (0!=1)
	{
		if (m_debug_mode->GetValue())
		{
			if (m_button_events.size() > 0)
			{
				switch (m_button_events[0].index)
				{
					case Step:
						if (debug_idx >= dump.size())
							debug_idx = 0;
						RunTo = debug_idx;
						break;
					case RunCursor:
						RunTo = m_button_events[0].index;
						if (debug_idx > RunTo)
							debug_idx = 0;
						break;
					case RunVSync:
						if (debug_idx >= dump.size())
							debug_idx = 1;
						auto it = std::find_if(dump.begin() + debug_idx, dump.end(), [](const GSData& gs) { return gs.id == Registers; });
						if (it != std::end(dump))
							RunTo = std::distance(dump.begin(), it);
						break;
				}
				m_button_events.erase(m_button_events.begin());

				if (debug_idx <= RunTo)
				{
					while (debug_idx <= RunTo)
					{
						ProcessDumpEvent(dump[debug_idx++], regs);
					}
					auto it = std::find_if(dump.begin() + debug_idx, dump.end(), [](const GSData& gs) { return gs.id == Registers; });
					if (it != std::end(dump))
						ProcessDumpEvent(*it, regs);

					debug_idx--;
				}

				// do vsync
				ProcessDumpEvent(GSData{VSync, 0, 0, Dummy}, regs);
			}
		}
		else
		{
			while (i < dump.size())
			{
				ProcessDumpEvent(dump[i++], regs);

				if (dump[i].id == VSync)
					break;
			}
			if (i >= dump.size())
				i = 0;
		}
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

void Dialogs::GSDumpDialog::StepPacket(wxCommandEvent& event)
{
	m_button_events.push_back(GSEvent{Step, 0});
}

void Dialogs::GSDumpDialog::ToCursor(wxCommandEvent& event)
{
	// TODO: modify 0 to wxTreeCtrl index
	m_button_events.push_back(GSEvent{RunCursor, 0});
}

void Dialogs::GSDumpDialog::ToVSync(wxCommandEvent& event)
{
	m_button_events.push_back(GSEvent{RunVSync, 0});
}

void Dialogs::GSDumpDialog::ToStart(wxCommandEvent& event)
{
	m_button_events.push_back(GSEvent{RunCursor, 0});
}

void Dialogs::GSDumpDialog::GenPacketList(std::vector<GSData>& dump)
{
	int i = 0;
	m_gif_list->DeleteAllItems();
	wxTreeItemId mainrootId = m_gif_list->AddRoot("root");
	wxTreeItemId rootId = m_gif_list->AppendItem(mainrootId, "VSync");
	for (auto& element : dump)
	{
		switch (element.id)
		{
			case Transfer:
			{
				switch (element.path)
				{
					case Path1Old:
					{
						wxString s;
						s.Printf("%d - Transfer - Path1Old - %d byte", i, element.length);
						m_gif_list->AppendItem(rootId, s);
						break;
					}
					case Path1New:
					{
						wxString s;
						s.Printf("%d - Transfer - Path1New - %d byte", i, element.length);
						m_gif_list->AppendItem(rootId, s);
						break;
					}
					case Path2:
					{
						wxString s;
						s.Printf("%d - Transfer - Path2 - %d byte", i, element.length);
						m_gif_list->AppendItem(rootId, s);
						break;
					}
					case Path3:
					{
						wxString s;
						s.Printf("%d - Transfer - Path3 - %d byte", i, element.length);
						m_gif_list->AppendItem(rootId, s);
						break;
					}
				}
				break;
			}
			case VSync:
			{
				wxString s;
				s.Printf("%d - VSync - %d byte", i, element.length);
				m_gif_list->SetItemText(rootId, s);
				rootId = m_gif_list->AppendItem(mainrootId, "VSync");
				break;
			}
			case ReadFIFO2:
			{
				wxString s;
				s.Printf("%d - ReadFIFO2 - %d byte", i, element.length);
				m_gif_list->AppendItem(rootId, s);
				break;
			}
			case Registers:
			{
				wxString s;
				s.Printf("%d - Registers - %d byte", i, element.length);
				m_gif_list->AppendItem(rootId, s);
				break;
			}
		}
		i++;
	}
	m_gif_list->Delete(rootId);
}