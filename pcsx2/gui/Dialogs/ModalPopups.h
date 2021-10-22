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
#include "common/pxStreams.h"
#include "GS/GSLzma.h"

#include <wx/wizard.h>
#include <wx/treectrl.h>
#include <wx/fswatcher.h>

#define GEN_REG_ENUM_CLASS_CONTENT(ClassName, EntryName, Value) \
	EntryName = Value,

#define GEN_REG_GETNAME_CONTENT(ClassName, EntryName, Value) \
	case ClassName::EntryName: \
		return #EntryName;

#define GEN_REG_ENUM_CLASS_AND_GETNAME(Macro, ClassName, Type, DefaultString) \
	enum class ClassName : Type \
	{ \
		Macro(GEN_REG_ENUM_CLASS_CONTENT) \
	}; \
	static constexpr const char* GetName(ClassName reg) \
	{ \
		switch (reg) \
		{ \
			Macro(GEN_REG_GETNAME_CONTENT) \
			default: \
				return DefaultString; \
		} \
	}

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

		// clang-format off

#define DEF_GSType(X) \
	X(GSType, Transfer,  0) \
	X(GSType, VSync,     1) \
	X(GSType, ReadFIFO2, 2) \
	X(GSType, Registers, 3)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GSType, GSType, u8, "UnknownType")
#undef DEF_GSType

#define DEF_GSTransferPath(X) \
	X(GSTransferPath, Path1Old, 0) \
	X(GSTransferPath, Path2,    1) \
	X(GSTransferPath, Path3,    2) \
	X(GSTransferPath, Path1New, 3) \
	X(GSTransferPath, Dummy,    4)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GSTransferPath, GSTransferPath, u8, "UnknownPath")
#undef DEF_GSTransferPath

#define DEF_GIFFlag(X) \
	X(GIFFlag, PACKED,  0) \
	X(GIFFlag, REGLIST, 1) \
	X(GIFFlag, IMAGE,   2) \
	X(GIFFlag, IMAGE2,  3)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GIFFlag, GIFFlag, u8, "UnknownFlag")
#undef DEF_GifFlag

#define DEF_GIFReg(X) \
	X(GIFReg, PRIM,       0x00) \
	X(GIFReg, RGBAQ,      0x01) \
	X(GIFReg, ST,         0x02) \
	X(GIFReg, UV,         0x03) \
	X(GIFReg, XYZF2,      0x04) \
	X(GIFReg, XYZ2,       0x05) \
	X(GIFReg, TEX0_1,     0x06) \
	X(GIFReg, TEX0_2,     0x07) \
	X(GIFReg, CLAMP_1,    0x08) \
	X(GIFReg, CLAMP_2,    0x09) \
	X(GIFReg, FOG,        0x0a) \
	X(GIFReg, XYZF3,      0x0c) \
	X(GIFReg, XYZ3,       0x0d) \
	X(GIFReg, AD,         0x0e) \
	X(GIFReg, NOP,        0x0f) \
	X(GIFReg, TEX1_1,     0x14) \
	X(GIFReg, TEX1_2,     0x15) \
	X(GIFReg, TEX2_1,     0x16) \
	X(GIFReg, TEX2_2,     0x17) \
	X(GIFReg, XYOFFSET_1, 0x18) \
	X(GIFReg, XYOFFSET_2, 0x19) \
	X(GIFReg, PRMODECONT, 0x1a) \
	X(GIFReg, PRMODE,     0x1b) \
	X(GIFReg, TEXCLUT,    0x1c) \
	X(GIFReg, SCANMSK,    0x22) \
	X(GIFReg, MIPTBP1_1,  0x34) \
	X(GIFReg, MIPTBP1_2,  0x35) \
	X(GIFReg, MIPTBP2_1,  0x36) \
	X(GIFReg, MIPTBP2_2,  0x37) \
	X(GIFReg, TEXA,       0x3b) \
	X(GIFReg, FOGCOL,     0x3d) \
	X(GIFReg, TEXFLUSH,   0x3f) \
	X(GIFReg, SCISSOR_1,  0x40) \
	X(GIFReg, SCISSOR_2,  0x41) \
	X(GIFReg, ALPHA_1,    0x42) \
	X(GIFReg, ALPHA_2,    0x43) \
	X(GIFReg, DIMX,       0x44) \
	X(GIFReg, DTHE,       0x45) \
	X(GIFReg, COLCLAMP,   0x46) \
	X(GIFReg, TEST_1,     0x47) \
	X(GIFReg, TEST_2,     0x48) \
	X(GIFReg, PABE,       0x49) \
	X(GIFReg, FBA_1,      0x4a) \
	X(GIFReg, FBA_2,      0x4b) \
	X(GIFReg, FRAME_1,    0x4c) \
	X(GIFReg, FRAME_2,    0x4d) \
	X(GIFReg, ZBUF_1,     0x4e) \
	X(GIFReg, ZBUF_2,     0x4f) \
	X(GIFReg, BITBLTBUF,  0x50) \
	X(GIFReg, TRXPOS,     0x51) \
	X(GIFReg, TRXREG,     0x52) \
	X(GIFReg, TRXDIR,     0x53) \
	X(GIFReg, HWREG,      0x54) \
	X(GIFReg, SIGNAL,     0x60) \
	X(GIFReg, FINISH,     0x61) \
	X(GIFReg, LABEL,      0x62)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GIFReg, GIFReg, u8, "UnknownReg")
#undef DEF_GIFReg

		// clang-format on

		struct GSData
		{
			GSType id;
			std::unique_ptr<char[]> data;
			int length;
			GSTransferPath path;
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
		std::vector<GSData> m_dump_packets;
		std::vector<wxTreeItemId> m_gif_items;

		float m_stored_q = 1.0;
		void ProcessDumpEvent(const GSData& event, char* regs);
		u32 ReadPacketSize(const void* packet);
		void GenPacketList();
		void GenPacketInfo(GSData& dump);
		void ParseTransfer(wxTreeItemId& id, char* data);
		void ParseTreeReg(wxTreeItemId& id, GIFReg reg, u128 data, bool packed);
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
