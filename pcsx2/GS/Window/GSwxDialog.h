/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GS.h"
#include "GSSetting.h"

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/wrapsizer.h>
#include <wx/statline.h>
#include <wx/filepicker.h>
#include <vector>
#include <functional>

class GSUIElementHolder
{
	class GSwxChoice : public wxChoice
	{
	public:
		const std::vector<GSSetting>& settings;

		GSwxChoice(
			wxWindow* parent,
			wxWindowID id,
			const wxPoint& pos,
			const wxSize& size,
			const wxArrayString& choices,
			const std::vector<GSSetting>* settings,
			long style = 0,
			const wxValidator& validator = wxDefaultValidator,
			const wxString& name = wxChoiceNameStr)
			: wxChoice(parent, id, pos, size, choices, style, validator, name)
			, settings(*settings)
		{
		}
	};

	struct UIElem
	{
		enum class Type
		{
			CheckBox,
			Choice,
			Spin,
			Slider,
			File,
			Directory,
		};

		Type type;
		wxControl* control;
		const char* config;
		std::function<bool()> prereq;

		UIElem(Type type, wxControl* control, const char* config, std::function<bool()> prereq)
			: type(type), control(control), config(config), prereq(prereq)
		{
		}
	};

	static bool noPrereq() { return true; }

	wxWindow* m_window;
	std::vector<UIElem> m_elems;

	wxStaticText* addWithLabel(wxControl* control, UIElem::Type type, wxSizer* sizer, const char* label, const char* config_name, int tooltip, std::function<bool()> prereq, wxSizerFlags flags = wxSizerFlags().Centre().Expand().Left());

public:
	GSUIElementHolder(wxWindow* window);
	wxCheckBox* addCheckBox(wxSizer* sizer, const char* label, const char* config_name, int tooltip = -1, std::function<bool()> prereq = noPrereq);
	std::pair<wxChoice*, wxStaticText*> addComboBoxAndLabel(wxSizer* sizer, const char* label, const char* config_name, const std::vector<GSSetting>* settings, int tooltip = -1, std::function<bool()> prereq = noPrereq);
	wxSpinCtrl* addSpin(wxSizer* sizer, const char* config_name, int min, int max, int initial, int tooltip = -1, std::function<bool()> prereq = noPrereq);
	std::pair<wxSpinCtrl*, wxStaticText*> addSpinAndLabel(wxSizer* sizer, const char* label, const char* config_name, int min, int max, int initial, int tooltip = -1, std::function<bool()> prereq = noPrereq);
	std::pair<wxSlider*, wxStaticText*> addSliderAndLabel(wxSizer* sizer, const char* label, const char* config_name, int min, int max, int initial, int tooltip = -1, std::function<bool()> prereq = noPrereq);
	std::pair<wxFilePickerCtrl*, wxStaticText*> addFilePickerAndLabel(wxSizer* sizer, const char* label, const char* config_name, int tooltip = -1, std::function<bool()> prereq = noPrereq);
	std::pair<wxDirPickerCtrl*, wxStaticText*> addDirPickerAndLabel(wxSizer* sizer, const char* label, const char* config_name, int tooltip = -1, std::function<bool()> prereq = noPrereq);

	void Load();
	void Save();
	void Update();
	void DisableAll();
};

namespace GSSettingsDialog
{
	class RendererTab : public wxPanel
	{
	public:
		GSUIElementHolder m_ui;
		wxChoice* m_internal_resolution;
		bool m_is_hardware = false;
		bool m_is_native_res = false;

		RendererTab(wxWindow* parent);
		void Load() { m_ui.Load(); }
		void Save() { m_ui.Save(); }
		void DoUpdate() { m_ui.Update(); }
	};

	class HacksTab : public wxPanel
	{
	public:
		GSUIElementHolder m_ui;
		wxSpinCtrl *skip_x_spin, *skip_y_spin;
		bool m_is_hardware = false;
		bool m_is_native_res = false;

		HacksTab(wxWindow* parent);
		void Load() { m_ui.Load(); }
		void Save() { m_ui.Save(); }
		void DoUpdate();
	};

	class DebugTab : public wxPanel
	{
	public:
		GSUIElementHolder m_ui;
		bool m_is_ogl_hw = false;
		bool m_is_vk_hw = false;

		DebugTab(wxWindow* parent);
		void Load() { m_ui.Load(); }
		void Save() { m_ui.Save(); }
		void DoUpdate();
	};

	class RecTab : public wxPanel
	{
	public:
		GSUIElementHolder m_ui;

		RecTab(wxWindow* parent);
		void Load() { m_ui.Load(); }
		void Save() { m_ui.Save(); }
		void DoUpdate() { m_ui.Update(); }
	};

	class PostTab : public wxPanel
	{
	public:
		GSUIElementHolder m_ui;
		bool m_is_vk_hw = false;

		PostTab(wxWindow* parent);
		void Load() { m_ui.Load(); }
		void Save() { m_ui.Save(); }
		void DoUpdate() { m_ui.Update(); }
	};

	class OSDTab : public wxPanel
	{
	public:
		GSUIElementHolder m_ui;

		OSDTab(wxWindow* parent);
		void Load() { m_ui.Load(); }
		void Save() { m_ui.Save(); }
		void DoUpdate() { m_ui.Update(); }
	};

	class Dialog : public wxDialog
	{
		GSUIElementHolder m_ui;

		wxBoxSizer* m_top_box;
		wxChoice* m_renderer_select;
		wxChoice* m_adapter_select;
		wxChoice* m_bifilter_select;
		RendererTab* m_renderer_panel;
		HacksTab* m_hacks_panel;
		DebugTab* m_debug_panel;
		RecTab* m_rec_panel;
		PostTab* m_post_panel;
		OSDTab* m_osd_panel;

	public:
		Dialog();
		~Dialog();
		void Load();
		void Save();
		void Update();
		void CallUpdate(wxCommandEvent& event);
		void OnRendererChange(wxCommandEvent& event);
		void RendererChange();
		GSRendererType GetSelectedRendererType();
	};

} // namespace GSSettingsDialog

extern bool RunwxDialog();
