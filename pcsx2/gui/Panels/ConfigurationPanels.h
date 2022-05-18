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

// All of the options screens in PCSX2 are implemented as panels which can be bound to
// either their own dialog boxes, or made into children of a paged Properties box.  The
// paged Properties box is generally superior design, and there's a good chance we'll not
// want to deviate form that design anytime soon.  But there's no harm in keeping nice
// encapsulation of panels regardless, just in case we want to shuffle things around. :)

#pragma once

#include <wx/statline.h>
#include <wx/dnd.h>
#include <memory>

#include "gui/AppCommon.h"
#include "gui/ApplyState.h"
#include "gui/i18n.h"

namespace Panels
{
	// --------------------------------------------------------------------------------------
	//  DirPickerPanel
	// --------------------------------------------------------------------------------------
	// A simple panel which provides a specialized configurable directory picker with a
	// "[x] Use Default setting" option, which enables or disables the panel.
	//
	class DirPickerPanel : public BaseApplicableConfigPanel
	{
		typedef BaseApplicableConfigPanel _parent;

	protected:
		FoldersEnum_t m_FolderId;
		wxDirPickerCtrl* m_pickerCtrl;
		pxCheckBox* m_checkCtrl;
		wxTextCtrl* m_textCtrl;
		wxButton* b_explore;

	public:
		DirPickerPanel(wxWindow* parent, FoldersEnum_t folderid, const wxString& label, const wxString& dialogLabel);
		DirPickerPanel(wxWindow* parent, FoldersEnum_t folderid, const wxString& dialogLabel);
		virtual ~DirPickerPanel() = default;

		void Reset();
		wxDirName GetPath() const;
		void SetPath(const wxString& src);

		DirPickerPanel& SetStaticDesc(const wxString& msg);
		DirPickerPanel& SetToolTip(const wxString& tip);

		wxWindowID GetId() const;
		wxWindowID GetPanelId() const { return m_windowId; }

		// Overrides!

		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		bool Enable(bool enable = true);

	protected:
		void Init(FoldersEnum_t folderid, const wxString& dialogLabel, bool isCompact);
		void InitForPortableMode(const wxString& normalized);
		void InitForRegisteredMode(const wxString& normalized, const wxString& dialogLabel, bool isCompact);

		void UseDefaultPath_Click(wxCommandEvent& event);
		void Explore_Click(wxCommandEvent& event);
		void UpdateCheckStatus(bool someNoteworthyBoolean);
	};

	// --------------------------------------------------------------------------------------
	//  DocsFolderPickerPanel / LanguageSelectionPanel
	// --------------------------------------------------------------------------------------
	class DocsFolderPickerPanel : public BaseApplicableConfigPanel
	{
	protected:
		pxRadioPanel* m_radio_UserMode;
		DirPickerPanel* m_dirpicker_custom;

	public:
		virtual ~DocsFolderPickerPanel() = default;
		DocsFolderPickerPanel(wxWindow* parent, bool isFirstTime = true);

		void Apply();
		void AppStatusEvent_OnSettingsApplied();

		DocsModeType GetDocsMode() const;
		wxWindowID GetDirPickerId() const { return m_dirpicker_custom ? m_dirpicker_custom->GetId() : 0; }

	protected:
		void OnRadioChanged(wxCommandEvent& evt);
	};

	class LanguageSelectionPanel : public BaseApplicableConfigPanel
	{
	protected:
		LangPackList m_langs;
		wxComboBox* m_picker;

	public:
		virtual ~LanguageSelectionPanel() = default;
		LanguageSelectionPanel(wxWindow* parent, bool showApply = true);

		void Apply();
		void AppStatusEvent_OnSettingsApplied();

	protected:
		void OnApplyLanguage_Clicked(wxCommandEvent& evt);
	};

	// --------------------------------------------------------------------------------------
	//  CpuPanelEE / CpuPanelVU : Sub Panels
	// --------------------------------------------------------------------------------------

	// --------------------------------------------------------------------------------------
	//  BaseAdvancedCpuOptions
	// --------------------------------------------------------------------------------------
	class BaseAdvancedCpuOptions : public BaseApplicableConfigPanel_SpecificConfig
	{

	protected:
		pxRadioPanel* m_RoundModePanel;
		pxRadioPanel* m_ClampModePanel;

	public:
		BaseAdvancedCpuOptions(wxWindow* parent);
		virtual ~BaseAdvancedCpuOptions() = default;

		void RestoreDefaults();

	protected:
		void OnRestoreDefaults(wxCommandEvent& evt);
		void ApplyRoundmode(SSE_MXCSR& mxcsr);
	};

