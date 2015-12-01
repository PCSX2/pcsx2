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
#include "ConfigurationPanels.h"

using namespace pxSizerFlags;

const wxChar* Panels::SpeedHacksPanel::GetEEcycleSliderMsg( int val )
{
	switch( val )
	{
		case -2:
		{
			m_msg_eecycle->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"-2 - Reduces the EE's cyclerate by about 50%.  Moderate speedup, but *will* cause stuttering audio on many FMVs.");
		}
		case -1:
		{
			m_msg_eecycle->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"-1 - Reduces the EE's cyclerate by about 33%.  Mild speedup for most games with high compatibility.");
		}
		case 0:
		{
			m_msg_eecycle->SetForegroundColour(wxColour(14,158,19)); // Dark Green
			return pxEt(L"0 - Default cyclerate. This closely matches the actual speed of a real PS2 EmotionEngine.");
		}
		case 1:
		{
			m_msg_eecycle->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"1 - Increases the EE's cyclerate by about 33%. Increases hardware requirements, may increase in-game FPS.");
		}
		case 2:
		{
			m_msg_eecycle->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"2 - Increases the EE's cyclerate by about 50%. Greatly increases hardware requirements, may noticeably increase in-game FPS.\nThis setting can cause games to FAIL TO BOOT.");
		}
		default:
			break;
	}

	return L"Unreachable Warning Suppressor!!";
}

const wxChar* Panels::SpeedHacksPanel::GetVUcycleSliderMsg( int val )
{
	switch( val )
	{
		case 0:
		{
			m_msg_vustealer->SetForegroundColour(wxColour(14,158,19)); // Dark Green
			return pxEt(L"0 - Disables VU Cycle Stealing.  Most compatible setting!");
		}
		case 1:
		{
			m_msg_vustealer->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"1 - Mild VU Cycle Stealing.  Lower compatibility, but some speedup for most games.");
		}
		case 2:
		{
			m_msg_vustealer->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"2 - Moderate VU Cycle Stealing.  Even lower compatibility, but significant speedups in some games.");
		}
		case 3:
		{
			// TODO: Mention specific games that benefit from this setting here.
			m_msg_vustealer->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"3 - Maximum VU Cycle Stealing.  Usefulness is limited, as this will cause flickering visuals or slowdown in most games.");
		}
		default:
			break;
	}

	return L"Unreachable Warning Suppressor!!";
}

void Panels::SpeedHacksPanel::SetEEcycleSliderMsg()
{
	m_msg_eecycle->SetLabel( GetEEcycleSliderMsg(m_slider_eecycle->GetValue()) );
}

void Panels::SpeedHacksPanel::SetVUcycleSliderMsg()
{
	m_msg_vustealer->SetLabel( GetVUcycleSliderMsg(m_slider_vustealer->GetValue()) );
}

