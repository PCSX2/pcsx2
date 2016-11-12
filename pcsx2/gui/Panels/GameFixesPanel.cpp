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

Panels::GameFixesPanel::GameFixesPanel( wxWindow* parent )
	: BaseApplicableConfigPanel_SpecificConfig( parent )
{
	// NOTE: Order of checkboxes must match the order of the bits in the GamefixOptions structure!
	// NOTE2: Don't make this static, because translations can change at run-time :)

	const wxString check_text[GamefixId_COUNT] =
	{
		_("VU Add Hack - Fixes Tri-Ace games boot crash."),
		_("VU Clip Flag Hack - For Persona games (SuperVU recompiler only!)"),
		_("FPU Compare Hack - For Digimon Rumble Arena 2."),
		_("FPU Multiply Hack - For Tales of Destiny."),
		_("FPU Negative Div Hack - For Gundam games."),
		_("VU XGkick Hack - For Erementar Gerad."),
		_("FFX videos fix - Fixes bad graphics overlay in FFX videos."),
		_("EE timing hack - Multi purpose hack. Try if all else fails."),
		_("Skip MPEG hack - Skips videos/FMVs in games to avoid game hanging/freezes."),
		_("OPH Flag hack - Try if your game freezes showing the same frame."),
		_("Ignore DMAC writes when it is busy."),
		_("Simulate VIF1 FIFO read ahead. Fixes slow loading games."),
		_("Delay VIF1 Stalls (VIF1 FIFO) - For SOCOM 2 HUD and Spy Hunter loading hang."),
		_("Enable the GIF FIFO (slower but needed for Hotwheels, Wallace and Gromit, DJ Hero)"),
		_("Switch to GSdx software rendering when an FMV plays"),
		_("Preload TLB hack to avoid tlb miss on Goemon"),
		_("VU I bit Hack avoid constant recompilation (Scarface The World Is Yours)"),
	};

	m_listbox = new wxCheckListBox( this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
			GamefixId_COUNT, check_text, wxLB_NEEDED_SB );

	m_listbox->SetToolTip( pxEt(
				L"VU Add Hack - Games that need this hack to boot:\n * Star Ocean 3\n * Radiata Stories\n * Valkyrie Profile 2\n\n"
				L"EE timing hack - Known to affect following games:\n * Digital Devil Saga (Fixes FMV and crashes)\n * SSX (Fixes bad graphics and crashes)\n * Resident Evil: Dead Aim (Causes garbled textures)\n\n"
				L"OPH Flag hack - Known to affect following games:\n * Bleach Blade Battler\n * Growlanser II and III\n * Wizardry\n\n"
				L"DMAC write - Known to affect following games:\n * Mana Khemia 1 (Going \"off campus\")\n\n"
				L"VIF1 FIFO - Known to affect following games:\n * Test Drive Unlimited\n * Transformers\n\n"
				));

	m_check_Enable = new pxCheckBox( this, _("Enable manual game fixes [Not recommended]"),
		pxE( L"It's better to enable 'Automatic game fixes' at the main menu instead, and leave this page empty. ('Automatic' means: selectively use specific tested fixes for specific games). Manual game fixes will NOT increase your performance. In fact they may decrease it."
		)
	);

	// By default, wx will create a small widget with a scrollbar. If display
	// is big enough, set a min size to avoid the scrollbar
	if( wxSystemSettings::GetMetric(wxSYS_SCREEN_Y) > 850 )
		m_listbox->SetMinSize(wxSize(-1, 400));

	m_check_Enable->SetToolTip(pxE(L"Gamefixes can work around wrong emulation in some titles. \nThey may also cause compatibility or performance issues.\n\nThe safest way is to make sure that all game fixes are completely disabled.")).SetSubPadding( 1 );
	m_check_Enable->SetValue( g_Conf->EnableGameFixes );

	*this	+= m_check_Enable	| StdExpand();
	*this	+= m_listbox 		| pxCenter;

	Bind(wxEVT_CHECKBOX, &GameFixesPanel::OnEnable_Toggled, this, m_check_Enable->GetId());

	EnableStuff();
}

void Panels::GameFixesPanel::Apply()
{
	g_Conf->EnableGameFixes = m_check_Enable->GetValue();

	Pcsx2Config::GamefixOptions& opts( g_Conf->EmuOptions.Gamefixes );
	for (GamefixId i=GamefixId_FIRST; i < pxEnumEnd; ++i)
		opts.Set((GamefixId)i, m_listbox->IsChecked(i));

	// make sure the user's command line specifications are disabled (if present).
	wxGetApp().Overrides.ApplyCustomGamefixes = false;
}

void Panels::GameFixesPanel::EnableStuff( AppConfig* configToUse )
{
	if (!configToUse) configToUse = g_Conf.get();

	m_listbox->Enable(m_check_Enable->GetValue() && !configToUse->EnablePresets);

	Layout();
}

void Panels::GameFixesPanel::OnEnable_Toggled( wxCommandEvent& evt )
{
	AppConfig tmp=*g_Conf;
	tmp.EnablePresets=false; //if clicked, button was enabled, so not using a preset --> let EnableStuff work

	EnableStuff( &tmp );
	evt.Skip();
}

void Panels::GameFixesPanel::AppStatusEvent_OnSettingsApplied()
{
	ApplyConfigToGui( *g_Conf );
}

void Panels::GameFixesPanel::ApplyConfigToGui( AppConfig& configToApply, int flags )
{
	const Pcsx2Config::GamefixOptions& opts( configToApply.EmuOptions.Gamefixes );
	for (GamefixId i=GamefixId_FIRST; i < pxEnumEnd; ++i)
		m_listbox->Check( (int)i, opts.Get((GamefixId)i) );//apply the use/don't-use fix values

	m_check_Enable->SetValue( configToApply.EnableGameFixes );//main gamefixes checkbox
	EnableStuff( &configToApply );// enable/disable the all the fixes controls according to the main one

	this->Enable(!configToApply.EnablePresets);
}