	// --------------------------------------------------------------------------------------
	//  AdvancedOptionsFPU / AdvancedOptionsVU
	// --------------------------------------------------------------------------------------
	class AdvancedOptionsFPU : public BaseAdvancedCpuOptions
	{
	public:
		AdvancedOptionsFPU(wxWindow* parent);
		virtual ~AdvancedOptionsFPU() = default;
		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);
	};

	class AdvancedOptionsVU : public BaseAdvancedCpuOptions
	{
	public:
		AdvancedOptionsVU(wxWindow* parent);
		virtual ~AdvancedOptionsVU() = default;
		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);
	};

	// --------------------------------------------------------------------------------------
	//  CpuPanelEE / CpuPanelVU : Actual Panels
	// --------------------------------------------------------------------------------------
	class CpuPanelEE : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		pxRadioPanel* m_panel_RecEE;
		pxRadioPanel* m_panel_RecIOP;
		pxCheckBox* m_check_EECacheEnable;
		AdvancedOptionsFPU* m_advancedOptsFpu;
		wxButton* m_button_RestoreDefaults;

	public:
		CpuPanelEE(wxWindow* parent);
		virtual ~CpuPanelEE() = default;

		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);

	protected:
		void OnRestoreDefaults(wxCommandEvent& evt);
		void EECache_Event(wxCommandEvent& evt);
	};

	class CpuPanelVU : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		pxRadioPanel* m_panel_VU0;
		pxRadioPanel* m_panel_VU1;
		Panels::AdvancedOptionsVU* m_advancedOptsVu;
		wxButton* m_button_RestoreDefaults;

	public:
		CpuPanelVU(wxWindow* parent);
		virtual ~CpuPanelVU() = default;

		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);

	protected:
		void OnRestoreDefaults(wxCommandEvent& evt);
	};

	// --------------------------------------------------------------------------------------
	//  FrameSkipPanel
	// --------------------------------------------------------------------------------------
	class FrameSkipPanel : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		wxSpinCtrl* m_spin_FramesToSkip;
		wxSpinCtrl* m_spin_FramesToDraw;

		pxRadioPanel* m_radio_SkipMode;

	public:
		FrameSkipPanel(wxWindow* parent);
		virtual ~FrameSkipPanel() = default;

		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);
	};

	// --------------------------------------------------------------------------------------
	//  FramelimiterPanel
	// --------------------------------------------------------------------------------------
	class FramelimiterPanel : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		pxCheckBox* m_check_LimiterDisable;
		wxSpinCtrl* m_spin_NominalPct;
		wxSpinCtrl* m_spin_SlomoPct;
		wxSpinCtrl* m_spin_TurboPct;

		wxTextCtrl* m_text_BaseNtsc;
		wxTextCtrl* m_text_BasePal;

		wxCheckBox* m_SkipperEnable;
		wxCheckBox* m_TurboSkipEnable;
		wxSpinCtrl* m_spin_SkipThreshold;

	public:
		FramelimiterPanel(wxWindow* parent);
		virtual ~FramelimiterPanel() = default;

		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);
	};

	// --------------------------------------------------------------------------------------
	//  GSWindowSettingsPanel
	// --------------------------------------------------------------------------------------
	class GSWindowSettingsPanel : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		// Exclusive mode is currently not used (true for svn r4399).
		// PCSX2 has partial infrastructure for it:
		//  - GS seem to support it (it supports the API and has implementation), but I don't know if it ever got called.
		//  - BUT, the configuration (AppConfig, and specifically GSWindowOptions) do NOT seem to have a place to store this value,
		//    and PCSX2's code doesn't seem to use this API anywhere. So, no exclusive mode for now.
		//    - avih

		wxComboBox* m_combo_AspectRatio;
		wxComboBox* m_combo_FMVAspectRatioSwitch;
		wxComboBox* m_combo_vsync;

		wxTextCtrl* m_text_Zoom;

		pxCheckBox* m_check_CloseGS;
		pxCheckBox* m_check_SizeLock;
		pxCheckBox* m_check_VsyncEnable;
		pxCheckBox* m_check_Fullscreen;

		pxCheckBox* m_check_HideMouse;
		pxCheckBox* m_check_DclickFullscreen;

		wxTextCtrl* m_text_WindowWidth;
		wxTextCtrl* m_text_WindowHeight;

	public:
		GSWindowSettingsPanel(wxWindow* parent);
		virtual ~GSWindowSettingsPanel() = default;
		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);
	};

	class VideoPanel : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		pxCheckBox* m_check_SynchronousGS;
		wxSpinCtrl* m_spinner_VsyncQueue;
		wxButton* m_restore_defaults;
		FrameSkipPanel* m_span;
		FramelimiterPanel* m_fpan;

	public:
		VideoPanel(wxWindow* parent);
		virtual ~VideoPanel() = default;
		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void Defaults_Click(wxCommandEvent& evt);
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);

	protected:
		void OnOpenWindowSettings(wxCommandEvent& evt);
	};

	// --------------------------------------------------------------------------------------
	//  SpeedHacksPanel
	// --------------------------------------------------------------------------------------
	class SpeedHacksPanel : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		wxBoxSizer* m_sizer;
		wxFlexGridSizer* s_table;

		pxCheckBox* m_check_Enable;
		wxButton* m_button_Defaults;

		wxPanelWithHelpers* m_eeRateSliderPanel;
		wxPanelWithHelpers* m_eeSkipSliderPanel;
		wxSlider* m_slider_eeRate;
		wxSlider* m_slider_eeSkip;
		pxStaticText* m_msg_eeRate;
		pxStaticText* m_msg_eeSkip;

		pxCheckBox* m_check_intc;
		pxCheckBox* m_check_waitloop;
		pxCheckBox* m_check_fastCDVD;
		pxCheckBox* m_check_vuFlagHack;
		pxCheckBox* m_check_vuThread;
		pxCheckBox* m_check_vu1Instant;

	public:
		virtual ~SpeedHacksPanel() = default;
		SpeedHacksPanel(wxWindow* parent);
		void Apply();
		void EnableStuff(AppConfig* configToUse = NULL);
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);

	protected:
		const wxChar* GetEECycleRateSliderMsg(int val);
		const wxChar* GetEECycleSkipSliderMsg(int val);
		void SetEEcycleSliderMsg();
		void SetVUcycleSliderMsg();

		void OnEnable_Toggled(wxCommandEvent& evt);
		void Defaults_Click(wxCommandEvent& evt);
		void EECycleRate_Scroll(wxScrollEvent& event);
		void VUCycleRate_Scroll(wxScrollEvent& event);
		void VUThread_Enable(wxCommandEvent& evt);
	};

	// --------------------------------------------------------------------------------------
	//  GameFixesPanel
	// --------------------------------------------------------------------------------------
	class GameFixesPanel : public BaseApplicableConfigPanel_SpecificConfig
	{
	protected:
		pxCheckBox* m_checkbox[GamefixId_COUNT];
		pxCheckBox* m_check_Enable;

	public:
		GameFixesPanel(wxWindow* parent);
		virtual ~GameFixesPanel() = default;
		void EnableStuff(AppConfig* configToUse = NULL);
		void OnEnable_Toggled(wxCommandEvent& evt);
		void Apply();
		void AppStatusEvent_OnSettingsApplied();
		void ApplyConfigToGui(AppConfig& configToApply, int flags = 0);
	};

	class SettingsDirPickerPanel : public DirPickerPanel
	{
	public:
		SettingsDirPickerPanel(wxWindow* parent);
	};

	// --------------------------------------------------------------------------------------
	//  StandardPathsPanel
	// --------------------------------------------------------------------------------------
	class StandardPathsPanel : public BaseApplicableConfigPanel
	{
	public:
		StandardPathsPanel(wxWindow* parent);
		void Apply();
		void AppStatusEvent_OnSettingsApplied();
	};

	// --------------------------------------------------------------------------------------
	//  BaseSelectorPanel
	// --------------------------------------------------------------------------------------
	class BaseSelectorPanel : public BaseApplicableConfigPanel
	{
		typedef BaseApplicableConfigPanel _parent;

	public:
		virtual ~BaseSelectorPanel() = default;
		BaseSelectorPanel(wxWindow* parent);

		virtual void RefreshSelections();

		virtual bool Show(bool visible = true);
		virtual void OnShown();
		virtual void OnFolderChanged(wxFileDirPickerEvent& evt);

	protected:
		void OnRefreshSelections(wxCommandEvent& evt);

		// This method is called when the enumeration contents have changed.  The implementing
		// class should populate or re-populate listbox/selection components when invoked.
		//
		virtual void DoRefresh() = 0;

		// This method is called when an event has indicated that the enumeration status of the
		// selector may have changed.  The implementing class should re-enumerate the folder/source
		// data and return either TRUE (enumeration status unchanged) or FALSE (enumeration status
		// changed).
		//
		// If the implementation returns FALSE, then the BaseSelectorPanel will invoke a call to
		// DoRefresh() [which also must be implemented]
		//
		virtual bool ValidateEnumerationStatus() = 0;

		void OnShow(wxShowEvent& evt);
	};

	// --------------------------------------------------------------------------------------
	//  BiosSelectorPanel
	// --------------------------------------------------------------------------------------
	class BiosSelectorPanel : public BaseSelectorPanel
	{
	protected:
		std::unique_ptr<wxArrayString> m_BiosList;
		wxListBox* m_ComboBox;
		DirPickerPanel* m_FolderPicker;

	public:
		BiosSelectorPanel(wxWindow* parent);
		virtual ~BiosSelectorPanel() = default;

		class EnumThread : public Threading::pxThread
		{
		public:
			std::vector<std::pair<wxString, u32>> Result;

			virtual ~EnumThread()
			{
				try
				{
					pxThread::Cancel();
				}
				DESTRUCTOR_CATCHALL
			}
			EnumThread(BiosSelectorPanel& parent);

		protected:
			void ExecuteTaskInThread();
			BiosSelectorPanel& m_parent;
		};

	protected:
		virtual void Apply();
		virtual void AppStatusEvent_OnSettingsApplied();
		virtual void DoRefresh();
		virtual void OnEnumComplete(wxCommandEvent& evt);
		virtual bool ValidateEnumerationStatus();

		std::unique_ptr<EnumThread> m_EnumeratorThread;
	};
} // namespace Panels