Panels::SpeedHacksPanel::SpeedHacksPanel( wxWindow* parent )
 : BaseApplicableConfigPanel_SpecificConfig( parent )
{
	const wxSizerFlags sliderFlags( wxSizerFlags().Border( wxLEFT | wxRIGHT, 8 ).Expand() );

	m_check_Enable = new pxCheckBox( this, _("Enable speedhacks"),
		pxE( L"Speedhacks usually improve emulation speed, but can cause glitches, broken audio, and false FPS readings.  When having emulation problems, disable this panel first."
		)
	);
	m_check_Enable->SetToolTip(_("A safe and easy way to make sure that all speedhacks are completely disabled.")).SetSubPadding( 1 );

	wxPanelWithHelpers* left	= new wxPanelWithHelpers( this, wxVERTICAL );
	wxPanelWithHelpers* right	= new wxPanelWithHelpers( this, wxVERTICAL );

	left->SetMinWidth( 300 );
	right->SetMinWidth( 300 );

	m_button_Defaults = new wxButton( right, wxID_DEFAULT, _("Restore Defaults") );
	pxSetToolTip( m_button_Defaults, _("Resets all speedhack options to their defaults, which consequently turns them all OFF.") );

	// ------------------------------------------------------------------------
	// EE Cyclerate Hack Section:

	// Misc help text that I might find a home for later:
	// Cycle stealing works by 'fast-forwarding' the EE by an arbitrary number of cycles whenever VU1 micro-programs
	// are run, which works as a rough-guess skipping of what would normally be idle time spent running on the EE.

	m_eeSliderPanel = new wxPanelWithHelpers( left, wxVERTICAL, _("EE Cyclerate [Not Recommended]") );

	m_slider_eecycle = new wxSlider( m_eeSliderPanel, wxID_ANY, 0, -2, 2,
		wxDefaultPosition, wxDefaultSize, wxHORIZONTAL | wxSL_AUTOTICKS | wxSL_LABELS );

	m_msg_eecycle = new pxStaticHeading( m_eeSliderPanel );
	m_msg_eecycle->SetHeight(5);

	const wxChar* ee_tooltip = pxEt( L"Setting lower values on this slider effectively reduces the clock speed of the EmotionEngine's R5900 core cpu, and typically brings big speedups to games that fail to utilize the full potential of the real PS2 hardware. Conversely, higher values effectively increase the clock speed which may bring about an increase in in-game FPS while also making games more demanding and possibly causing glitches."
	);

	pxSetToolTip( m_slider_eecycle, ee_tooltip );
	pxSetToolTip( m_msg_eecycle, ee_tooltip );

	// ------------------------------------------------------------------------
	// VU Cycle Stealing Hack Section:

	m_vuSliderPanel = new wxPanelWithHelpers( right, wxVERTICAL, _("VU Cycle Stealing [Not Recommended]") );

	m_slider_vustealer = new wxSlider(m_vuSliderPanel, wxID_ANY, 0, 0, 3, wxDefaultPosition, wxDefaultSize,
		wxHORIZONTAL | wxSL_AUTOTICKS | wxSL_LABELS );

	m_msg_vustealer = new pxStaticHeading(m_vuSliderPanel);
	m_msg_vustealer->SetHeight(5);

	const wxChar* vu_tooltip = pxEt( L"This slider controls the amount of cycles the VU unit steals from the EmotionEngine.  Higher values increase the number of cycles stolen from the EE for each VU microprogram the game runs."
	);

	pxSetToolTip( m_slider_vustealer, vu_tooltip );
	pxSetToolTip( m_msg_vustealer, vu_tooltip );

	// ------------------------------------------------------------------------
	// microVU Hacks Section:

	wxPanelWithHelpers* vuHacksPanel = new wxPanelWithHelpers( right, wxVERTICAL, _("microVU Hacks") );

	m_check_vuFlagHack = new pxCheckBox( vuHacksPanel, _("mVU Flag Hack"),
		_("Good Speedup and High Compatibility; may cause bad graphics... [Recommended]" ) );

	m_check_vuThread = new pxCheckBox( vuHacksPanel, _("MTVU (Multi-Threaded microVU1)"),
		_("Good Speedup and High Compatibility; may cause hanging... [Recommended if 3+ cores]") );

	m_check_vuFlagHack->SetToolTip( pxEt( L"Updates Status Flags only on blocks which will read them, instead of all the time. This is safe most of the time, and Super VU does something similar by default."
	) );

	m_check_vuThread->SetToolTip( pxEt( L"Runs VU1 on its own thread (microVU1-only). Generally a speedup on CPUs with 3 or more cores. This is safe for most games, but a few games are incompatible and may hang. In the case of GS limited games, it may be a slowdown (especially on dual core CPUs)."
	) );

	// ------------------------------------------------------------------------
	// All other hacks Section:

	wxPanelWithHelpers* miscHacksPanel = new wxPanelWithHelpers( left, wxVERTICAL, _("Other Hacks") );

	m_check_intc = new pxCheckBox( miscHacksPanel, _("Enable INTC Spin Detection"),
		_("Huge speedup for some games, with almost no compatibility side effects. [Recommended]") );

	m_check_waitloop = new pxCheckBox( miscHacksPanel, _("Enable Wait Loop Detection"),
		_("Moderate speedup for some games, with no known side effects. [Recommended]" ) );

	m_check_fastCDVD = new pxCheckBox( miscHacksPanel, _("Enable fast CDVD"),
		_("Fast disc access, less loading times. [Not Recommended]") );


	m_check_intc->SetToolTip( pxEt( L"This hack works best for games that use the INTC Status register to wait for vsyncs, which includes primarily non-3D RPG titles. Games that do not use this method of vsync will see little or no speedup from this hack."
	) );

	m_check_waitloop->SetToolTip( pxEt( L"Primarily targetting the EE idle loop at address 0x81FC0 in the kernel, this hack attempts to detect loops whose bodies are guaranteed to result in the same machine state for every iteration until a scheduled event triggers emulation of another unit.  After a single iteration of such loops, we advance to the time of the next event or the end of the processor's timeslice, whichever comes first."
	) );

	m_check_fastCDVD->SetToolTip( pxEt( L"Check HDLoader compatibility lists for known games that have issues with this. (Often marked as needing 'mode 1' or 'slow DVD'"
	) );

	// ------------------------------------------------------------------------
	//  Layout and Size ---> (!!)

	//wxFlexGridSizer& DefEnableSizer( *new wxFlexGridSizer( 3, 0, 12 ) );
	//DefEnableSizer.AddGrowableCol( 1, 1 );
	//DefEnableSizer.AddGrowableCol( 2, 10 );
	//DefEnableSizer.AddGrowableCol( 1, 1 );
	//DefEnableSizer	+= m_button_Defaults	| StdSpace().Align( wxALIGN_LEFT );
	//DefEnableSizer	+= pxStretchSpacer(1);
	//DefEnableSizer	+= m_check_Enable		| StdExpand().Align( wxALIGN_RIGHT );

	*m_eeSliderPanel += m_slider_eecycle | sliderFlags;
	*m_eeSliderPanel += m_msg_eecycle | sliderFlags;

	*m_vuSliderPanel += m_slider_vustealer | sliderFlags;
	*m_vuSliderPanel += m_msg_vustealer | sliderFlags;

	*vuHacksPanel += m_check_vuFlagHack | StdExpand();
	*vuHacksPanel += m_check_vuThread | StdExpand();
	//*vuHacksPanel	+= 57; // Aligns left and right boxes in default language and font size

	*miscHacksPanel	+= m_check_intc | StdExpand();
	*miscHacksPanel	+= m_check_waitloop | StdExpand();
	*miscHacksPanel	+= m_check_fastCDVD | StdExpand();

	*left	+= m_eeSliderPanel | StdExpand();
	*left	+= miscHacksPanel	| StdExpand();

	*right	+= m_vuSliderPanel | StdExpand();
	*right	+= vuHacksPanel		| StdExpand();
	*right	+= StdPadding;
	*right	+= m_button_Defaults| StdButton();

	s_table = new wxFlexGridSizer( 2 );
	s_table->AddGrowableCol( 0, 1 );
	s_table->AddGrowableCol( 1, 1 );
	*s_table+= left				| pxExpand;
	*s_table+= right			| pxExpand;

	*this	+= m_check_Enable | StdExpand();
	*this	+= new wxStaticLine( this )	| pxExpand.Border(wxLEFT | wxRIGHT, 20);
	*this	+= StdPadding;
	*this	+= s_table					| pxExpand;

	// ------------------------------------------------------------------------

	Connect( m_slider_eecycle->GetId(),		wxEVT_SCROLL_CHANGED, wxScrollEventHandler( SpeedHacksPanel::EECycleRate_Scroll ) );
	Connect( m_slider_vustealer->GetId(),	wxEVT_SCROLL_CHANGED, wxScrollEventHandler( SpeedHacksPanel::VUCycleRate_Scroll ) );
	Connect( m_check_Enable->GetId(),		wxEVT_COMMAND_CHECKBOX_CLICKED,	wxCommandEventHandler( SpeedHacksPanel::OnEnable_Toggled ) );
	Connect( wxID_DEFAULT,					wxEVT_COMMAND_BUTTON_CLICKED,	wxCommandEventHandler( SpeedHacksPanel::Defaults_Click ) );
}

