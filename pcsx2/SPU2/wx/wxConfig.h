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

#pragma once

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/wrapsizer.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>

#if defined(__unix__) || defined(__APPLE__)
#include <SDL.h>
#include <SDL_audio.h>
#include "Config.h"
#endif

class MixerTab : public wxPanel
{
public:
	wxChoice *m_inter_select, *m_audio_select;
	wxSlider *m_latency_slider, *m_volume_slider;
	wxStaticBoxSizer *m_volume_box, *m_latency_box;
	wxBoxSizer* m_audio_box;

	MixerTab(wxWindow* parent);
	void Load();
	void Save();
	void Update();
	void CallUpdate(wxCommandEvent& event);
};

class SyncTab : public wxPanel
{
public:
	wxChoice* m_sync_select;
	wxButton *launch_adv_dialog, *reset_button;
	wxSpinCtrl *seq_spin, *seek_spin, *overlap_spin;

	SyncTab(wxWindow* parent);
	void Load();
	void Save();
	void Update();
	void CallUpdate(wxCommandEvent& event);
	void OnButtonClicked(wxCommandEvent& event);
};

class DebugTab : public wxPanel
{
public:
	wxCheckBox* debug_check;
	wxButton* launch_debug_dialog;

	wxStaticBoxSizer *m_console_box, *m_log_only_box, *dump_box;
	wxCheckBox* show_check;
	wxCheckBox *key_check, *voice_check, *dma_check, *autodma_check, *buffer_check, *adpcm_check;
	wxCheckBox *dma_actions_check, *dma_writes_check, *auto_output_check;
	wxCheckBox *core_voice_check, *memory_check, *register_check;

	DebugTab(wxWindow* parent);
	void Load();
	void Save();
	void Update();
	void CallUpdate(wxCommandEvent& event);
};

class Dialog : public wxDialog
{
	wxBoxSizer *m_top_box, *m_cubeb_box;
	wxChoice *m_module_select, *m_cubeb_select;
	wxStaticText *m_cubeb_text;

	MixerTab* m_mixer_panel;
	SyncTab* m_sync_panel;
	DebugTab* m_debug_panel;

public:
	Dialog();
	~Dialog();
	void Display();
	void Load();
	void Save();
	void Reconfigure();
	void CallReconfigure(wxCommandEvent& event);
};
