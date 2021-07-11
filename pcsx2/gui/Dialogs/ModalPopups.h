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

#include "App.h"
#include "ConfigurationDialog.h"
#include "Panels/ConfigurationPanels.h"
#include "Utilities/pxStreams.h"

#include <wx/wizard.h>
#include <wx/treectrl.h>
#include <wx/fswatcher.h>

// clang-format off
#define GSDUMP_GIFREG(X) \
	X(PRIM, 0x00)        \
	X(RGBAQ, 0x01)       \
	X(ST, 0x02)          \
	X(UV, 0x03)          \
	X(XYZF2, 0x04)       \
	X(XYZ2, 0x05)        \
	X(TEX0_1, 0x06)      \
	X(TEX0_2, 0x07)      \
	X(CLAMP_1, 0x08)     \
	X(CLAMP_2, 0x09)     \
	X(FOG, 0x0a)         \
	X(XYZF3, 0x0c)       \
	X(XYZ3, 0x0d)        \
	X(AD, 0x0e)          \
	X(NOP, 0x0f)         \
	X(TEX1_1, 0x14)      \
	X(TEX1_2, 0x15)      \
	X(TEX2_1, 0x16)      \
	X(TEX2_2, 0x17)      \
	X(XYOFFSET_1, 0x18)  \
	X(XYOFFSET_2, 0x19)  \
	X(PRMODECONT, 0x1a)  \
	X(PRMODE, 0x1b)      \
	X(TEXCLUT, 0x1c)     \
	X(SCANMSK, 0x22)     \
	X(MIPTBP1_1, 0x34)   \
	X(MIPTBP1_2, 0x35)   \
	X(MIPTBP2_1, 0x36)   \
	X(MIPTBP2_2, 0x37)   \
	X(TEXA, 0x3b)        \
	X(FOGCOL, 0x3d)      \
	X(TEXFLUSH, 0x3f)    \
	X(SCISSOR_1, 0x40)   \
	X(SCISSOR_2, 0x41)   \
	X(ALPHA_1, 0x42)     \
	X(ALPHA_2, 0x43)     \
	X(DIMX, 0x44)        \
	X(DTHE, 0x45)        \
	X(COLCLAMP, 0x46)    \
	X(TEST_1, 0x47)      \
	X(TEST_2, 0x48)      \
	X(PABE, 0x49)        \
	X(FBA_1, 0x4a)       \
	X(FBA_2, 0x4b)       \
	X(FRAME_1, 0x4c)     \
	X(FRAME_2, 0x4d)     \
	X(ZBUF_1, 0x4e)      \
	X(ZBUF_2, 0x4f)      \
	X(BITBLTBUF, 0x50)   \
	X(TRXPOS, 0x51)      \
	X(TRXREG, 0x52)      \
	X(TRXDIR, 0x53)      \
	X(HWREG, 0x54)       \
	X(SIGNAL, 0x60)      \
	X(FINISH, 0x61)      \
	X(LABEL, 0x62)
// clang-format on

