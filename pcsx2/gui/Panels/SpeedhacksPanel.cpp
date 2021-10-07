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
#include "gui/App.h"
#include "ConfigurationPanels.h"

using namespace pxSizerFlags;

const wxChar* Panels::SpeedHacksPanel::GetEECycleRateSliderMsg( int val )
{
	switch( val )
	{
		case -3:
		{
			m_msg_eeRate->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"50% cyclerate. Significant reduction of CPU requirements. Speedup for very lightweight games, slows down others. FMVs and audio may stutter or skip.");
		}
		case -2:
		{
			const wxColour LightRed = wxColour(255, 80, 0);
			m_msg_eeRate->SetForegroundColour(LightRed);
			return pxEt(L"60% cyclerate. Moderate reduction of CPU requirements. Speedup for lightweight games, slows down others. FMVs and audio may stutter or skip.");
		}
		case -1:
		{
			const wxColour Orange = wxColour(255, 120, 0);
			m_msg_eeRate->SetForegroundColour(Orange);
			return pxEt(L"75% cyclerate. Slight reduction of CPU requirements. Speedup for less demanding games, slows down others.");
		}
		case 0:
		{
			const wxColour DarkSeaGreen = wxColour(14, 158, 19);
			m_msg_eeRate->SetForegroundColour(DarkSeaGreen);
			return pxEt(L"Default cyclerate. Runs the emulated PS2 Emotion Engine at normal speed.");
		}
		case 1:
		{
			const wxColour Orange = wxColour(255, 120, 0);
			m_msg_eeRate->SetForegroundColour(Orange);
			return pxEt(L"130% cyclerate. Moderate increase of CPU requirements. Variable framerate games may have higher internal framerates.");
		}
		case 2:
		{
			const wxColour LightRed = wxColour(255, 80, 0);
			m_msg_eeRate->SetForegroundColour(LightRed);
			return pxEt(L"180% cyclerate. Significant increase of CPU requirements. Variable framerate games will have higher internal framerates. FMVs may be slow. May cause stability problems.");
		}
		case 3:
		{
			m_msg_eeRate->SetForegroundColour(wxColour(L"Red"));
			return pxEt(L"300% cyclerate. Extreme increase of CPU requirements. Variable framerate games will have higher internal framerates. FMVs may be slow. May cause stability problems.");
		}
		default:
			break;
	}

	return L"Unreachable Warning Suppressor!!";
}

const wxChar* Panels::SpeedHacksPanel::GetEECycleSkipSliderMsg( int val )
{
	switch( val )
	{
		case 0:
		{
			const wxColour DarkSeaGreen = wxColour(14, 158, 19);
			m_msg_eeSkip->SetForegroundColour(DarkSeaGreen);
			return pxEt(L"Disables EE Cycle Skipping. Most compatible setting.");
		}
		case 1:
		{
			const wxColour Orange = wxColour(255, 120, 0);
			m_msg_eeSkip->SetForegroundColour(Orange);
			return pxEt(L"Mild EE Cycle Skipping. Mild slow down for most games, but may help some games with mild VU starvation problems run at full speed.");
		}
		case 2:
		{
			const wxColour LightRed = wxColour(255, 80, 0);
			m_msg_eeSkip->SetForegroundColour(LightRed);
			return pxEt(L"Moderate EE Cycle Skipping. Slow down for most games, but may help some games with moderate VU starvation problems run at full speed.");
		}
		default:
			break;
	}

	return L"Unreachable Warning Suppressor!!";
}

void Panels::SpeedHacksPanel::SetEEcycleSliderMsg()
{
	m_msg_eeRate->SetLabel( GetEECycleRateSliderMsg(m_slider_eeRate->GetValue()) );
}

void Panels::SpeedHacksPanel::SetVUcycleSliderMsg()
{
	m_msg_eeSkip->SetLabel( GetEECycleSkipSliderMsg(m_slider_eeSkip->GetValue()) );
}

