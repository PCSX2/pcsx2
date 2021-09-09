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
#include "HostDisplay.h"

#include "PathDefs.h"
#include "gui/AppConfig.h"
#include "gui/GSFrame.h"
#include "Counters.h"
#include "PerformanceMetrics.h"

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
#include <array>
#include <functional>

template <typename Output, typename Input, typename std::enable_if<sizeof(Input) == sizeof(Output), bool>::type = true>
static constexpr Output BitCast(Input input)
{
	Output output;
	memcpy(&output, &input, sizeof(input));
	return output;
}
template <typename Output = u32>
static constexpr Output GetBits(u64 value, u32 shift, u32 numbits)
{
	return static_cast<Output>((value >> shift) & ((1ull << numbits) - 1));
}

template <typename Output = u32>
static constexpr Output GetBits(u128 value, u32 shift, u32 numbits)
{
	u64 outval = 0;
	if (shift == 0)
		outval = value.lo;
	else if (shift < 64)
		outval = (value.lo >> shift) | (value.hi << (64 - shift));
	else
		outval = value.hi >> (shift - 64);
	return static_cast<Output>(outval & ((1ull << numbits) - 1));
}
static constexpr const char* GetNameOneBit(u8 value, const char* zero, const char* one)
{
	switch (value)
	{
		case 0: return zero;
		case 1: return one;
		default: return "UNKNOWN";
	}
}
static constexpr const char* GetNameBool(bool value)
{
	return value ? "True" : "False";
}

static constexpr const char* GetNamePRIMPRIM(u8 prim)
{
	switch (prim)
	{
		case 0: return "Point";
		case 1: return "Line";
		case 2: return "Line Strip";
		case 3: return "Triangle";
		case 4: return "Triangle Strip";
		case 5: return "Triangle Fan";
		case 6: return "Sprite";
		case 7: return "Invalid";
		default: return "UNKNOWN";
	}
}
static constexpr const char* GetNamePRIMIIP(u8 iip)
{
	return GetNameOneBit(iip, "Flat Shading", "Gouraud Shading");
}
static constexpr const char* GetNamePRIMFST(u8 fst)
{
	return GetNameOneBit(fst, "STQ Value", "UV Value");
}
static constexpr const char* GetNamePRIMCTXT(u8 ctxt)
{
	return GetNameOneBit(ctxt, "Context 1", "Context 2");
}
static constexpr const char* GetNamePRIMFIX(u8 fix)
{
	return GetNameOneBit(fix, "Unfixed", "Fixed");
}
static constexpr const char* GetNameTEXTCC(u8 tcc)
{
	return GetNameOneBit(tcc, "RGB", "RGBA");
}
static constexpr const char* GetNameTEXTFX(u8 tfx)
{
	switch (tfx)
	{
		case 0: return "Modulate";
		case 1: return "Decal";
		case 2: return "Highlight";
		case 3: return "Highlight2";
		default: return "UNKNOWN";
	}
}
static constexpr const char* GetNameTEXCSM(u8 csm)
{
	return GetNameOneBit(csm, "CSM1", "CSM2");
}
static constexpr const char* GetNameTEXPSM(u8 psm)
{
	switch (psm)
	{
		case 000: return "PSMCT32";
		case 001: return "PSMCT24";
		case 002: return "PSMCT16";
		case 012: return "PSMCT16S";
		case 023: return "PSMT8";
		case 024: return "PSMT4";
		case 033: return "PSMT8H";
		case 044: return "PSMT4HL";
		case 054: return "PSMT4HH";
		case 060: return "PSMZ32";
		case 061: return "PSMZ24";
		case 062: return "PSMZ16";
		case 072: return "PSMZ16S";
		default: return "UNKNOWN";
	}
}
static constexpr const char* GetNameTEXCPSM(u8 psm)
{
	switch (psm)
	{
		case 000: return "PSMCT32";
		case 002: return "PSMCT16";
		case 012: return "PSMCT16S";
		default: return "UNKNOWN";
	}
}

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
	rdoverrides.Add(Pcsx2Config::GSOptions::GetRendererName(GSRendererType::SW));
	rdoverrides.Add(Pcsx2Config::GSOptions::GetRendererName(GSRendererType::OGL));