#define GSDUMP_GIFREG_NAME GIFReg
#define GSDUMP_GIFREG_TYPE u8

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
			ID_SETTINGS
		};
		enum GSType : u8
		{
			Transfer = 0,
			VSync = 1,
			ReadFIFO2 = 2,
			Registers = 3
		};
		static constexpr const char* GSTypeNames[256] = {
			"Transfer",
			"VSync",
			"ReadFIFO2",
			"Registers"
		};
		enum GSTransferPath : u8
		{
			Path1Old = 0,
			Path2 = 1,
			Path3 = 2,
			Path1New = 3,
			Dummy = 4
		};
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
		static constexpr const char* GSTransferPathNames[256] = {
			"Path1Old",
			"Path2",
			"Path3",
			"Path1New",
			"Dummy"
		};
		enum GifFlag : u8
		{
			GIF_FLG_PACKED = 0,
			GIF_FLG_REGLIST = 1,
			GIF_FLG_IMAGE = 2,
			GIF_FLG_IMAGE2 = 3
		};
		static constexpr const char* GifFlagNames[256] = {
			"GIF_FLG_PACKED",
			"GIF_FLG_REGLIST",
			"GIF_FLG_IMAGE",
			"GIF_FLG_IMAGE2"
		};
		static constexpr const char* GsPrimNames[256] = {
			"GS_POINTLIST",
			"GS_LINELIST",
			"GS_LINESTRIP",
			"GS_TRIANGLELIST",
			"GS_TRIANGLESTRIP",
			"GS_TRIANGLEFAN",
			"GS_SPRITE",
			"GS_INVALID"
		};
		static constexpr const char* GsIIPNames[256] = {
			"FlatShading",
			"Gouraud"
		};
		static constexpr const char* GsFSTNames[256] = {
			"STQValue",
			"UVValue"
		};
		static constexpr const char* GsCTXTNames[256] = {
			"Context1",
			"Context2"
		};
		static constexpr const char* GsFIXNames[256] = {
			"Unfixed",
			"Fixed"
		};
		static constexpr const char* TEXTCCNames[256] = {
			"RGB",
			"RGBA"
		};
		static constexpr const char* TEXTFXNames[256] = {
			"MODULATE",
			"DECAL",
			"HIGHLIGHT",
			"HIGHLIGHT2"
		};
		static constexpr const char* TEXCSMNames[256] = {
			"CSM1",
			"CSM2"
		};
		// a GNU extension exists to initialize array at given indices which would be 
		// exactly what we need here but, obviously, MSVC is at it again to make our 
		// life harder than sandpaper on your skin, so we make do
		// clang-format off
		static constexpr const char* TEXCPSMNames[256] = {
			"PSMCT32",
			"",
			"PSMCT16",
			"","","","","","","",
			"PSMCT16S"
		};
		static constexpr const char* TEXPSMNames[256] = {
			"PSMCT32",
			"PSMCT24",
			"PSMCT16",
			"","","","","","","",
			"PSMCT16S",
			"","","","","","","","",
			"PSMT8",
			"PSMT4",
			"","","","","","",
			"PSMT8H",
			"","","","","","","","",
			"PSMT4HL",
			"","","","","","","",
			"PSMT4HH",
			"","","",
			"PSMZ32",
			"PSMZ24",
			"PSMZ16",
			"","","","","","","",
			"PSMZ16S"
		};
		// clang-format on

		// the actual type is defined above thanks to preprocessing magic
		enum GSDUMP_GIFREG_NAME : GSDUMP_GIFREG_TYPE
		{
#define X(name, value) name = value,
			GSDUMP_GIFREG(X)
#undef X
		};
		constexpr auto GIFRegName(GSDUMP_GIFREG_NAME e) noexcept
		{
#define X(name, value)               \
	case (GSDUMP_GIFREG_NAME::name): \
		return #name;
			switch (e)
			{
				GSDUMP_GIFREG(X)
			}
#undef X
			return "UNKNOWN";
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
		void GenPacketList();
		void GenPacketInfo(GSData& dump);
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
			bool m_debug = false;
			size_t m_debug_index;
			std::unique_ptr<pxInputStream> m_dump_file;
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

	class IPCDialog : public wxDialogWithHelpers
	{
	public:
		IPCDialog(wxWindow* parent = NULL);
		virtual ~IPCDialog() = default;

		void OnConfirm(wxCommandEvent& evt);
		static wxString GetNameStatic() { return L"IPCSettings"; }
		wxString GetDialogName() const { return GetNameStatic(); }
	};
} // namespace Dialogs

wxWindowID pxIssueConfirmation(wxDialogWithHelpers& confirmDlg, const MsgButtons& buttons);
wxWindowID pxIssueConfirmation(wxDialogWithHelpers& confirmDlg, const MsgButtons& buttons, const wxString& disablerKey);

namespace GSDump
{
	extern bool isRunning;
}