// Doesn't modify values - only locks(gray out)/unlocks as necessary.
void Panels::SpeedHacksPanel::EnableStuff( AppConfig* configToUse )
{
	if( !configToUse ) configToUse = g_Conf;

	bool hasPreset = configToUse->EnablePresets;
	bool hacksEnabled = configToUse->EnableSpeedHacks;
	bool HacksEnabledAndNoPreset = hacksEnabled && !hasPreset;

	// Main checkbox and Restore-defaults - locked only if presets are enabled
	m_check_Enable->Enable(!hasPreset);
	m_button_Defaults->Enable(!hasPreset);

	// lock/unlock the slider panels rather than the sliders themselves
	// in order to affect both sliders and texts
	m_eeSliderPanel->Enable(HacksEnabledAndNoPreset);
	m_vuSliderPanel->Enable(HacksEnabledAndNoPreset);

	// checkboxes
	m_check_vuFlagHack->Enable(HacksEnabledAndNoPreset);
	m_check_intc->Enable(HacksEnabledAndNoPreset);
	m_check_waitloop->Enable(HacksEnabledAndNoPreset);
	m_check_fastCDVD->Enable(HacksEnabledAndNoPreset);

	m_check_vuThread->Enable(hacksEnabled); // MTVU is unaffected by presets
}

