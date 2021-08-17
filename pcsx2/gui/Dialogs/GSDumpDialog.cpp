/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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


#include "common/EmbeddedImage.h"
#include "gui/Resources/NoIcon.h"
#include "GS.h"

#include "PathDefs.h"
#include "gui/AppConfig.h"
#include "gui/GSFrame.h"
#include "Counters.h"

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
#include <functional>

namespace GSDump
{
	bool isRunning = false;
}

wxDEFINE_EVENT(EVT_CLOSE_DUMP, wxCommandEvent);

using namespace pxSizerFlags;

// --------------------------------------------------------------------------------------
//  GSDumpDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::GSDumpDialog::GSDumpDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, _("GS Debugger"), pxDialogFlags().SetMinimize())
	, m_dump_list(new wxListView(this, ID_DUMP_LIST, wxDefaultPosition, wxSize(400, 300), wxLC_NO_HEADER | wxLC_REPORT | wxLC_SINGLE_SEL))
	, m_preview_image(new wxStaticBitmap(this, wxID_ANY, wxBitmap(EmbeddedImage<res_NoIcon>().Get()), wxDefaultPosition, wxSize(400,250)))
	, m_debug_mode(new wxCheckBox(this, ID_DEBUG_MODE, _("Debug Mode")))
	, m_renderer_overrides(new wxRadioBox())
	, m_gif_list(new wxTreeCtrl(this, ID_SEL_PACKET, wxDefaultPosition, wxSize(400, 300), wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT))
	, m_gif_packet(new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(400, 300), wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT))
	, m_start(new wxButton(this, ID_RUN_START, _("Go to Start"), wxDefaultPosition, wxSize(150,50)))
	, m_step(new wxButton(this, ID_RUN_STEP, _("Step"), wxDefaultPosition, wxSize(150, 50)))
	, m_selection(new wxButton(this, ID_RUN_CURSOR, _("Run to Selection"), wxDefaultPosition, wxSize(150, 50)))
	, m_vsync(new wxButton(this, ID_RUN_VSYNC, _("Go to next VSync"), wxDefaultPosition, wxSize(150, 50)))
	, m_settings(new wxButton(this, ID_SETTINGS, _("Open GS Settings"), wxDefaultPosition, wxSize(150, 50)))
	, m_run(new wxButton(this, ID_RUN_DUMP, _("Run"), wxDefaultPosition, wxSize(150, 50)))
	, m_thread(std::make_unique<GSThread>(this))
{
	wxBoxSizer* dump_info = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dump_preview = new wxBoxSizer(wxVERTICAL);
	wxFlexGridSizer* debugger = new wxFlexGridSizer(3, StdPadding, StdPadding);
	wxBoxSizer* dumps = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer* dbg_tree = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dbg_actions = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* gif = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dumps_list = new wxBoxSizer(wxVERTICAL);

	m_run->SetDefault();
	wxArrayString rdoverrides;
	rdoverrides.Add("None");
	rdoverrides.Add("OGL SW");
	rdoverrides.Add("D3D11 HW");
	rdoverrides.Add("OGL HW");
	m_renderer_overrides->Create(this, wxID_ANY, "Renderer overrides", wxDefaultPosition, wxDefaultSize, rdoverrides, 1);

	dbg_tree->Add(new wxStaticText(this, wxID_ANY, _("GIF Packets")));
	dbg_tree->Add(m_gif_list, StdExpand());
	dbg_actions->Add(m_debug_mode);
	dbg_actions->Add(m_start, StdExpand());
	dbg_actions->Add(m_step, StdExpand());
	dbg_actions->Add(m_selection, StdExpand());
	dbg_actions->Add(m_vsync, StdExpand());
	gif->Add(new wxStaticText(this, wxID_ANY, _("Packet Content")));
	gif->Add(m_gif_packet, StdExpand());

	dumps_list->Add(new wxStaticText(this, wxID_ANY, _("GS Dumps List")), StdExpand());
	dumps_list->Add(m_dump_list, StdExpand());
	dump_info->Add(m_renderer_overrides, StdExpand());
	dump_info->Add(m_settings, StdExpand());
	dump_info->Add(m_run, StdExpand());
	dump_preview->Add(new wxStaticText(this, wxID_ANY, _("Preview")), StdExpand());
	dump_preview->Add(m_preview_image, StdCenter());

	dumps->Add(dumps_list);
	dumps->Add(dump_info);
	dumps->Add(dump_preview);

	debugger->Add(dbg_tree);
	debugger->Add(dbg_actions);
	debugger->Add(gif);
	
	*this += dumps;
	*this += debugger;

	// populate UI and setup state
	m_debug_mode->Disable();
	m_start->Disable();
	m_step->Disable();
	m_selection->Disable();
	m_vsync->Disable();
	GetDumpsList();

	m_fs_watcher.SetOwner(this);
	m_fs_watcher.Add(wxFileName(g_Conf->Folders.Snapshots.ToAscii()));
	wxEvtHandler::Connect(wxEVT_FSWATCHER, wxFileSystemWatcherEventHandler(Dialogs::GSDumpDialog::PathChanged));

	Bind(wxEVT_LIST_ITEM_SELECTED, &Dialogs::GSDumpDialog::SelectedDump, this, ID_DUMP_LIST);
	Bind(wxEVT_LIST_ITEM_ACTIVATED, &Dialogs::GSDumpDialog::RunDump, this, ID_DUMP_LIST);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::RunDump, this, ID_RUN_DUMP);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::ToStart, this, ID_RUN_START);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::StepPacket, this, ID_RUN_STEP);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::ToCursor, this, ID_RUN_CURSOR);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::ToVSync, this, ID_RUN_VSYNC);
	Bind(wxEVT_BUTTON, &Dialogs::GSDumpDialog::OpenSettings, this, ID_SETTINGS);
	Bind(wxEVT_TREE_SEL_CHANGED, &Dialogs::GSDumpDialog::ParsePacket, this, ID_SEL_PACKET);
	Bind(wxEVT_CHECKBOX, &Dialogs::GSDumpDialog::CheckDebug, this, ID_DEBUG_MODE);
	Bind(EVT_CLOSE_DUMP, &Dialogs::GSDumpDialog::CloseDump, this);
}

