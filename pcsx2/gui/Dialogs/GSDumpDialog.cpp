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


#include "gui/EmbeddedImage.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "gui/Resources/NoIcon.h"
#include "HostDisplay.h"

#include "gui/PathDefs.h"
#include "gui/AppConfig.h"
#include "gui/GSFrame.h"
#include "Counters.h"
#include "PerformanceMetrics.h"
#include "GameDatabase.h"

#include <wx/mstream.h>
#include <wx/listctrl.h>
#include <wx/filepicker.h>
#include <wx/radiobut.h>
#include <wx/button.h>
#include <wx/treectrl.h>
#include <wx/checkbox.h>
#include <wx/spinctrl.h>
#include <wx/dir.h>
#include <wx/image.h>
#include <wx/wfstream.h>
#include <array>
#include <functional>
#include <optional>

using namespace GSDumpTypes;

static std::optional<wxImage> GetPreviewImageFromDump(const std::string& filename)
{
	std::vector<u32> pixels;
	u32 width, height;
	if (!GSDumpFile::GetPreviewImageFromDump(filename.c_str(), &width, &height, &pixels))
		return std::nullopt;

	// strip alpha bytes because wx is dumb and stores on a separate plane
	// apparently this isn't aligned? stupidity...
	const u32 pitch = width * 3;
	u8* wxpixels = static_cast<u8*>(std::malloc(pitch * height));
	for (u32 y = 0; y < height; y++)
	{
		const u8* in = reinterpret_cast<const u8*>(pixels.data() + y * width);
		u8* out = wxpixels + y * pitch;
		for (u32 x = 0; x < width; x++)
		{
			*(out++) = in[0];
			*(out++) = in[1];
			*(out++) = in[2];
			in += sizeof(u32);
		}
	}

	return wxImage(wxSize(width, height), wxpixels);
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

static GSRendererType s_renderer_overrides[] = {
	GSRendererType::SW,
#ifdef ENABLE_OPENGL
	GSRendererType::OGL,
#endif
#ifdef ENABLE_VULKAN
	GSRendererType::VK,
#endif
#if defined(_WIN32)
	GSRendererType::DX11,
	GSRendererType::DX12,
#elif defined(__APPLE__)
	GSRendererType::Metal,
#endif
};

Dialogs::GSDumpDialog::GSDumpDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, _("GS Debugger"), pxDialogFlags().SetMinimize().SetResize())
	, m_dump_list(new wxListView(this, ID_DUMP_LIST, wxDefaultPosition, wxSize(400, 300), wxLC_NO_HEADER | wxLC_REPORT | wxLC_SINGLE_SEL))
	, m_preview_image(new wxStaticBitmap(this, wxID_ANY, wxBitmap(EmbeddedImage<res_NoIcon>().Get()), wxDefaultPosition, wxSize(400,250)))
	, m_debug_mode(new wxCheckBox(this, ID_DEBUG_MODE, _("Debug Mode")))
	, m_renderer_overrides(new wxRadioBox())
	, m_framerate_selector(new wxSpinCtrl(this, ID_FRAMERATE, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 999, 0))
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
	wxFlexGridSizer* grid = new wxFlexGridSizer(3, StdPadding, StdPadding);
	wxBoxSizer* dbg_tree = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dbg_actions = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* gif = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* dumps_list = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* framerate_sel = new wxBoxSizer(wxHORIZONTAL);
	framerate_sel->Add(new wxStaticText(this, wxID_ANY, _("Framerate:")), wxSizerFlags().Centre());
	framerate_sel->Add(m_framerate_selector, wxSizerFlags(1).Expand());

	m_run->SetDefault();
	wxArrayString rdoverrides;
	rdoverrides.Add("None");
	for (GSRendererType over : s_renderer_overrides)
		rdoverrides.Add(Pcsx2Config::GSOptions::GetRendererName(over));
	m_renderer_overrides->Create(this, wxID_ANY, "Renderer overrides", wxDefaultPosition, wxDefaultSize, rdoverrides, 1);

	dbg_tree->Add(new wxStaticText(this, wxID_ANY, _("GIF Packets")));
	dbg_tree->Add(m_gif_list, StdExpand().Proportion(1));
	dbg_actions->Add(m_debug_mode);
	dbg_actions->Add(m_start, StdExpand());
	dbg_actions->Add(m_step, StdExpand());
	dbg_actions->Add(m_selection, StdExpand());
	dbg_actions->Add(m_vsync, StdExpand());
	gif->Add(new wxStaticText(this, wxID_ANY, _("Packet Content")));
	gif->Add(m_gif_packet, StdExpand().Proportion(1));

	dumps_list->Add(new wxStaticText(this, wxID_ANY, _("GS Dumps List")), StdExpand());
	dumps_list->Add(m_dump_list, StdExpand());
	dump_info->Add(m_renderer_overrides, StdExpand());
	dump_info->Add(framerate_sel, StdExpand());
	dump_info->Add(m_settings, StdExpand());
	dump_info->Add(m_run, StdExpand());
	dump_preview->Add(new wxStaticText(this, wxID_ANY, _("Preview")), StdExpand());
	dump_preview->Add(m_preview_image, StdCenter());

	grid->Add(dumps_list, StdExpand());
	grid->Add(dump_info, StdExpand());
	grid->Add(dump_preview, StdExpand());

	grid->Add(dbg_tree, StdExpand());
	grid->Add(dbg_actions, StdExpand());
	grid->Add(gif, StdExpand());

	grid->AddGrowableCol(0);
	grid->AddGrowableCol(2);
	grid->AddGrowableRow(1);

	SetSizerAndFit(grid);

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
	Bind(wxEVT_SPINCTRL, &Dialogs::GSDumpDialog::UpdateFramerate, this, ID_FRAMERATE);
	Bind(EVT_CLOSE_DUMP, &Dialogs::GSDumpDialog::CloseDump, this);

	UpdateFramerate(m_framerate_selector->GetValue());
}

