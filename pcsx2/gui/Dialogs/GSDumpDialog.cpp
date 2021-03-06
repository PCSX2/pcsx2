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
#include <functional>

using namespace pxSizerFlags;

// --------------------------------------------------------------------------------------
//  GSDumpDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::GSDumpDialog::GSDumpDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, _("GSDumpGov"), pxDialogFlags())
	, m_dump_list(new wxListView(this, ID_DUMP_LIST, wxDefaultPosition, wxSize(500, 400), wxLC_NO_HEADER | wxLC_REPORT))
	, m_preview_image(new wxStaticBitmap(this, wxID_ANY, wxBitmap(EmbeddedImage<res_NoIcon>().Get())))
	, m_selected_dump(new wxString(""))
	, m_debug_mode(new wxCheckBox(this, ID_DEBUG_MODE, _("Debug Mode")))
	, m_renderer_overrides(new wxRadioBox())
	, m_gif_list(new wxTreeCtrl(this, ID_SEL_PACKET, wxDefaultPosition, wxSize(500, 400), wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT))
	, m_gif_packet(new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(500, 400), wxTR_HIDE_ROOT | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT))
	, m_start(new wxButton(this, ID_RUN_START, _("Go to Start")))
	, m_step(new wxButton(this, ID_RUN_START, _("Step")))
	, m_selection(new wxButton(this, ID_RUN_START, _("Run to Selection")))
	, m_vsync(new wxButton(this, ID_RUN_START, _("Go to next VSync")))
	, m_thread(std::make_unique<GSThread>(this))
{
	//TODO: figure out how to fix sliders so the destructor doesn't segfault
	wxFlexGridSizer& general(*new wxFlexGridSizer(2, StdPadding, StdPadding));
	wxBoxSizer& dump_info(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dump_preview(*new wxBoxSizer(wxVERTICAL));
	wxFlexGridSizer& debugger(*new wxFlexGridSizer(3, StdPadding, StdPadding));
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


	m_debug_mode->Disable();
	m_start->Disable();
	m_step->Disable();
	m_selection->Disable();
	m_vsync->Disable();


	// debugger
	dbg_tree += new wxStaticText(this, wxID_ANY, _("GIF Packets"));
	dbg_tree += m_gif_list;
	dbg_actions += m_debug_mode;
	dbg_actions += m_start;
	dbg_actions += m_step;
	dbg_actions += m_selection;
	dbg_actions += m_vsync;

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
	Bind(wxEVT_TREE_SEL_CHANGED, &Dialogs::GSDumpDialog::ParsePacket, this, ID_SEL_PACKET);
	Bind(wxEVT_CHECKBOX, &Dialogs::GSDumpDialog::CheckDebug, this, ID_DEBUG_MODE);
}

void Dialogs::GSDumpDialog::GetDumpsList()
{
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
	m_debug_mode->Enable();
	m_start->Enable();
	m_step->Enable();
	m_selection->Enable();
	m_vsync->Enable();
	GetCorePlugins().Shutdown();
	m_thread->Start();
	return;
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
	m_button_events.push_back(GSEvent{RunCursor, wxAtoi(m_gif_list->GetItemText(m_gif_list->GetFocusedItem()).BeforeFirst('-'))});
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
	wxTreeItemId rootId = m_gif_list->AppendItem(mainrootId, "0 - VSync");
	for (auto& element : dump)
	{
			wxString s;
			([&] { return element.id == Transfer; })() ? (s.Printf("%d - %s - %s - %d byte", i, GSTypeNames[element.id], GSTransferPathNames[element.path], element.length)) : 
									s.Printf("%d - %s - %d byte", i, GSTypeNames[element.id], element.length);
			if (element.id == VSync)
			{
				m_gif_list->SetItemText(rootId, s);
				rootId = m_gif_list->AppendItem(mainrootId, "VSync");
			}
			else 
				m_gif_list->AppendItem(rootId, s);
		i++;
	}
	m_gif_list->Delete(rootId);
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
				u64 tag = *(u64*)(dump.data);
				u64 regs = *(u64*)(dump.data + 8);
				u8 nloop = tag & ((u64)(1 << 15) - 1);
				u8 eop = (tag >> 15) & 1;
				u8 pre = (tag >> 46) & 1;
				u32 prim = (tag >> 47) & ((u64)(1 << 11) - 1);
				u8 flg = ((tag >> 58) & 3);
				u32 nreg = (u32)((tag >> 60) & ((u64)(1 << 4) - 1));
				if (nreg == 0)
					nreg = 16;

				wxString snloop, seop, sflg, spre, sprim, snreg, sreg;
				std::vector<wxString> infos = {snloop, seop, sflg, spre, sprim, snreg, sreg};
				m_stored_q = 1;

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
						for (int j = 0; j < nloop; j++)
						{
							for (int i = 0; i < nreg; i++)
							{
								u128 reg_data;
								reg_data.lo =  *(u64*)(dump.data + p);
								reg_data.hi =  *(u64*)(dump.data + p + 8);
								ParseTreeReg(regId, (GIFReg)((regs >> (i * 4)) & ((u64)(1 << 4) - 1)), reg_data, true);
								p += 16;
							}
						}
						break;
					}
					case GIF_FLG_REGLIST:
					{
						for (int j = 0; j < nloop; j++)
						{
							for (int i = 0; i < nreg; i++)
							{
								u128 reg_data;
								reg_data.lo =  *(u64*)(dump.data + p);
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

				//m_gif_packet->AppendItem(trootId, s);
				break;
			}
			case VSync:
			{
				wxString s;
				s.Printf("Field = %d", (u8)dump.data);
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
			{
				m_gif_packet->AppendItem(rootId, "Registers");
				break;
			}
	}
	m_gif_packet->ExpandAll();
}

void Dialogs::GSDumpDialog::ParsePacket(wxTreeEvent& event)
{
	int id = wxAtoi(m_gif_list->GetItemText(event.GetItem()).BeforeFirst('-'));
	GenPacketInfo(m_dump_packets[id]);
}

void Dialogs::GSDumpDialog::ParseTreeReg(wxTreeItemId& id, GIFReg reg, u128 data, bool packed)
{
	wxTreeItemId rootId = m_gif_packet->AppendItem(id, wxString(GIFRegName(reg)));
	switch (reg)
	{
		case PRIM:
		{
			ParseTreePrim(rootId, data.lo);
			break;
		}
		case RGBAQ:
		{
			wxString a, b, c, d, e;
			std::vector<wxString> rgb_infos = {a, b, c, d, e};

			if (packed)
			{
				rgb_infos[0].Printf("R = %u", (u32)(data.lo & ((u64)(1 << 8) - 1)));
				rgb_infos[1].Printf("G = %u", (u32)((data.lo >> 32) & ((u64)(1 << 8) - 1)));
				rgb_infos[2].Printf("B = %u", (u32)(data.hi & ((u64)(1 << 8) - 1)));
				rgb_infos[3].Printf("A = %u", (u32)((data.hi >> 32) & ((u64)(1 << 8) - 1)));
				rgb_infos[4].Printf("Q = %u", m_stored_q);
			}
			else
			{
				rgb_infos[0].Printf("R = %u", (u32)(data.lo & ((u64)(1 << 8) - 1)));
				rgb_infos[1].Printf("G = %u", (u32)((data.lo >> 8) & ((u64)(1 << 8) - 1)));
				rgb_infos[2].Printf("B = %u", (u32)((data.lo >> 16) & ((u64)(1 << 8) - 1)));
				rgb_infos[3].Printf("A = %u", (u32)((data.lo >> 24) & ((u64)(1 << 8) - 1)));
				rgb_infos[4].Printf("Q = %u", *(u32*)(&data.lo + 4));
			}

			for (auto& el : rgb_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case ST:
		{
			wxString s, t;
			std::vector<wxString> st_infos = {s, t};
			st_infos[0].Printf("S = %u", *(u32*)(&data.lo));
			st_infos[1].Printf("T = %u", *(u32*)(&data.lo + 4));
			if (packed)
			{
				wxString q;
				m_stored_q = *(u32*)(&data.hi + 4);
				q.Printf("Q = %u", m_stored_q);
				st_infos.push_back(q);
			}
			for (auto& el : st_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case UV:
		{
			wxString s, t;
			u32 v;
			s.Printf("U = %u", (u32)(data.lo & ((u64)(1 << 14) - 1)) / 16);
			if (packed)
				v = (u32)((data.lo >> 32) & ((u64)(1 << 14) - 1)) / 16;
			else 
				v = (u32)((data.lo >> 16) & ((u64)(1 << 14) - 1)) / 16;
			t.Printf("V = %u", v);
			m_gif_packet->AppendItem(rootId, s);
			m_gif_packet->AppendItem(rootId, t);
			break;
		}
		case XYZF2:
		case XYZF3:
		{
			if (packed && (reg == XYZF2) && ((data.lo >> 47) & ((u64)(1 << 1) - 1)) == 1)
				m_gif_packet->SetItemText(rootId, GIFRegName(XYZF3));

			wxString a, b, c, d;
			std::vector<wxString> xyzf_infos = {a, b, c, d};
			if (packed)
			{
				xyzf_infos[0].Printf("X = %u", (u32)(data.lo & ((u64)(1 << 16) - 1)) / 16);
				xyzf_infos[1].Printf("Y = %u", (u32)((data.lo >> 32) & ((u64)(1 << 16) - 1)) / 16);
				xyzf_infos[2].Printf("Z = %u", (u32)((data.hi >> 4) & ((u64)(1 << 24) - 1)));
				xyzf_infos[3].Printf("F = %u", (u32)((data.hi >> 36) & ((u64)(1 << 8) - 1)));
			}
			else
			{
				xyzf_infos[0].Printf("X = %u", (u32)(data.lo & ((u64)(1 << 16) - 1)) / 16);
				xyzf_infos[1].Printf("Y = %u", (u32)((data.lo >> 16) & ((u64)(1 << 16) - 1)) / 16);
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

			wxString a, b, c;
			std::vector<wxString> xyz_infos = {a, b, c};
			if (packed)
			{
				xyz_infos[0].Printf("X = %u", (u32)(data.lo & ((u64)(1 << 16) - 1)) / 16);
				xyz_infos[1].Printf("Y = %u", (u32)((data.lo >> 32) & ((u64)(1 << 16) - 1)) / 16);
				xyz_infos[2].Printf("Z = %u", *(u32*)(&data.hi));
			}
			else
			{
				xyz_infos[0].Printf("X = %u", (u32)(data.lo & ((u64)(1 << 16) - 1)) / 16);
				xyz_infos[1].Printf("Y = %u", (u32)((data.lo >> 16) & ((u64)(1 << 16) - 1)) / 16);
				xyz_infos[2].Printf("Z = %u", *(u32*)(&data.lo)+4);
			}

			for (auto& el : xyz_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case TEX0_1:
		case TEX0_2:
		{
			wxString a, b, c, d, e, f, g, h, i, j, k, l;
			std::vector<wxString> tex_infos = {a, b, c, d, e, f, g, h, i, j, k, l};

			tex_infos[0].Printf("TBP0 = %u", (u32)(data.lo & ((u64)(1 << 14) - 1)));
			tex_infos[1].Printf("TBW = %u", (u32)((data.lo >> 14) & ((u64)(1 << 6) - 1)));
			tex_infos[2].Printf("PSM = %u", (u32)((data.lo >> 20) & ((u64)(1 << 6) - 1)));
			tex_infos[3].Printf("TW = %u", (u32)((data.lo >> 26) & ((u64)(1 << 4) - 1)));
			tex_infos[4].Printf("TH = %u", (u32)((data.lo >> 30) & ((u64)(1 << 4) - 1)));
			tex_infos[5].Printf("TCC = %u", (u32)((data.lo >> 34) & ((u64)(1 << 1) - 1)));
			tex_infos[6].Printf("TFX = %u", (u32)((data.lo >> 35) & ((u64)(1 << 2) - 1)));
			tex_infos[7].Printf("CBP = %u", (u32)((data.lo >> 37) & ((u64)(1 << 14) - 1)));
			tex_infos[8].Printf("CPSM = %u", (u32)((data.lo >> 51) & ((u64)(1 << 4) - 1)));
			tex_infos[9].Printf("CSM = %u", (u32)((data.lo >> 55) & ((u64)(1 << 1) - 1)));
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
			wxString s;
			GIFReg nreg = (GIFReg)(data.hi & ((u64)(1 << 8) - 1));
			if ((GIFReg)nreg == AD)
			{
				s.Printf("NOP");
				m_gif_packet->AppendItem(id, s);
			}
			else
				ParseTreeReg(id, nreg, data, packed);
			m_gif_packet->Delete(rootId);
			break;
		}
	}
}

void Dialogs::GSDumpDialog::ParseTreePrim(wxTreeItemId& id, u32 prim)
{
	wxString a, b, c, d, e, f, g, h, i;
	std::vector<wxString> prim_infos = {a, b, c, d, e, f, g, h, i};

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

void Dialogs::GSDumpDialog::CheckDebug(wxCommandEvent& event)
{
	if (m_debug_mode->GetValue())
		GenPacketList(m_dump_packets);
	else
	{
		m_gif_list->DeleteAllItems();
		m_gif_packet->DeleteAllItems();
		m_gif_list->Refresh();
	}
}

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
	m_root_window->m_debug_mode->Disable();
	m_root_window->m_start->Disable();
	m_root_window->m_step->Disable();
	m_root_window->m_selection->Disable();
	m_root_window->m_vsync->Disable();
	m_root_window->m_gif_list->DeleteAllItems();
	m_root_window->m_gif_packet->DeleteAllItems();
	m_root_window->m_gif_list->Refresh();
	m_root_window->m_button_events.clear();
}

void Dialogs::GSDumpDialog::GSThread::ExecuteTaskInThread()
{
	pxInputStream dump_file(*m_root_window->m_selected_dump, new wxFFileInputStream(*m_root_window->m_selected_dump));

	if (!dump_file.IsOk())
	{
		OnStop();
		return;
	}

	char freeze_data[sizeof(int) * 2];
	u32 crc = 0, ss = 0;
	// XXX: check the numbers are correct
	int renderer_override = m_root_window->m_renderer_overrides->GetSelection();
	char regs[8192];

	dump_file.Read(&crc, 4);
	dump_file.Read(&ss, 4);


	char* state_data = (char*)malloc(sizeof(char) * ss);
	dump_file.Read(state_data, ss);
	dump_file.Read(&regs, 8192);

	int ssi = ss;
	freezeData fd = {ss, (s8*)state_data};
	m_root_window->m_dump_packets.clear();

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
				m_root_window->m_dump_packets.push_back(data);
				break;
			}
			case VSync:
			{
				u8 vsync = 0;
				dump_file.Read(&vsync, 1);
				GSData data = {id, (char*)vsync, 1, Dummy};
				m_root_window->m_dump_packets.push_back(data);
				break;
			}
			case ReadFIFO2:
			{
				u32 fifo = 0;
				dump_file.Read(&fifo, 4);
				GSData data = {id, (char*)fifo, 4, Dummy};
				m_root_window->m_dump_packets.push_back(data);
				break;
			}
			case Registers:
			{
				char regs_tmp[8192];
				dump_file.Read(&regs, 8192);
				GSData data = {id, regs_tmp, 8192, Dummy};
				m_root_window->m_dump_packets.push_back(data);
				break;
			}
		}
	}

	if (m_root_window->m_debug_mode->GetValue())
		m_root_window->GenPacketList(m_root_window->m_dump_packets);

	GetCorePlugins().Init();
	GSsetBaseMem((void*)regs);
	if (GSopen2((void*)pDsp, renderer_override) != 0)
	{
		OnStop();
		return;
	}

	GSsetGameCRC((int)crc, 0);

	if (!GetCorePlugins().DoFreeze(PluginId_GS, 0, &fd, true))
		m_running = false;
	GSvsync(1);
	GSreset();
	GSsetBaseMem((void*)regs);
	GetCorePlugins().DoFreeze(PluginId_GS, 0, &fd, true);

	size_t i = 0;
	int RunTo = 0;
	size_t debug_idx = 0;

	while (m_running)
	{
		if (m_root_window->m_debug_mode->GetValue())
		{
			if (m_root_window->m_button_events.size() > 0)
			{
				switch (m_root_window->m_button_events[0].index)
				{
					case Step:
						if (debug_idx >= m_root_window->m_dump_packets.size())
							debug_idx = 0;
						RunTo = debug_idx;
						break;
					case RunCursor:
						RunTo = m_root_window->m_button_events[0].index;
						if (debug_idx > RunTo)
							debug_idx = 0;
						break;
					case RunVSync:
						if (debug_idx >= m_root_window->m_dump_packets.size())
							debug_idx = 1;
						auto it = std::find_if(m_root_window->m_dump_packets.begin() + debug_idx, m_root_window->m_dump_packets.end(), [](const GSData& gs) { return gs.id == Registers; });
						if (it != std::end(m_root_window->m_dump_packets))
							RunTo = std::distance(m_root_window->m_dump_packets.begin(), it);
						break;
				}
				m_root_window->m_button_events.erase(m_root_window->m_button_events.begin());

				if (debug_idx <= RunTo)
				{
					while (debug_idx <= RunTo)
					{
						m_root_window->ProcessDumpEvent(m_root_window->m_dump_packets[debug_idx++], regs);
					}
					auto it = std::find_if(m_root_window->m_dump_packets.begin() + debug_idx, m_root_window->m_dump_packets.end(), [](const GSData& gs) { return gs.id == Registers; });
					if (it != std::end(m_root_window->m_dump_packets))
						m_root_window->ProcessDumpEvent(*it, regs);

					debug_idx--;
				}

				// do vsync
				m_root_window->ProcessDumpEvent(GSData{VSync, 0, 0, Dummy}, regs);
			}
		}
		else
		{
			while (i < (m_root_window->m_dump_packets.size()-1))
			{
				m_root_window->ProcessDumpEvent(m_root_window->m_dump_packets[i++], regs);

				if (m_root_window->m_dump_packets[i].id == VSync)
					break;
			}
			if (i >= m_root_window->m_dump_packets.size())
				i = 0;
		}
	}

	GetCorePlugins().Shutdown();
	OnStop();
	return;
}