void Dialogs::GSDumpDialog::GetDumpsList()
{
	m_dump_list->ClearAll();
	wxDir snaps(g_Conf->Folders.Snapshots.ToAscii());
	wxString filename;
	bool cont = snaps.GetFirst(&filename, "*.gs", wxDIR_DEFAULT);
	int i = 0, h = 0, j = 0;
	m_dump_list->AppendColumn("Dumps");
	// set the column size to be exactly of the size of our list
	m_dump_list->GetSize(&h, &j);
	m_dump_list->SetColumnWidth(0, h);

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
		img.Rescale(400,250, wxIMAGE_QUALITY_HIGH);
		m_preview_image->SetBitmap(wxBitmap(img));
	}
	else
		m_preview_image->SetBitmap(EmbeddedImage<res_NoIcon>().Get());
	m_selected_dump = wxString(filename);
}

void Dialogs::GSDumpDialog::PathChanged(wxFileSystemWatcherEvent& event)
{
	int type = event.GetChangeType();

	if (type == wxFSW_EVENT_CREATE || type == wxFSW_EVENT_DELETE || type == wxFSW_EVENT_RENAME)
		GetDumpsList();
}

void Dialogs::GSDumpDialog::CloseDump(wxCommandEvent& event)
{
	m_debug_mode->Disable();
	m_start->Disable();
	m_step->Disable();
	m_selection->Disable();
	m_vsync->Disable();
	m_gif_list->DeleteAllItems();
	m_gif_packet->DeleteAllItems();
	m_debug_mode->SetValue(false);
	m_run->Enable();
}

// --------------------------------------------------------------------------------------
//  GSDumpDialog GUI Buttons
// --------------------------------------------------------------------------------------