Panels::SpeedHacksPanel::SpeedHacksPanel( wxWindow* parent )
 : BaseApplicableConfigPanel_SpecificConfig( parent )
{
	const wxSizerFlags sliderFlags( wxSizerFlags().Border( wxLEFT | wxRIGHT, 8 ).Expand() );

	m_check_Enable = new pxCheckBox(
		this, _("Enable speedhacks"),
		pxE( L"Speedhacks usually improve emulation speed"", but can cause glitches, broken audio, and false FPS readings.  When having emulation problems, disable this panel first."
		)
	);
	m_check_Enable->SetToolTip(_("A safe and easy way to make sure that all speedhacks are completely disabled.")).SetSubPadding( 1 );


	m_button_Defaults = new wxButton( this, wxID_DEFAULT, _("Restore Defaults") );

	// ------------------------------------------------------------------------
	// EE Cyclerate Hack Section:

	// Misc help text that I might find a home for later:
	// Cycle stealing works by 'fast-forwarding' the EE by an arbitrary number of cycles whenever VU1 micro-programs
	// are run, which works as a rough-guess skipping of what would normally be idle time spent running on the EE.

	m_eeRateSliderPanel = new wxPanelWithHelpers( this, wxVERTICAL, _("EE Cyclerate [Not Recommended]") );

	m_slider_eeRate = new wxSlider( m_eeRateSliderPanel, wxID_ANY, 0, -3, 3,
		wxDefaultPosition, wxDefaultSize, wxHORIZONTAL | wxSL_AUTOTICKS | wxSL_LABELS );

	m_msg_eeRate = new pxStaticHeading( m_eeRateSliderPanel );	
	const wxChar* eeRate_tooltip = pxEt( L"Modifies the emulated Emotion Engine CPU clock. Higher values may increase the internal framerate in games with variable framerates, but will increase CPU requirements substantially. Lower values will reduce the CPU load allowing lightweight games to run full speed on weaker CPUs." );
	pxSetToolTip( m_slider_eeRate, eeRate_tooltip );
	pxSetToolTip( m_msg_eeRate, eeRate_tooltip );

	// ------------------------------------------------------------------------
	// EE Cycle Skipping Hack Section:

	m_eeSkipSliderPanel = new wxPanelWithHelpers( this, wxVERTICAL, _("EE Cycle Skipping [Not Recommended]") );

	m_slider_eeSkip = new wxSlider(m_eeSkipSliderPanel, wxID_ANY, 0, 0, 2, wxDefaultPosition, wxDefaultSize,
		wxHORIZONTAL | wxSL_AUTOTICKS | wxSL_LABELS );

	m_msg_eeSkip = new pxStaticHeading(m_eeSkipSliderPanel);

	const wxChar* eeSkip_tooltip = pxEt( L"Makes the emulated Emotion Engine skip cycles, allowing VU microprograms to execute at faster intervals. Helps a small subset of games with VU starvation problems, E.g. Shadow of the Colossus. More often than not this is harmful to performance and causes FPS readouts to be inaccurate." );

	pxSetToolTip( m_slider_eeSkip, eeSkip_tooltip );
	pxSetToolTip( m_msg_eeSkip, eeSkip_tooltip );

	// ------------------------------------------------------------------------
	// microVU Hacks Section:

	wxPanelWithHelpers* vuHacksPanel = new wxPanelWithHelpers( this, wxVERTICAL, _("microVU Hacks") );

	m_check_vuFlagHack = new pxCheckBox( vuHacksPanel, _("mVU Flag Hack"),
		_("Good Speedup and High Compatibility; may cause bad graphics... [Recommended]" ) );

	m_check_vuThread = new pxCheckBox( vuHacksPanel, _("MTVU (Multi-Threaded microVU1)"),
		_("Good Speedup and High Compatibility; may cause hanging... [Recommended on 3+ cores]") );

	m_check_vu1Instant = new pxCheckBox(vuHacksPanel, _("Instant VU1 (without MTVU only)"),
		_("Good Speedup and High Compatibility; may cause some graphical errors"));

	m_check_vuFlagHack->SetToolTip( pxEt( L"Updates Status Flags only on blocks which will read them, instead of all the time. This is safe most of the time."
	) );

	m_check_vuThread->SetToolTip( pxEt( L"Runs VU1 on its own thread (microVU1-only). Generally a speedup on CPUs with 3 or more cores. This is safe for most games, but a few games are incompatible and may hang. In the case of GS limited games, it may be a slowdown (especially on dual core CPUs)."
	) );

	m_check_vu1Instant->SetToolTip(pxEt(L"Runs VU1 instantly (when MTVU is disabled). Provides a modest speed improvement. This is safe for most games, but a few games may exhibit graphical errors."
	));

	// ------------------------------------------------------------------------
	// All other hacks Section:

	wxPanelWithHelpers* miscHacksPanel = new wxPanelWithHelpers( this, wxVERTICAL, _("Other Hacks") );

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

	m_check_fastCDVD->SetToolTip( pxEt( L"Check HDLoader compatibility lists for known games that have issues with this (often marked as needing 'mode 1' or 'slow DVD')."
	) );


	*m_eeRateSliderPanel += m_slider_eeRate | sliderFlags;
	*m_eeRateSliderPanel += m_msg_eeRate | sliderFlags;

	*m_eeSkipSliderPanel += m_slider_eeSkip | sliderFlags;
	*m_eeSkipSliderPanel += m_msg_eeSkip | sliderFlags;

	*vuHacksPanel += m_check_vuFlagHack | StdExpand();
	*vuHacksPanel += m_check_vuThread | StdExpand();
	*vuHacksPanel += m_check_vu1Instant | StdExpand();

	*miscHacksPanel += m_check_intc | StdExpand();
	*miscHacksPanel += m_check_waitloop | StdExpand();
	*miscHacksPanel += m_check_fastCDVD | StdExpand();

	s_table = new wxFlexGridSizer( 3, 2, 0, 0 );
	s_table->Add(m_eeRateSliderPanel, StdExpand());
	s_table->Add(m_eeSkipSliderPanel, StdExpand());
	s_table->Add(miscHacksPanel, StdExpand());
	s_table->Add(vuHacksPanel, StdExpand());
	s_table->Add(new wxStaticText(this, -1, ""));
	s_table->Add(m_button_Defaults, StdButton());

	m_sizer = new wxBoxSizer(wxVERTICAL);
	m_sizer->Add(m_check_Enable, StdExpand());
	m_sizer->Add(s_table);

	SetSizer(m_sizer);
	// ------------------------------------------------------------------------

	Bind(wxEVT_SCROLL_CHANGED, &SpeedHacksPanel::EECycleRate_Scroll, this, m_slider_eeRate->GetId());
	Bind(wxEVT_SCROLL_CHANGED, &SpeedHacksPanel::VUCycleRate_Scroll, this, m_slider_eeSkip->GetId());
	Bind(wxEVT_CHECKBOX, &SpeedHacksPanel::OnEnable_Toggled, this, m_check_Enable->GetId());
	Bind(wxEVT_BUTTON, &SpeedHacksPanel::Defaults_Click, this, wxID_DEFAULT);
	Bind(wxEVT_CHECKBOX, &SpeedHacksPanel::VUThread_Enable, this, m_check_vuThread->GetId());
}

