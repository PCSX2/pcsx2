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

#pragma once

#include "gui/App.h"
#include "ConfigurationDialog.h"
#include "gui/Panels/ConfigurationPanels.h"
#include "GS/GSLzma.h"

#include <wx/wizard.h>
#include <wx/treectrl.h>
#include <wx/fswatcher.h>

class FirstTimeWizard : public wxWizard
{
	typedef wxWizard _parent;

protected:
	wxWizardPageSimple& m_page_intro;
	wxWizardPageSimple& m_page_bios;

	wxPanelWithHelpers& m_panel_Intro;
	Panels::BiosSelectorPanel& m_panel_BiosSel;

public:
	FirstTimeWizard(wxWindow* parent);
	virtual ~FirstTimeWizard() = default;

	wxWizardPage* GetFirstPage() const { return &m_page_intro; }

	int ShowModal();

protected:
	virtual void OnPageChanging(wxWizardEvent& evt);
	virtual void OnPageChanged(wxWizardEvent& evt);
	virtual void OnDoubleClicked(wxCommandEvent& evt);

	void OnRestartWizard(wxCommandEvent& evt);
};


namespace Dialogs
{
	class AboutBoxDialog : public wxDialogWithHelpers
	{
	public:
		AboutBoxDialog(wxWindow* parent = NULL);
		virtual ~AboutBoxDialog() = default;

		static wxString GetNameStatic() { return L"AboutBox"; }
		wxString GetDialogName() const { return GetNameStatic(); }
	};

	class GSDumpDialog : public wxDialogWithHelpers
	{
	public:
		GSDumpDialog(wxWindow* parent = NULL);
		virtual ~GSDumpDialog() = default;

		static wxString GetNameStatic()
		{
			return L"GS Debugger";
		}
		wxString GetDialogName() const
		{
			return GetNameStatic();
		}

	protected:
		wxListView* m_dump_list;
		wxStaticBitmap* m_preview_image;
		wxString m_selected_dump;
		wxCheckBox* m_debug_mode;
		wxRadioBox* m_renderer_overrides;
		wxSpinCtrl* m_framerate_selector;
		wxTreeCtrl* m_gif_list;
		wxTreeCtrl* m_gif_packet;
		wxButton* m_start;
		wxButton* m_step;
		wxButton* m_selection;
		wxButton* m_vsync;
		wxButton* m_settings;
		wxButton* m_run;
		long m_focused_dump;
		wxFileSystemWatcher m_fs_watcher;

		void GetDumpsList();
		void UpdateFramerate(int val);
		void UpdateFramerate(wxCommandEvent& evt);
		void SelectedDump(wxListEvent& evt);
		void RunDump(wxCommandEvent& event);
		void ToStart(wxCommandEvent& event);
		void StepPacket(wxCommandEvent& event);
		void ToCursor(wxCommandEvent& event);
		void ToVSync(wxCommandEvent& event);
		void OpenSettings(wxCommandEvent& event);
		void ParsePacket(wxTreeEvent& event);
		void CheckDebug(wxCommandEvent& event);
		void PathChanged(wxFileSystemWatcherEvent& event);
		enum
		{
			ID_DUMP_LIST,
			ID_RUN_DUMP,
			ID_RUN_START,
			ID_RUN_STEP,
			ID_RUN_CURSOR,
			ID_RUN_VSYNC,
			ID_SEL_PACKET,
			ID_DEBUG_MODE,
			ID_SETTINGS,
			ID_FRAMERATE,
		};

		enum ButtonState
		{
			Step,
			RunCursor,
			RunVSync
		};

		struct GSEvent
		{
			ButtonState btn;
			int index;
		};
		std::vector<GSEvent> m_button_events;
		std::vector<wxTreeItemId> m_gif_items;

		float m_stored_q = 1.0;
		void ProcessDumpEvent(const GSDumpFile::GSData& event, u8* regs);
		u32 ReadPacketSize(const void* packet);
		void GenPacketList();
		void GenPacketInfo(const GSDumpFile::GSData& dump);
		void ParseTransfer(wxTreeItemId& id, const u8* data);
		void ParseTreeReg(wxTreeItemId& id, GSDumpTypes::GIFReg reg, u128 data, bool packed);
		void ParseTreePrim(wxTreeItemId& id, u32 prim);
		void CloseDump(wxCommandEvent& event);
		class GSThread : public pxThread
		{
		protected:
			// parent thread
			typedef pxThread _parent;
			void ExecuteTaskInThread();
			void OnStop();
			GSDumpDialog* m_root_window;

		public:
			int m_renderer = 0;
			u64 m_frame_ticks = 0;
			u64 m_next_frame_time = 0;
			bool m_debug = false;
			size_t m_debug_index;
			std::unique_ptr<GSDumpFile> m_dump_file;
			GSThread(GSDumpDialog* dlg);
			virtual ~GSThread();
		};
		std::unique_ptr<GSThread> m_thread;
	};


	class PickUserModeDialog : public BaseApplicableDialog
	{
	protected:
		Panels::DocsFolderPickerPanel* m_panel_usersel;
		Panels::LanguageSelectionPanel* m_panel_langsel;

	public:
		PickUserModeDialog(wxWindow* parent);
		virtual ~PickUserModeDialog() = default;

	protected:
		void OnOk_Click(wxCommandEvent& evt);
	};


	class ImportSettingsDialog : public wxDialogWithHelpers
	{
	public:
		ImportSettingsDialog(wxWindow* parent);
		virtual ~ImportSettingsDialog() = default;

	protected:
		void OnImport_Click(wxCommandEvent& evt);
		void OnOverwrite_Click(wxCommandEvent& evt);
	};

	class AssertionDialog : public wxDialogWithHelpers
	{
	public:
		AssertionDialog(const wxString& text, const wxString& stacktrace);
		virtual ~AssertionDialog() = default;
	};

	class PINEDialog : public wxDialogWithHelpers
	{
	public:
		PINEDialog(wxWindow* parent = NULL);
		virtual ~PINEDialog() = default;

		void OnConfirm(wxCommandEvent& evt);
		static wxString GetNameStatic() { return L"PINESettings"; }
		wxString GetDialogName() const { return GetNameStatic(); }
	};
} // namespace Dialogs

wxWindowID pxIssueConfirmation(wxDialogWithHelpers& confirmDlg, const MsgButtons& buttons);
wxWindowID pxIssueConfirmation(wxDialogWithHelpers& confirmDlg, const MsgButtons& buttons, const wxString& disablerKey);

namespace GSDump
{
	extern bool isRunning;
}