void Dialogs::GSDumpDialog::RunDump(wxCommandEvent& event)
{
	if (!m_run->IsEnabled())
		return;
	m_thread->m_dump_file = std::make_unique<pxInputStream>(m_selected_dump, new wxFFileInputStream(m_selected_dump));

	if (!(m_thread->m_dump_file)->IsOk())
	{
		wxString s;
		s.Printf(_("Failed to load the dump %s !"), m_selected_dump);
		wxMessageBox(s, _("GS Debugger"), wxICON_ERROR);
		return;
	}
	m_run->Disable();
	m_debug_mode->Enable();
	m_thread->m_renderer = m_renderer_overrides->GetSelection();
	m_thread->Start();
	return;
}

void Dialogs::GSDumpDialog::CheckDebug(wxCommandEvent& event)
{
	if (m_debug_mode->GetValue())
	{
		GenPacketList();
		m_start->Enable();
		m_step->Enable();
		m_selection->Enable();
		m_vsync->Enable();
	}
	else
	{
		m_gif_list->DeleteAllItems();
		m_gif_packet->DeleteAllItems();
		m_gif_list->Refresh();
		m_start->Disable();
		m_step->Disable();
		m_selection->Disable();
		m_vsync->Disable();
	}
	m_thread->m_debug = m_debug_mode->GetValue();
}

void Dialogs::GSDumpDialog::StepPacket(wxCommandEvent& event)
{
	if (m_thread->m_debug_index < m_gif_items.size() - 1)
	{
		m_gif_list->SelectItem(m_gif_items[m_thread->m_debug_index + 1]);
		m_button_events.push_back(GSEvent{Step, 0});
	}
}

void Dialogs::GSDumpDialog::ToCursor(wxCommandEvent& event)
{
	m_button_events.push_back(GSEvent{RunCursor, wxAtoi(m_gif_list->GetItemText(m_gif_list->GetFocusedItem()).BeforeFirst('-'))});
}

void Dialogs::GSDumpDialog::ToVSync(wxCommandEvent& event)
{
	if (m_thread->m_debug_index < m_gif_items.size() - 1)
	{
		wxTreeItemId pkt = m_gif_items[m_thread->m_debug_index];
		if (!m_gif_list->ItemHasChildren(pkt))
			pkt = m_gif_list->GetItemParent(pkt);
		if (m_gif_list->GetNextSibling(pkt).IsOk())
			m_gif_list->SelectItem(m_gif_list->GetNextSibling(pkt));
		m_button_events.push_back(GSEvent{RunVSync, 0});
	}
}

void Dialogs::GSDumpDialog::OpenSettings(wxCommandEvent& event)
{
	GSconfigure();
}

void Dialogs::GSDumpDialog::ToStart(wxCommandEvent& event)
{
	m_gif_list->SelectItem(m_gif_items[0]);
	m_button_events.push_back(GSEvent{RunCursor, 0});
}

// --------------------------------------------------------------------------------------
//  GSDumpDialog Packet Parsing
// --------------------------------------------------------------------------------------

void Dialogs::GSDumpDialog::GenPacketList()
{
	int i = 0;
	m_gif_list->DeleteAllItems();
	m_gif_items.clear();
	wxTreeItemId mainrootId = m_gif_list->AddRoot("root");
	wxTreeItemId rootId = m_gif_list->AppendItem(mainrootId, "0 - VSync");
	for (auto& element : m_dump_packets)
	{
		wxString s, t;
		element.id == Transfer ? t.Printf(" - %s", GSTransferPathNames[element.path]) : t.Printf("");
		s.Printf("%d - %s%s - %d byte", i, GSTypeNames[element.id], t, element.length);
		if (element.id == VSync)
		{
			m_gif_list->SetItemText(rootId, s);
			rootId = m_gif_list->AppendItem(mainrootId, "VSync");
			m_gif_items.push_back(rootId);
		}
		else
		{
			wxTreeItemId tmp = m_gif_list->AppendItem(rootId, s);
			m_gif_items.push_back(tmp);
		}

		i++;
	}
	m_gif_list->Delete(rootId);
	m_gif_list->SelectItem(m_gif_items[0]);
}