// Doesn't modify values - only locks(gray out)/unlocks as necessary.
void Panels::SpeedHacksPanel::EnableStuff( AppConfig* configToUse )
{
	if (!configToUse) configToUse = g_Conf.get();

	bool hasPreset = configToUse->EnablePresets;
	bool hacksEnabled = configToUse->EnableSpeedHacks;
	bool HacksEnabledAndNoPreset = hacksEnabled && !hasPreset;

	// Main checkbox and Restore-defaults - locked only if presets are enabled
	m_check_Enable->Enable(!hasPreset);
	m_button_Defaults->Enable(!hasPreset);

	// lock/unlock the slider panels rather than the sliders themselves
	// in order to affect both sliders and texts
	m_eeRateSliderPanel->Enable(HacksEnabledAndNoPreset);
	m_eeSkipSliderPanel->Enable(HacksEnabledAndNoPreset);

	// checkboxes
	m_check_vuFlagHack->Enable(HacksEnabledAndNoPreset);
	m_check_intc->Enable(HacksEnabledAndNoPreset);
	m_check_waitloop->Enable(HacksEnabledAndNoPreset);
	m_check_fastCDVD->Enable(HacksEnabledAndNoPreset);

	// Grayout MTVU on safest preset
	m_check_vuThread->Enable(hacksEnabled && (!hasPreset || configToUse->PresetIndex != 0));

	// Disables the Instant VU1 checkbox when MTVU is checked in the GUI as reflected in the code.
	// Makes Instant VU1 toggleable when MTVU is unchecked in the GUI.
	// Some may think that having MTVU + Instant VU1 checked, can have bad side-effects when it doesn't.
	m_check_vu1Instant->Enable(hacksEnabled && !m_check_vuThread->GetValue());

	// Layout necessary to ensure changed slider text gets re-aligned properly
	// and to properly gray/ungray pxStaticText stuff (I suspect it causes a
	// paint event to be sent on Windows)
	Layout();
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

	m_slider_eeRate	->SetValue( opts.EECycleRate );

	opts.EECycleSkip = std::clamp<u8>(opts.EECycleSkip, 0, 2);
	m_slider_eeSkip	->SetValue( opts.EECycleSkip );

	SetEEcycleSliderMsg();
	SetVUcycleSliderMsg();

	m_check_vuFlagHack->SetValue(opts.vuFlagHack);
	m_check_intc->SetValue(opts.IntcStat);
	m_check_waitloop->SetValue(opts.WaitLoop);
	m_check_fastCDVD->SetValue(opts.fastCDVD);
	m_check_vuThread->SetValue(opts.vuThread);
	m_check_vu1Instant->SetValue(opts.vu1Instant);

	// Then, lock(gray out)/unlock the widgets as necessary.
	EnableStuff( &configToApply );
}

// Apply the values from the widgets to the config,
// regardless if locked (grayed out) or not.
void Panels::SpeedHacksPanel::Apply()
{
	g_Conf->EnableSpeedHacks = m_check_Enable->GetValue();

	Pcsx2Config::SpeedhackOptions& opts( g_Conf->EmuOptions.Speedhacks );

	opts.EECycleRate		= m_slider_eeRate->GetValue();
	opts.EECycleSkip		= m_slider_eeSkip->GetValue();

	opts.WaitLoop			= m_check_waitloop->GetValue();
	opts.fastCDVD			= m_check_fastCDVD->GetValue();
	opts.IntcStat			= m_check_intc->GetValue();
	opts.vuFlagHack			= m_check_vuFlagHack->GetValue();
	opts.vuThread			= m_check_vuThread->GetValue();
	opts.vu1Instant			= m_check_vu1Instant->GetValue();

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

	Layout();

	event.Skip();
}

void Panels::SpeedHacksPanel::VUCycleRate_Scroll(wxScrollEvent &event)
{
	SetVUcycleSliderMsg();

	Layout();

	event.Skip();
}

void Panels::SpeedHacksPanel::VUThread_Enable(wxCommandEvent& evt)
{
	m_check_vu1Instant->Enable(!m_check_vuThread->GetValue());
	Layout();
}