void Dialogs::GSDumpDialog::GetDumpsList()
{
	m_dump_list->ClearAll();
	wxDir snaps(g_Conf->Folders.Snapshots.ToAscii());
	wxString filename;
	bool cont = snaps.GetFirst(&filename);
	int h = 0, j = 0;
	m_dump_list->AppendColumn("Dumps");
	// set the column size to be exactly of the size of our list
	m_dump_list->GetSize(&h, &j);
	m_dump_list->SetColumnWidth(0, h);
	std::vector<wxString> dumps;

	while (cont)
	{
		if (filename.EndsWith(".gs"))
			dumps.push_back(filename.substr(0, filename.length() - 3));
		else if (filename.EndsWith(".gs.xz"))
			dumps.push_back(filename.substr(0, filename.length() - 6));
		else if (filename.EndsWith(".gs.zst"))
			dumps.push_back(filename.substr(0, filename.length() - 7));
		cont = snaps.GetNext(&filename);
	}
	std::sort(dumps.begin(), dumps.end(), [](const wxString& a, const wxString& b) { return a.CmpNoCase(b) < 0; });
	dumps.erase(std::unique(dumps.begin(), dumps.end()), dumps.end()); // In case there was both .gs and .gs.xz
	for (size_t i = 0; i < dumps.size(); i++)
		m_dump_list->InsertItem(i, dumps[i]);
}

void Dialogs::GSDumpDialog::UpdateFramerate(int val)
{
	if (val)
		m_thread->m_frame_ticks = (GetTickFrequency() + (val/2)) / val;
	else
		m_thread->m_frame_ticks = 0;
}

void Dialogs::GSDumpDialog::UpdateFramerate(wxCommandEvent& evt)
{
	UpdateFramerate(evt.GetInt());
}