void Dialogs::GSDumpDialog::GenPacketInfo(GSData& dump)
{
	m_gif_packet->DeleteAllItems();
	wxTreeItemId rootId = m_gif_packet->AddRoot("root");
	switch (dump.id)
	{
		case Transfer:
		{
			wxTreeItemId trootId;
			wxString s;
			s.Printf("Transfer Path %s", GSTransferPathNames[dump.path]);
			trootId = m_gif_packet->AppendItem(rootId, s);
			u64 tag = *(u64*)(dump.data.get());
			u64 regs = *(u64*)(dump.data.get() + 8);
			u32 nloop = tag & ((1 << 15) - 1);
			u8 eop = (tag >> 15) & 1;
			u8 pre = (tag >> 46) & 1;
			u32 prim = (tag >> 47) & ((1 << 11) - 1);
			u8 flg = ((tag >> 58) & 3);
			u32 nreg = (u32)((tag >> 60) & ((1 << 4) - 1));
			if (nreg == 0)
				nreg = 16;

			std::vector<wxString> infos(7);
			m_stored_q = 1.0;

			infos[0].Printf("nloop = %u", nloop);
			infos[1].Printf("eop = %u", eop);
			infos[2].Printf("flg = %s", GifFlagNames[flg]);
			infos[3].Printf("pre = %u", pre);
			infos[4].Printf("Prim");
			infos[5].Printf("nreg = %u", nreg);
			infos[6].Printf("reg");

			wxTreeItemId primId;
			wxTreeItemId regId;
			for (int i = 0; i < 7; i++)
			{
				wxTreeItemId res = m_gif_packet->AppendItem(trootId, infos[i]);
				switch (i)
				{
					case 4:
						ParseTreePrim(res, prim);
						break;
					case 6:
						regId = res;
						break;
				}
			}

			int p = 16;
			switch ((GifFlag)flg)
			{
				case GIF_FLG_PACKED:
				{
					for (u32 j = 0; j < nloop; j++)
					{
						for (u32 i = 0; i < nreg; i++)
						{
							u128 reg_data;
							reg_data.lo =  *(u64*)(dump.data.get() + p);
							reg_data.hi =  *(u64*)(dump.data.get() + p + 8);
							ParseTreeReg(regId, (GIFReg)((regs >> (i * 4)) & ((u64)(1 << 4) - 1)), reg_data, true);
							p += 16;
						}
					}
					break;
				}
				case GIF_FLG_REGLIST:
				{
					for (u32 j = 0; j < nloop; j++)
					{
						for (u32 i = 0; i < nreg; i++)
						{
							u128 reg_data;
							reg_data.lo =  *(u64*)(dump.data.get() + p);
							ParseTreeReg(regId, (GIFReg)((regs >> (i * 4)) & ((u64)(1 << 4) - 1)), reg_data, false);
							p += 8;
						}
					}
					break;
				}
				case GIF_FLG_IMAGE:
				case GIF_FLG_IMAGE2:
				{
					wxString z;
					s.Printf("IMAGE %d bytes", nloop * 16);
					m_gif_packet->AppendItem(regId, z);
					break;
				}
			}
			break;
		}
		case VSync:
		{
			wxString s;
			s.Printf("Field = %u", *(u8*)(dump.data.get()));
			m_gif_packet->AppendItem(rootId, s);
			break;
		}
		case ReadFIFO2:
		{
			wxString s;
			s.Printf("ReadFIFO2: Size = %d byte", dump.length);
			m_gif_packet->AppendItem(rootId, s);
			break;
		}
		case Registers:
			m_gif_packet->AppendItem(rootId, "Registers");
			break;
	}
	m_gif_packet->ExpandAll();
}

void Dialogs::GSDumpDialog::ParsePacket(wxTreeEvent& event)
{
	GenPacketInfo(m_dump_packets[wxAtoi(m_gif_list->GetItemText(event.GetItem()).BeforeFirst('-'))]);
}