#ifdef ENABLE_VULKAN
	rdoverrides.Add(Pcsx2Config::GSOptions::GetRendererName(GSRendererType::VK));
#endif
#if defined(_WIN32)
	rdoverrides.Add(Pcsx2Config::GSOptions::GetRendererName(GSRendererType::DX11));
#endif
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
	m_settings->Enable();
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
	m_settings->Disable();
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

	// config has to be reloaded here, otherwise it won't apply when we restart
	g_Conf->EmuOptions.GS.ReloadIniSettings();
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
		element.id == GSType::Transfer ? t.Printf(" - %s", GetName(element.path)) : t.Printf("");
		s.Printf("%d - %s%s - %d byte", i, GetName(element.id), t, element.length);
		if (element.id == GSType::VSync)
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
		case GSType::Transfer:
		{
			wxTreeItemId trootId = m_gif_packet->AppendItem(rootId, wxString::Format("Transfer Path %s", GetName(dump.path)));
			u64 tag  = *(u64*)(dump.data.get());
			u64 regs = *(u64*)(dump.data.get() + 8);
			u32 nloop   = GetBits(tag,  0, 15);
			u8 eop      = GetBits(tag, 15,  1);
			u8 pre      = GetBits(tag, 46,  1);
			u32 prim    = GetBits(tag, 47, 11);
			GIFFlag flg = GetBits<GIFFlag>(tag, 58, 2);
			u32 nreg    = GetBits(tag, 60,  4);
			if (nreg == 0)
				nreg = 16;

			std::array<wxString, 7> infos;
			m_stored_q = 1.0;

			m_gif_packet->AppendItem(trootId, wxString::Format("nloop = %u", nloop));
			m_gif_packet->AppendItem(trootId, wxString::Format("eop = %u", eop));
			m_gif_packet->AppendItem(trootId, wxString::Format("flg = %s", GetName(flg)));
			m_gif_packet->AppendItem(trootId, wxString::Format("pre = %u", pre));
			if (pre)
			{
				wxTreeItemId id = m_gif_packet->AppendItem(trootId, L"prim");
				ParseTreePrim(id, prim);
			}
			m_gif_packet->AppendItem(trootId, wxString::Format("nreg = %u", nreg));
			wxTreeItemId regId = m_gif_packet->AppendItem(trootId, L"reg");

			int p = 16;
			switch (flg)
			{
				case GIFFlag::PACKED:
				{
					for (u32 j = 0; j < nloop; j++)
					{
						for (u32 i = 0; i < nreg; i++)
						{
							u128 reg_data;
							memcpy(&reg_data, dump.data.get() + p, 16);
							ParseTreeReg(regId, GetBits<GIFReg>(regs, i * 4, 4), reg_data, true);
							p += 16;
						}
					}
					break;
				}
				case GIFFlag::REGLIST:
				{
					for (u32 j = 0; j < nloop; j++)
					{
						for (u32 i = 0; i < nreg; i++)
						{
							u128 reg_data;
							memcpy(&reg_data.lo, dump.data.get() + p, 8);
							reg_data.hi = 0;
							ParseTreeReg(regId, GetBits<GIFReg>(regs, i * 4, 4), reg_data, false);
							p += 8;
						}
					}
					break;
				}
				case GIFFlag::IMAGE:
				case GIFFlag::IMAGE2:
					m_gif_packet->AppendItem(regId, wxString::Format("IMAGE %d bytes", nloop * 16));
					break;
			}
			break;
		}
		case GSType::VSync:
		{
			wxString s;
			s.Printf("Field = %u", *(u8*)(dump.data.get()));
			m_gif_packet->AppendItem(rootId, s);
			break;
		}
		case GSType::ReadFIFO2:
		{
			wxString s;
			s.Printf("ReadFIFO2: Size = %d byte", dump.length);
			m_gif_packet->AppendItem(rootId, s);
			break;
		}
		case GSType::Registers:
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
	wxTreeItemId rootId = m_gif_packet->AppendItem(id, wxString(GetName(reg)));
	switch (reg)
	{
		case GIFReg::PRIM:
			ParseTreePrim(rootId, data.lo);
			break;
		case GIFReg::RGBAQ:
		{
			std::array<wxString, 5> rgb_infos;

			if (packed)
			{
				rgb_infos[0].Printf("R = %u", GetBits(data,  0, 8));
				rgb_infos[1].Printf("G = %u", GetBits(data, 32, 8));
				rgb_infos[2].Printf("B = %u", GetBits(data, 64, 8));
				rgb_infos[3].Printf("A = %u", GetBits(data, 96, 8));;
				rgb_infos[4].Printf("Q = %g", m_stored_q);
			}
			else
			{
				rgb_infos[0].Printf("R = %u", GetBits(data,  0, 8));
				rgb_infos[1].Printf("G = %u", GetBits(data,  8, 8));
				rgb_infos[2].Printf("B = %u", GetBits(data, 16, 8));
				rgb_infos[3].Printf("A = %u", GetBits(data, 24, 8));
				rgb_infos[4].Printf("Q = %g", BitCast<float>(data._u32[1]));
			}

			for (auto& el : rgb_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case GIFReg::ST:
			m_gif_packet->AppendItem(rootId, wxString::Format("S = %g", BitCast<float>(data._u32[0])));
			m_gif_packet->AppendItem(rootId, wxString::Format("T = %g", BitCast<float>(data._u32[1])));
			if (packed)
			{
				m_stored_q = BitCast<float>(data._u32[2]);
				m_gif_packet->AppendItem(rootId, wxString::Format("Q = %g", m_stored_q));
			}
			break;
		case GIFReg::UV:
			m_gif_packet->AppendItem(rootId, wxString::Format("U = %g", static_cast<float>(GetBits(data, 0, 14)) / 16.f));
			m_gif_packet->AppendItem(rootId, wxString::Format("V = %g", static_cast<float>(GetBits(data, packed ? 32 : 16, 14)) / 16.f));
			break;
		case GIFReg::XYZF2:
		case GIFReg::XYZF3:
		{
			if (packed && (reg == GIFReg::XYZF2) && GetBits(data, 111, 1))
				m_gif_packet->SetItemText(rootId, GetName(GIFReg::XYZF3));

			std::array<wxString, 4> xyzf_infos;
			if (packed)
			{
				xyzf_infos[0].Printf("X = %g", static_cast<float>(GetBits(data,  0, 16)) / 16.0);
				xyzf_infos[1].Printf("Y = %g", static_cast<float>(GetBits(data, 32, 16)) / 16.0);
				xyzf_infos[2].Printf("Z = %u", GetBits(data, 68, 24));
				xyzf_infos[3].Printf("F = %u", GetBits(data, 100, 8));
			}
			else
			{
				xyzf_infos[0].Printf("X = %g", static_cast<float>(GetBits(data,  0, 16)) / 16.0);
				xyzf_infos[1].Printf("Y = %g", static_cast<float>(GetBits(data, 16, 16)) / 16.0);
				xyzf_infos[2].Printf("Z = %u", GetBits(data, 32, 24));
				xyzf_infos[3].Printf("F = %u", GetBits(data, 56, 8));
			}

			for (auto& el : xyzf_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case GIFReg::XYZ2:
		case GIFReg::XYZ3:
		{
			if (packed && (reg == GIFReg::XYZ2) && GetBits(data, 111, 1))
				m_gif_packet->SetItemText(rootId, GetName(GIFReg::XYZ3));

			std::vector<wxString> xyz_infos(3);
			if (packed)
			{
				xyz_infos[0].Printf("X = %g", static_cast<float>(GetBits(data,  0, 16)) / 16.0);
				xyz_infos[1].Printf("Y = %g", static_cast<float>(GetBits(data, 32, 16)) / 16.0);
				xyz_infos[2].Printf("Z = %u", data._u32[2]);
			}
			else
			{
				xyz_infos[0].Printf("X = %g", static_cast<float>(GetBits(data,  0, 16)) / 16.0);
				xyz_infos[1].Printf("Y = %g", static_cast<float>(GetBits(data, 16, 16)) / 16.0);
				xyz_infos[2].Printf("Z = %u", data._u32[1]);
			}

			for (auto& el : xyz_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case GIFReg::TEX0_1:
		case GIFReg::TEX0_2:
		{
			std::array<wxString, 12> tex_infos;

			tex_infos[0].Printf("TBP0 = %u", GetBits(data, 0, 14));
			tex_infos[1].Printf("TBW = %u",  GetBits(data, 14, 6));
			tex_infos[2].Printf("PSM = %s",  GetNameTEXPSM (GetBits(data, 20, 6)));
			tex_infos[3].Printf("TW = %u",   GetBits(data, 26, 4));
			tex_infos[4].Printf("TH = %u",   GetBits(data, 30, 4));
			tex_infos[5].Printf("TCC = %s",  GetNameTEXTCC (GetBits(data, 34, 1)));
			tex_infos[6].Printf("TFX = %s",  GetNameTEXTFX (GetBits(data, 35, 2)));
			tex_infos[7].Printf("CBP = %u",  GetBits(data, 37, 14));
			tex_infos[8].Printf("CPSM = %s", GetNameTEXCPSM(GetBits(data, 51, 4)));
			tex_infos[9].Printf("CSM = %s",  GetNameTEXCSM (GetBits(data, 55, 1)));
			tex_infos[10].Printf("CSA = %u", GetBits(data, 56, 5));
			tex_infos[11].Printf("CLD = %u", GetBits(data, 61, 3));

			for (auto& el : tex_infos)
				m_gif_packet->AppendItem(rootId, el);
			break;
		}
		case GIFReg::FOG:
		{
			m_gif_packet->AppendItem(rootId, wxString::Format("F = %u", GetBits(data, packed ? 100 : 56, 8)));
			break;
		}
		case GIFReg::AD:
		{
			GIFReg nreg = GetBits<GIFReg>(data, 64, 8);
			if (nreg == GIFReg::AD)
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
	std::array<wxString, 9> prim_infos;

	prim_infos[0].Printf("Primitive Type = %s", GetNamePRIMPRIM(GetBits(prim, 0, 3)));
	prim_infos[1].Printf("IIP = %s",  GetNamePRIMIIP(GetBits(prim, 3, 1)));
	prim_infos[2].Printf("TME = %s",  GetNameBool(GetBits(prim, 4, 1)));
	prim_infos[3].Printf("FGE = %s",  GetNameBool(GetBits(prim, 5, 1)));
	prim_infos[4].Printf("ABE = %s",  GetNameBool(GetBits(prim, 6, 1)));
	prim_infos[5].Printf("AA1 = %s",  GetNameBool(GetBits(prim, 7, 1)));
	prim_infos[6].Printf("FST = %s",  GetNamePRIMFST(GetBits(prim, 8, 1)));
	prim_infos[7].Printf("CTXT = %s", GetNamePRIMCTXT(GetBits(prim, 9, 1)));
	prim_infos[8].Printf("FIX = %s",  GetNamePRIMFIX(GetBits(prim, 10, 1)));

	for (auto& el : prim_infos)
		m_gif_packet->AppendItem(id, el);
}

void Dialogs::GSDumpDialog::ProcessDumpEvent(const GSData& event, char* regs)
{
	switch (event.id)
	{
		case GSType::Transfer:
		{
			switch (event.path)
			{
				case GSTransferPath::Path1Old:
				{
					std::unique_ptr<char[]> data(new char[16384]);
					int addr = 16384 - event.length;
					memcpy(data.get(), event.data.get() + addr, event.length);
					GSgifTransfer1((u8*)data.get(), addr);
					break;
				}
				case GSTransferPath::Path1New:
					GSgifTransfer((u8*)event.data.get(), event.length / 16);
					break;
				case GSTransferPath::Path2:
					GSgifTransfer2((u8*)event.data.get(), event.length / 16);
					break;
				case GSTransferPath::Path3:
					GSgifTransfer3((u8*)event.data.get(), event.length / 16);
					break;
				default:
					break;
			}
			break;
		}
		case GSType::VSync:
		{
			GSvsync((*((int*)(regs + 4096)) & 0x2000) > 0 ? (u8)1 : (u8)0, false);
			g_FrameCount++;
			break;
		}
		case GSType::ReadFIFO2:
		{
			std::unique_ptr<char[]> arr(new char[*((int*)event.data.get())]);
			GSreadFIFO2((u8*)arr.get(), *((int*)event.data.get()));
			break;
		}
		case GSType::Registers:
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
	GSRendererType renderer = g_Conf->EmuOptions.GS.Renderer;
	switch (m_renderer)
	{
		// Software
		case 1:
			renderer = GSRendererType::SW;
			break;
		// OpenGL
		case 2:
			renderer = GSRendererType::OGL;
			break;
#ifdef ENABLE_VULKAN
		// Vulkan
		case 3:
			renderer = GSRendererType::VK;
			break;
#endif
#ifdef _WIN32
		// D3D11
		case 4:		// WIN32 implies WITH_VULKAN so this is okay
			renderer = GSRendererType::DX11;
			break;
#endif
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

	// Calling Length() internally does fseek() -> ftell() -> fseek(). Slow.
	// Calling ftell() for Tell() doesn't seem to be very cheap either.
	// So we track the position ourselves.
	const wxFileOffset length = m_dump_file->Length();
	wxFileOffset pos = m_dump_file->Tell();
#define READ_FROM_DUMP_FILE(var, size) \
	do \
	{ \
		m_dump_file->Read(var, size); \
		pos += size; \
	} while (0)

	while (pos < length)
	{
		GSType id;
		GSTransferPath id_transfer = GSTransferPath::Dummy;
		READ_FROM_DUMP_FILE(&id, 1);
		s32 size = 0;
		switch (id)
		{
			case GSType::Transfer:
				READ_FROM_DUMP_FILE(&id_transfer, 1);
				READ_FROM_DUMP_FILE(&size, 4);
				break;
			case GSType::VSync:
				size = 1;
				break;
			case GSType::ReadFIFO2:
				size = 4;
				break;
			case GSType::Registers:
				size = 8192;
				break;
		}
		std::unique_ptr<char[]> data(new char[size]);
		READ_FROM_DUMP_FILE(data.get(), size);
		m_root_window->m_dump_packets.push_back({id, std::move(data), size, id_transfer});
	}
#undef READ_FROM_DUMP_FILE

	GSinit();
	sApp.OpenGsPanel();

	// to gather the gs frame object we have to be a bit hacky since sApp is not syntax complete
	Pcsx2App* app = (Pcsx2App*)wxApp::GetInstance();
	GSFrame* window = nullptr;
	if (app)
	{
		PerformanceMetrics::Reset();
		window = app->GetGsFramePtr();
		g_FrameCount = 0;
	}

	if (!GSopen(g_Conf->EmuOptions.GS, renderer, (u8*)regs))
	{
		OnStop();
		return;
	}

	GSsetGameCRC((int)crc, 0);

	if (GSfreeze(FreezeAction::Load, &fd))
		GSDump::isRunning = false;
	GSvsync(1, false);
	GSreset();
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
							auto it = std::find_if(m_root_window->m_dump_packets.begin() + debug_idx + 1, m_root_window->m_dump_packets.end(), [](const GSData& gs) { return gs.id == GSType::Registers; });
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
						auto it = std::find_if(m_root_window->m_dump_packets.begin() + debug_idx + 1, m_root_window->m_dump_packets.end(), [](const GSData& gs) { return gs.id == GSType::Registers; });
						if (it != std::end(m_root_window->m_dump_packets))
							m_root_window->ProcessDumpEvent(*it, regs);
					}
				}

				// do vsync
				m_root_window->ProcessDumpEvent({GSType::VSync, 0, 0, GSTransferPath::Dummy}, regs);
			}
		}
		else if (m_root_window->m_dump_packets.size())
		{
			do
				m_root_window->ProcessDumpEvent(m_root_window->m_dump_packets[i++], regs);
			while (i < m_root_window->m_dump_packets.size() && m_root_window->m_dump_packets[i].id != GSType::VSync);

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