void Dialogs::GSDumpDialog::SelectedDump(wxListEvent& evt)
{
	wxString filename_preview = g_Conf->Folders.Snapshots.ToAscii() + ("/" + evt.GetText()) + ".png";
	wxString filename = g_Conf->Folders.Snapshots.ToAscii() + ("/" + evt.GetText()) + ".gs";
	if (!wxFileExists(filename))
		filename.append(".xz");
	if (!wxFileExists(filename))
		filename = filename.RemoveLast(3).append(".zst");
	if (wxFileExists(filename_preview))
	{
		auto img = wxImage(filename_preview);
		img.Rescale(400,250, wxIMAGE_QUALITY_HIGH);
		m_preview_image->SetBitmap(wxBitmap(img));
	}
	else if (std::optional<wxImage> img = GetPreviewImageFromDump(StringUtil::wxStringToUTF8String(filename)); img.has_value())
	{
		// try embedded dump
		img->Rescale(400, 250, wxIMAGE_QUALITY_HIGH);
		m_preview_image->SetBitmap(img.value());
	}
	else
	{
		m_preview_image->SetBitmap(EmbeddedImage<res_NoIcon>().Get());
	}

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

	m_thread->m_dump_file = GSDumpFile::OpenGSDump(m_selected_dump.ToUTF8());
	if (!m_thread->m_dump_file)
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

u32 Dialogs::GSDumpDialog::ReadPacketSize(const void* packet)
{
	u128 tag;
	memcpy(&tag, packet, sizeof(tag));
	u32 nloop   = GetBits(tag,  0, 15);
	GIFFlag flg = GetBits<GIFFlag>(tag, 58, 2);
	u32 nreg    = GetBits(tag, 60,  4);
	if (nreg == 0)
		nreg = 16;
	switch (flg)
	{
		case GIFFlag::PACKED:
			return 16 + nloop * nreg * 16;
		case GIFFlag::REGLIST:
			return 16 + (nloop * nreg * 8 + 8) & ~15;
		default:
			return 16 + nloop * 16;
	}
}

void Dialogs::GSDumpDialog::GenPacketList()
{
	int i = 0;
	m_gif_list->DeleteAllItems();
	m_gif_items.clear();
	wxTreeItemId mainrootId = m_gif_list->AddRoot("root");
	wxTreeItemId rootId = m_gif_list->AppendItem(mainrootId, "0 - VSync");
	for (auto& element : m_thread->m_dump_file->GetPackets())
	{
		wxString s, t;
		element.id == GSType::Transfer ? t.Printf(" - %s", GSDumpTypes::GetName(element.path)) : t.Printf("");
		s.Printf("%d - %s%s - %d byte", i, GSDumpTypes::GetName(element.id), t, element.length);
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

void Dialogs::GSDumpDialog::GenPacketInfo(const GSDumpFile::GSData& dump)
{
	m_gif_packet->DeleteAllItems();
	wxTreeItemId rootId = m_gif_packet->AddRoot("root");
	switch (dump.id)
	{
		case GSType::Transfer:
		{
			const u8* data = dump.data;
			u32 remaining = dump.length;
			int idx = 0;
			while (remaining >= 16)
			{
				wxTreeItemId trootId = m_gif_packet->AppendItem(rootId, wxString::Format("Transfer Path %s Packet %u", GSDumpTypes::GetName(dump.path), idx));
				ParseTransfer(trootId, data);
				m_gif_packet->Expand(trootId);
				u32 size = ReadPacketSize(data);
				remaining -= size;
				data += size;
				idx++;
			}
			break;
		}
		case GSType::VSync:
		{
			wxString s;
			s.Printf("Field = %u", dump.data[0]);
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
}

void Dialogs::GSDumpDialog::ParseTransfer(wxTreeItemId& trootId, const u8* data)
{
	u64 tag  = *(u64*)data;
	u64 regs = *(u64*)(data + 8);
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
	m_gif_packet->AppendItem(trootId, wxString::Format("flg = %s", GSDumpTypes::GetName(flg)));
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
					memcpy(&reg_data, data + p, 16);
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
					memcpy(&reg_data.lo, data + p, 8);
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
	m_gif_packet->Expand(regId);
}

void Dialogs::GSDumpDialog::ParsePacket(wxTreeEvent& event)
{
	GenPacketInfo(m_thread->m_dump_file->GetPackets()[wxAtoi(m_gif_list->GetItemText(event.GetItem()).BeforeFirst('-'))]);
}

void Dialogs::GSDumpDialog::ParseTreeReg(wxTreeItemId& id, GIFReg reg, u128 data, bool packed)
{
	wxTreeItemId rootId = m_gif_packet->AppendItem(id, wxString(GSDumpTypes::GetName(reg)));
	switch (reg)
	{
		case GIFReg::PRIM:
			ParseTreePrim(rootId, data.lo);
			break;
		case GIFReg::RGBAQ:
		{
			u32 r, g, b, a;
			float q;

			if (packed)
			{
				r = GetBits(data,  0, 8);
				g = GetBits(data, 32, 8);
				b = GetBits(data, 64, 8);
				a = GetBits(data, 96, 8);
				q = m_stored_q;
			}
			else
			{
				r = GetBits(data,  0, 8);
				g = GetBits(data,  8, 8);
				b = GetBits(data, 16, 8);
				a = GetBits(data, 24, 8);
				q = BitCast<float>(data._u32[1]);
			}

			m_gif_packet->SetItemText(rootId, wxString::Format("%s - #%02x%02x%02x%02x, %g", GSDumpTypes::GetName(reg), r, g, b, a, q));
			m_gif_packet->AppendItem(rootId, wxString::Format("R = %u", r));
			m_gif_packet->AppendItem(rootId, wxString::Format("G = %u", g));
			m_gif_packet->AppendItem(rootId, wxString::Format("B = %u", b));
			m_gif_packet->AppendItem(rootId, wxString::Format("A = %u", a));
			m_gif_packet->AppendItem(rootId, wxString::Format("Q = %g", q));
			break;
		}
		case GIFReg::ST:
		{
			float s = BitCast<float>(data._u32[0]);
			float t = BitCast<float>(data._u32[1]);
			m_gif_packet->AppendItem(rootId, wxString::Format("S = %g", s));
			m_gif_packet->AppendItem(rootId, wxString::Format("T = %g", t));
			if (packed)
			{
				m_stored_q = BitCast<float>(data._u32[2]);
				m_gif_packet->AppendItem(rootId, wxString::Format("Q = %g", m_stored_q));
				m_gif_packet->SetItemText(rootId, wxString::Format("%s - (%g, %g, %g)", GSDumpTypes::GetName(reg), s, t, m_stored_q));
			}
			else
			{
				m_gif_packet->SetItemText(rootId, wxString::Format("%s - (%g, %g)", GSDumpTypes::GetName(reg), s, t));
			}
			break;
		}
		case GIFReg::UV:
		{
			float u = static_cast<float>(GetBits(data, 0, 14)) / 16.f;
			float v = static_cast<float>(GetBits(data, packed ? 32 : 16, 14)) / 16.f;
			m_gif_packet->AppendItem(rootId, wxString::Format("U = %g", u));
			m_gif_packet->AppendItem(rootId, wxString::Format("V = %g", v));
			m_gif_packet->SetItemText(rootId, wxString::Format("%s - (%g, %g)", GSDumpTypes::GetName(reg), u, v));
			break;
		}
		case GIFReg::XYZF2:
		case GIFReg::XYZF3:
		{
			const char* name = GSDumpTypes::GetName(reg);
			if (packed && (reg == GIFReg::XYZF2) && GetBits(data, 111, 1))
				name = GSDumpTypes::GetName(GIFReg::XYZF3);

			float x, y;
			u32 z, f;

			if (packed)
			{
				x = static_cast<float>(GetBits(data,  0, 16)) / 16.0;
				y = static_cast<float>(GetBits(data, 32, 16)) / 16.0;
				z = GetBits(data, 68, 24);
				f = GetBits(data, 100, 8);
			}
			else
			{
				x = static_cast<float>(GetBits(data,  0, 16)) / 16.0;
				y = static_cast<float>(GetBits(data, 16, 16)) / 16.0;
				z = GetBits(data, 32, 24);
				f = GetBits(data, 56, 8);
			}

			m_gif_packet->SetItemText(rootId, wxString::Format("%s - (%g, %g, %u, %u)", name, x, y, z, f));
			m_gif_packet->AppendItem(rootId, wxString::Format("X = %g", x));
			m_gif_packet->AppendItem(rootId, wxString::Format("Y = %g", y));
			m_gif_packet->AppendItem(rootId, wxString::Format("Z = %u", z));
			m_gif_packet->AppendItem(rootId, wxString::Format("F = %u", f));
			break;
		}
		case GIFReg::XYZ2:
		case GIFReg::XYZ3:
		{
			const char* name = GSDumpTypes::GetName(reg);
			if (packed && (reg == GIFReg::XYZ2) && GetBits(data, 111, 1))
				name = GSDumpTypes::GetName(GIFReg::XYZ3);

			float x, y;
			u32 z;
			std::vector<wxString> xyz_infos(3);
			if (packed)
			{
				x = static_cast<float>(GetBits(data,  0, 16)) / 16.0;
				y = static_cast<float>(GetBits(data, 32, 16)) / 16.0;
				z = data._u32[2];
			}
			else
			{
				x = static_cast<float>(GetBits(data,  0, 16)) / 16.0;
				y = static_cast<float>(GetBits(data, 16, 16)) / 16.0;
				z = data._u32[1];
			}

			m_gif_packet->SetItemText(rootId, wxString::Format("%s - (%g, %g, %u)", name, x, y, z));
			m_gif_packet->AppendItem(rootId, wxString::Format("X = %g", x));
			m_gif_packet->AppendItem(rootId, wxString::Format("Y = %g", y));
			m_gif_packet->AppendItem(rootId, wxString::Format("Z = %u", z));
			break;
		}
		case GIFReg::TEX0_1:
		case GIFReg::TEX0_2:
		{
			u32 psm = GetBits(data, 20, 6);
			u32 tw = GetBits(data, 26, 4);
			u32 th = GetBits(data, 30, 4);

			m_gif_packet->SetItemText(rootId, wxString::Format("%s - %ux%u %s", GSDumpTypes::GetName(reg), 1 << tw, 1 << th, GetNameTEXPSM(psm)));
			m_gif_packet->AppendItem(rootId, wxString::Format("TBP0 = %u", GetBits(data, 0, 14)));
			m_gif_packet->AppendItem(rootId, wxString::Format("TBW = %u",  GetBits(data, 14, 6)));
			m_gif_packet->AppendItem(rootId, wxString::Format("PSM = %s",  GetNameTEXPSM(psm)));
			m_gif_packet->AppendItem(rootId, wxString::Format("TW = %u",   tw));
			m_gif_packet->AppendItem(rootId, wxString::Format("TH = %u",   th));
			m_gif_packet->AppendItem(rootId, wxString::Format("TCC = %s",  GetNameTEXTCC (GetBits(data, 34, 1))));
			m_gif_packet->AppendItem(rootId, wxString::Format("TFX = %s",  GetNameTEXTFX (GetBits(data, 35, 2))));
			m_gif_packet->AppendItem(rootId, wxString::Format("CBP = %u",  GetBits(data, 37, 14)));
			m_gif_packet->AppendItem(rootId, wxString::Format("CPSM = %s", GetNameTEXCPSM(GetBits(data, 51, 4))));
			m_gif_packet->AppendItem(rootId, wxString::Format("CSM = %s",  GetNameTEXCSM (GetBits(data, 55, 1))));
			m_gif_packet->AppendItem(rootId, wxString::Format("CSA = %u",  GetBits(data, 56, 5)));
			m_gif_packet->AppendItem(rootId, wxString::Format("CLD = %u",  GetBits(data, 61, 3)));
			break;
		}
		case GIFReg::FOG:
		{
			u32 f = GetBits(data, packed ? 100 : 56, 8);
			m_gif_packet->AppendItem(rootId, wxString::Format("F = %u", f));
			m_gif_packet->SetItemText(rootId, wxString::Format("%s - %u", GSDumpTypes::GetName(reg), f));
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
	m_gif_packet->Expand(id);
}

void Dialogs::GSDumpDialog::ProcessDumpEvent(const GSDumpFile::GSData& event, u8* regs)
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
					memcpy(data.get(), event.data + addr, event.length);
					GSgifTransfer1((u8*)data.get(), addr);
					break;
				}
				case GSTransferPath::Path1New:
					GSgifTransfer((u8*)event.data, event.length / 16);
					break;
				case GSTransferPath::Path2:
					GSgifTransfer2((u8*)event.data, event.length / 16);
					break;
				case GSTransferPath::Path3:
					GSgifTransfer3((u8*)event.data, event.length / 16);
					break;
				default:
					break;
			}
			break;
		}
		case GSType::VSync:
		{
			GSvsync((*((int*)(regs + 4096)) & 0x2000) > 0 ? (u8)0 : (u8)1, false);
			g_FrameCount++;
			break;
		}
		case GSType::ReadFIFO2:
		{
			std::unique_ptr<u8[]> arr(new u8[*((int*)event.data) * 16]);
			GetMTGS().InitAndReadFIFO(arr.get(), *((int*)event.data));
			break;
		}
		case GSType::Registers:
			memcpy(regs, event.data, 8192);
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
	m_dump_file = nullptr;

	wxCommandEvent event(EVT_CLOSE_DUMP);
	wxPostEvent(m_root_window, event);
}

void Dialogs::GSDumpDialog::GSThread::ExecuteTaskInThread()
{
	if (!m_dump_file->ReadFile())
	{
		OnStop();
		return;
	}

	GSRendererType renderer = g_Conf->EmuOptions.GS.Renderer;
	if (m_renderer > 0 && static_cast<size_t>(m_renderer) <= std::size(s_renderer_overrides))
		renderer = s_renderer_overrides[m_renderer - 1];

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

	Pcsx2Config::GSOptions config(g_Conf->EmuOptions.GS);
	config.MaskUserHacks();
	config.MaskUpscalingHacks();
	if (!m_dump_file->GetSerial().empty())
	{
		if (const GameDatabaseSchema::GameEntry* entry = GameDatabase::findGame(m_dump_file->GetSerial()); entry)
		{
			// apply hardware fixes to config before opening (e.g. tex in rt)
			entry->applyGSHardwareFixes(config);
		}
	}

	u8 regs[8192] = {};
	if (!m_dump_file->GetRegsData().empty())
		std::memcpy(regs, m_dump_file->GetRegsData().data(), std::min(m_dump_file->GetRegsData().size(), sizeof(regs)));

	if (!GSopen(config, renderer, regs))
	{
		OnStop();
		return;
	}

	GSsetGameCRC((int)m_dump_file->GetCRC(), 0);

	freezeData fd = {static_cast<int>(m_dump_file->GetStateData().size()),
		const_cast<u8*>(m_dump_file->GetStateData().data())};
	if (GSfreeze(FreezeAction::Load, &fd))
	{
		OnStop();
		return;
	}

	GSvsync(1, false);
	GSreset(false);
	GSfreeze(FreezeAction::Load, &fd);

	size_t i = 0;
	m_debug_index = 0;
	size_t debug_idx = 0;

	GSDump::isRunning = true;
	while (GSDump::isRunning)
	{
		if (m_debug)
		{
			if (m_root_window->m_button_events.size() > 0)
			{
				switch (m_root_window->m_button_events[0].btn)
				{
					case Step:
						if (debug_idx >= m_dump_file->GetPackets().size())
							debug_idx = 0;
						m_debug_index = debug_idx;
						break;
					case RunCursor:
						m_debug_index = m_root_window->m_button_events[0].index;
						if (debug_idx > m_debug_index)
							debug_idx = 0;
						break;
					case RunVSync:
						if (debug_idx >= m_dump_file->GetPackets().size())
							debug_idx = 1;
						if ((debug_idx + 1) < m_dump_file->GetPackets().size())
						{
							auto it = std::find_if(m_dump_file->GetPackets().begin() + debug_idx + 1, m_dump_file->GetPackets().end(), [](const GSDumpFile::GSData& gs) { return gs.id == GSType::Registers; });
							if (it != std::end(m_dump_file->GetPackets()))
								m_debug_index = std::distance(m_dump_file->GetPackets().begin(), it);
						}
						break;
				}
				m_root_window->m_button_events.erase(m_root_window->m_button_events.begin());

				if (debug_idx <= m_debug_index)
				{
					while (debug_idx <= m_debug_index)
					{
						m_root_window->ProcessDumpEvent(m_dump_file->GetPackets()[debug_idx++], regs);
					}
					if ((debug_idx + 1) < m_dump_file->GetPackets().size())
					{
						auto it = std::find_if(m_dump_file->GetPackets().begin() + debug_idx + 1, m_dump_file->GetPackets().end(), [](const GSDumpFile::GSData& gs) { return gs.id == GSType::Registers; });
						if (it != std::end(m_dump_file->GetPackets()))
							m_root_window->ProcessDumpEvent(*it, regs);
					}
				}

				// do vsync
				m_root_window->ProcessDumpEvent({GSType::VSync, 0, 0, GSTransferPath::Dummy}, regs);
			}
		}
		else if (m_dump_file->GetPackets().size())
		{
			while (i < m_dump_file->GetPackets().size())
			{
				const GSDumpFile::GSData& packet = m_dump_file->GetPackets()[i++];
				m_root_window->ProcessDumpEvent(packet, regs);
				if (packet.id == GSType::VSync)
				{
					if (m_frame_ticks)
					{
						// Frame limiter
						u64 now = GetCPUTicks();
						s64 ms = GetTickFrequency() / 1000;
						s64 sleep = m_next_frame_time - now - ms;
						if (sleep > ms)
							Threading::Sleep(sleep / ms);
						while ((now = GetCPUTicks()) < m_next_frame_time)
							ShortSpin();
						m_next_frame_time = std::max(now, m_next_frame_time + m_frame_ticks);
					}
					break;
				}
			}

			if (i >= m_dump_file->GetPackets().size())
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