void Dialogs::GSDumpDialog::ParseTreeReg(wxTreeItemId& id, GIFReg reg, u128 data, bool packed)
{
	wxTreeItemId rootId = m_gif_packet->AppendItem(id, wxString(GIFRegName(reg)));
	switch (reg)
	{
		case PRIM:
			ParseTreePrim(rootId, data.lo);
			break;
		case RGBAQ:
		{
			std::vector<wxString> rgb_infos(5);

			if (packed)
			{
				rgb_infos[0].Printf("R = %u", (u32)(data.lo & ((u64)(1 << 8) - 1)));
				rgb_infos[1].Printf("G = %u", (u32)((data.lo >> 32) & ((u64)(1 << 8) - 1)));
				rgb_infos[2].Printf("B = %u", (u32)(data.hi & ((u64)(1 << 8) - 1)));
				rgb_infos[3].Printf("A = %u", (u32)((data.hi >> 32) & ((u64)(1 << 8) - 1)));
				rgb_infos[4].Printf("Q = %f", m_stored_q);
			}
			else
			{
				rgb_infos[0].Printf("R = %u", (u32)(data.lo & ((u64)(1 << 8) - 1)));
				rgb_infos[1].Printf("G = %u", (u32)((data.lo >> 8) & ((u64)(1 << 8) - 1)));
				rgb_infos[2].Printf("B = %u", (u32)((data.lo >> 16) & ((u64)(1 << 8) - 1)));
				rgb_infos[3].Printf("A = %u", (u32)((data.lo >> 24) & ((u64)(1 << 8) - 1)));
				rgb_infos[4].Printf("Q = %f", *(float*)(&data.lo + 4));
			}

			for (auto& el : rgb_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case ST:
		{
			std::vector<wxString> st_infos(2);
			st_infos[0].Printf("S = %f", *(float*)(&data.lo));
			st_infos[1].Printf("T = %f", *(float*)(&data.lo + 4));
			if (packed)
			{
				wxString q;
				m_stored_q = *(float*)(&data.hi + 4);
				q.Printf("Q = %f", m_stored_q);
				st_infos.push_back(q);
			}
			for (auto& el : st_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case UV:
		{
			wxString s, t;
			double v;
			s.Printf("U = %f", (double)(data.lo & ((u64)(1 << 14) - 1)) / 16.0);
			if (packed)
				v = (double)((data.lo >> 32) & ((u64)(1 << 14) - 1)) / 16.0;
			else 
				v = (double)((data.lo >> 16) & ((u64)(1 << 14) - 1)) / 16.0;
			t.Printf("V = %f", v);
			m_gif_packet->AppendItem(rootId, s);
			m_gif_packet->AppendItem(rootId, t);
			break;
		}
		case XYZF2:
		case XYZF3:
		{
			if (packed && (reg == XYZF2) && ((data.lo >> 47) & ((u64)(1 << 1) - 1)) == 1)
				m_gif_packet->SetItemText(rootId, GIFRegName(XYZF3));

			std::vector<wxString> xyzf_infos(4);
			if (packed)
			{
				xyzf_infos[0].Printf("X = %f", (float)(data.lo & ((u64)(1 << 16) - 1)) / 16.0);
				xyzf_infos[1].Printf("Y = %f", (float)((data.lo >> 32) & ((u64)(1 << 16) - 1)) / 16.0);
				xyzf_infos[2].Printf("Z = %u", (u32)((data.hi >> 4) & ((u64)(1 << 24) - 1)));
				xyzf_infos[3].Printf("F = %u", (u32)((data.hi >> 36) & ((u64)(1 << 8) - 1)));
			}
			else
			{
				xyzf_infos[0].Printf("X = %f", (float)(data.lo & ((u64)(1 << 16) - 1)) / 16.0);
				xyzf_infos[1].Printf("Y = %f", (float)((data.lo >> 16) & ((u64)(1 << 16) - 1)) / 16.0);
				xyzf_infos[2].Printf("Z = %u", (u32)((data.lo >> 32) & ((u64)(1 << 24) - 1)));
				xyzf_infos[3].Printf("F = %u", (u32)((data.lo >> 56) & ((u64)(1 << 8) - 1)));
			}

			for (auto& el : xyzf_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case XYZ2:
		case XYZ3:
		{
			if (packed && (reg == XYZ2) && ((data.lo >> 47) & ((u64)(1 << 1) - 1)) == 1)
				m_gif_packet->SetItemText(rootId, GIFRegName(XYZ3));

			std::vector<wxString> xyz_infos(3);
			if (packed)
			{
				xyz_infos[0].Printf("X = %f", (float)(data.lo & ((u64)(1 << 16) - 1)) / 16.0);
				xyz_infos[1].Printf("Y = %f", (float)((data.lo >> 32) & ((u64)(1 << 16) - 1)) / 16.0);
				xyz_infos[2].Printf("Z = %u", *(u32*)(&data.hi));
			}
			else
			{
				xyz_infos[0].Printf("X = %f", (float)(data.lo & ((u64)(1 << 16) - 1)) / 16.0);
				xyz_infos[1].Printf("Y = %f", (float)((data.lo >> 16) & ((u64)(1 << 16) - 1)) / 16.0);
				xyz_infos[2].Printf("Z = %u", *(u32*)(&data.lo)+4);
			}

			for (auto& el : xyz_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case TEX0_1:
		case TEX0_2:
		{
			std::vector<wxString> tex_infos(12);

			tex_infos[0].Printf("TBP0 = %u", (u32)(data.lo & ((u64)(1 << 14) - 1)));
			tex_infos[1].Printf("TBW = %u", (u32)((data.lo >> 14) & ((u64)(1 << 6) - 1)));
			tex_infos[2].Printf("PSM = %s", TEXPSMNames[(u32)((data.lo >> 20) & ((u64)(1 << 6) - 1))]);
			tex_infos[3].Printf("TW = %u", (u32)((data.lo >> 26) & ((u64)(1 << 4) - 1)));
			tex_infos[4].Printf("TH = %u", (u32)((data.lo >> 30) & ((u64)(1 << 4) - 1)));
			tex_infos[5].Printf("TCC = %s", TEXTCCNames[(u32)((data.lo >> 34) & ((u64)(1 << 1) - 1))]);
			tex_infos[6].Printf("TFX = %s", TEXTFXNames[(u32)((data.lo >> 35) & ((u64)(1 << 2) - 1))]);
			tex_infos[7].Printf("CBP = %u", (u32)((data.lo >> 37) & ((u64)(1 << 14) - 1)));
			tex_infos[8].Printf("CPSM = %s", TEXCPSMNames[(u32)((data.lo >> 51) & ((u64)(1 << 4) - 1))]);
			tex_infos[9].Printf("CSM = %s", TEXCSMNames[(u32)((data.lo >> 55) & ((u64)(1 << 1) - 1))]);
			tex_infos[10].Printf("CSA = %u", (u32)((data.lo >> 56) & ((u64)(1 << 5) - 1)));
			tex_infos[11].Printf("CLD = %u", (u32)((data.lo >> 61) & ((u64)(1 << 3) - 1)));

			for (auto& el : tex_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case FOG:
		{
			wxString s;
			if (packed)
				s.Printf("F = %u", (u32)((data.hi >> 36) & ((u64)(1 << 8) - 1)));
			else
				s.Printf("F = %u", (u32)((data.lo >> 56) & ((u64)(1 << 8) - 1)));
			m_gif_packet->AppendItem(rootId, s);
			break;
		}
		case AD:
		{
			GIFReg nreg = (GIFReg)(data.hi & ((u64)(1 << 8) - 1));
			if ((GIFReg)nreg == AD)
			{
				wxString s;
				s.Printf("NOP");
				m_gif_packet->AppendItem(id, s);
			}
			else
				ParseTreeReg(id, nreg, data, packed);
			m_gif_packet->Delete(rootId);
			break;
		}
		default:
			break;
	}
}

void Dialogs::GSDumpDialog::ParseTreePrim(wxTreeItemId& id, u32 prim)
{
	std::vector<wxString> prim_infos(9);

	prim_infos[0].Printf("Primitive Type = %s", GsPrimNames[(prim & ((u64)(1 << 3) - 1))]);
	prim_infos[1].Printf("IIP = %s", GsIIPNames[((prim >> 3) & 1)]);
	prim_infos[2].Printf("TME = %s", (bool)((prim >> 4) & 1) ? "True" : "False");
	prim_infos[3].Printf("FGE = %s", (bool)((prim >> 5) & 1) ? "True" : "False");
	prim_infos[4].Printf("FGE = %s", (bool)((prim >> 6) & 1) ? "True" : "False");
	prim_infos[5].Printf("AA1 = %s", (bool)((prim >> 7) & 1) ? "True" : "False");
	prim_infos[6].Printf("FST = %s", GsFSTNames[((prim >> 3) & 1)]);
	prim_infos[7].Printf("CTXT = %s", GsCTXTNames[((prim >> 9) & 1)]);
	prim_infos[8].Printf("FIX = %s", GsFIXNames[((prim >> 10) & 1)]);

	for (auto& el : prim_infos)
		m_gif_packet->AppendItem(id, el);
}

void Dialogs::GSDumpDialog::ProcessDumpEvent(const GSData& event, char* regs)
{
	switch (event.id)
	{
		case Transfer:
		{
			switch (event.path)
			{
				case Path1Old:
				{
					std::unique_ptr<char[]> data(new char[16384]);
					int addr = 16384 - event.length;
					memcpy(data.get(), event.data.get() + addr, event.length);
					GSgifTransfer1((u8*)data.get(), addr);
					break;
				}
				case Path1New:
					GSgifTransfer((u8*)event.data.get(), event.length / 16);
					break;
				case Path2:
					GSgifTransfer2((u8*)event.data.get(), event.length / 16);
					break;
				case Path3:
					GSgifTransfer3((u8*)event.data.get(), event.length / 16);
					break;
				default:
					break;
			}
			break;
		}
		case VSync:
		{
			GSvsync((*((int*)(regs + 4096)) & 0x2000) > 0 ? (u8)1 : (u8)0);
			g_FrameCount++;
			Pcsx2App* app = (Pcsx2App*)wxApp::GetInstance();
			if (app)
				app->FpsManager.DoFrame();
			break;
		}
		case ReadFIFO2:
		{
			std::unique_ptr<char[]> arr(new char[*((int*)event.data.get())]);
			GSreadFIFO2((u8*)arr.get(), *((int*)event.data.get()));
			break;
		}
		case Registers:
			memcpy(regs, event.data.get(), 8192);
			break;
	}
}

// --------------------------------------------------------------------------------------
//  GSThread  Implementation
// --------------------------------------------------------------------------------------

Dialogs::GSDumpDialog::GSThread::GSThread(GSDumpDialog* dlg)
	: pxThread("GSDump")
	, m_root_window(dlg)
{
}

Dialogs::GSDumpDialog::GSThread::~GSThread()
{
	try
	{
		pxThread::Cancel();
	}
	DESTRUCTOR_CATCHALL
}

void Dialogs::GSDumpDialog::GSThread::OnStop()
{
	m_root_window->m_button_events.clear();
	m_dump_file->Close();

	wxCommandEvent event(EVT_CLOSE_DUMP);
	wxPostEvent(m_root_window, event);
}

void Dialogs::GSDumpDialog::GSThread::ExecuteTaskInThread()
{
	GSDump::isRunning = true;
	u32 crc = 0, ss = 0;
	s8 renderer_override = 0;
	switch (m_renderer)
	{
		// OGL SW
		case 1:
			renderer_override = 13;
			break;
		// D3D11 HW
		case 2:
			renderer_override = 3;
			break;
		// OGL HW
		case 3:
			renderer_override = 12;
			break;
		default:
			break;
	}
	char regs[8192];

	m_dump_file->Read(&crc, 4);
	m_dump_file->Read(&ss, 4);


	std::unique_ptr<char[]> state_data(new char[ss]);
	m_dump_file->Read(state_data.get(), ss);
	m_dump_file->Read(&regs, 8192);

	freezeData fd = {(int)ss, (u8*)state_data.get()};
	m_root_window->m_dump_packets.clear();

	while (m_dump_file->Tell() < m_dump_file->Length())
	{
		GSType id;
		GSTransferPath id_transfer = Dummy;
		m_dump_file->Read(&id, 1);
		s32 size = 0;
		switch (id)
		{
			case Transfer:
				m_dump_file->Read(&id_transfer, 1);
				m_dump_file->Read(&size, 4);
				break;
			case VSync:
				size = 1;
				break;
			case ReadFIFO2:
				size = 4;
				break;
			case Registers:
				size = 8192;
				break;
		}
		std::unique_ptr<char[]> data(new char[size]);
		m_dump_file->Read(data.get(), size);
		m_root_window->m_dump_packets.push_back({id, std::move(data), size, id_transfer});
	}

	GSinit();
	sApp.OpenGsPanel();

	// to gather the gs frame object we have to be a bit hacky since sApp is not syntax complete
	Pcsx2App* app = (Pcsx2App*)wxApp::GetInstance();
	GSFrame* window = nullptr;
	if (app)
	{
		app->FpsManager.Reset();
		window = app->GetGsFramePtr();
		g_FrameCount = 0;
	}

	GSsetBaseMem((u8*)regs);
	if (GSopen2((void**)pDsp, (renderer_override<<24)) != 0)
	{
		OnStop();
		return;
	}

	GSsetGameCRC((int)crc, 0);

	if (GSfreeze(FreezeAction::Load, &fd))
		GSDump::isRunning = false;
	GSvsync(1);
	GSreset();
	GSsetBaseMem((u8*)regs);
	GSfreeze(FreezeAction::Load, &fd);

	size_t i = 0;
	m_debug_index = 0;
	size_t debug_idx = 0;

	while (GSDump::isRunning)
	{
		if (m_debug)
		{
			if (m_root_window->m_button_events.size() > 0)
			{
				switch (m_root_window->m_button_events[0].btn)
				{
					case Step:
						if (debug_idx >= m_root_window->m_dump_packets.size())
							debug_idx = 0;
						m_debug_index = debug_idx;
						break;
					case RunCursor:
						m_debug_index = m_root_window->m_button_events[0].index;
						if (debug_idx > m_debug_index)
							debug_idx = 0;
						break;
					case RunVSync:
						if (debug_idx >= m_root_window->m_dump_packets.size())
							debug_idx = 1;
						if ((debug_idx + 1) < m_root_window->m_dump_packets.size())
						{
							auto it = std::find_if(m_root_window->m_dump_packets.begin() + debug_idx + 1, m_root_window->m_dump_packets.end(), [](const GSData& gs) { return gs.id == Registers; });
							if (it != std::end(m_root_window->m_dump_packets))
								m_debug_index = std::distance(m_root_window->m_dump_packets.begin(), it);
						}
						break;
				}
				m_root_window->m_button_events.erase(m_root_window->m_button_events.begin());

				if (debug_idx <= m_debug_index)
				{
					while (debug_idx <= m_debug_index)
					{
						m_root_window->ProcessDumpEvent(m_root_window->m_dump_packets[debug_idx++], regs);
					}
					if ((debug_idx + 1) < m_root_window->m_dump_packets.size())
					{
						auto it = std::find_if(m_root_window->m_dump_packets.begin() + debug_idx + 1, m_root_window->m_dump_packets.end(), [](const GSData& gs) { return gs.id == Registers; });
						if (it != std::end(m_root_window->m_dump_packets))
							m_root_window->ProcessDumpEvent(*it, regs);
					}
				}

				// do vsync
				m_root_window->ProcessDumpEvent({VSync, 0, 0, Dummy}, regs);
			}
		}
		else if (m_root_window->m_dump_packets.size())
		{
			do
				m_root_window->ProcessDumpEvent(m_root_window->m_dump_packets[i++], regs);
			while (i < m_root_window->m_dump_packets.size() && m_root_window->m_dump_packets[i].id != VSync);

			if (i >= m_root_window->m_dump_packets.size())
				i = 0;
		}
		if (window)
		{
			if (!window->IsShown())
			{
				GSclose();
				GSshutdown();
				sApp.CloseGsPanel();
				GSDump::isRunning = false;
			}
		}
	}

	OnStop();
	return;
}