void Panels::SpeedHacksPanel::AppStatusEvent_OnSettingsApplied()
{
	//Console.WriteLn("SpeedHacksPanel::AppStatusEvent_OnSettingsApplied()");
	ApplyConfigToGui( *g_Conf );
}

void Panels::SpeedHacksPanel::ApplyConfigToGui( AppConfig& configToApply, int flags )
{
	Pcsx2Config::SpeedhackOptions& opts=configToApply.EmuOptions.Speedhacks;

	// First, set the values of the widgets (checked/unchecked etc).
	m_check_Enable->SetValue(configToApply.EnableSpeedHacks);

	m_slider_eecycle	->SetValue( opts.EECycleRate );
	m_slider_vustealer	->SetValue( opts.VUCycleSteal );

	SetEEcycleSliderMsg();
	SetVUcycleSliderMsg();

	m_check_vuFlagHack->SetValue(opts.vuFlagHack);
	if( !(flags & AppConfig::APPLY_FLAG_FROM_PRESET) )
		m_check_vuThread	->SetValue(opts.vuThread);
	m_check_intc->SetValue(opts.IntcStat);
	m_check_waitloop->SetValue(opts.WaitLoop);
	m_check_fastCDVD->SetValue(opts.fastCDVD);

	// Then, lock(gray out)/unlock the widgets as necessary.
	EnableStuff( &configToApply );

	// Layout necessary to ensure changed slider text gets re-aligned properly
	Layout();

	//Console.WriteLn("SpeedHacksPanel::ApplyConfigToGui: EnabledPresets: %s", configToApply.EnablePresets?"true":"false");
}

// Apply the values from the widgets to the config,
// regardless if locked (grayed out) or not.
void Panels::SpeedHacksPanel::Apply()
{
	g_Conf->EnableSpeedHacks = m_check_Enable->GetValue();

	Pcsx2Config::SpeedhackOptions& opts( g_Conf->EmuOptions.Speedhacks );

	opts.EECycleRate		= m_slider_eecycle->GetValue();
	opts.VUCycleSteal		= m_slider_vustealer->GetValue();

	opts.WaitLoop			= m_check_waitloop->GetValue();
	opts.fastCDVD			= m_check_fastCDVD->GetValue();
	opts.IntcStat			= m_check_intc->GetValue();
	opts.vuFlagHack			= m_check_vuFlagHack->GetValue();
	opts.vuThread			= m_check_vuThread->GetValue();

	// If the user has a command line override specified, we need to disable it
	// so that their changes take effect
	wxGetApp().Overrides.DisableSpeedhacks = false;
}

void Panels::SpeedHacksPanel::OnEnable_Toggled( wxCommandEvent& evt )
{
	AppConfig tmp=*g_Conf;
	tmp.EnablePresets=false; //if clicked, button was enabled, so not using a preset --> let EnableStuff work
	tmp.EnableSpeedHacks = m_check_Enable->GetValue();

	EnableStuff( &tmp );
	evt.Skip();
}

void Panels::SpeedHacksPanel::Defaults_Click( wxCommandEvent& evt )
{
	//Can only get here presets are disabled at the GUI (= the 'Defaults' button is enabled).
	AppConfig currentConfigWithHacksReset = *g_Conf;
	currentConfigWithHacksReset.EmuOptions.Speedhacks = Pcsx2Config::SpeedhackOptions();
	currentConfigWithHacksReset.EnablePresets=false;//speed hacks gui depends on preset, apply it as if presets are disabled
	ApplyConfigToGui( currentConfigWithHacksReset );
	evt.Skip();
}

void Panels::SpeedHacksPanel::EECycleRate_Scroll(wxScrollEvent &event)
{
	SetEEcycleSliderMsg();
	event.Skip();
}

void Panels::SpeedHacksPanel::VUCycleRate_Scroll(wxScrollEvent &event)
{
	SetVUcycleSliderMsg();
	event.Skip();
}
