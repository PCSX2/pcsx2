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
#include "AppCommon.h"
#include "Dialogs/ModalPopups.h"

Dialogs::GSFrameConfigurationDialog::GSFrameConfigurationDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, AddAppName(_("Titlebar Customization")), pxDialogFlags())
{
	const int spacer_size = 10;
	wxBoxSizer *Y_axis = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *X_axis = new wxBoxSizer(wxHORIZONTAL);
	wxButton *CancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
	wxButton *OkButton = new wxButton(this, wxID_OK, _("OK"));

	m_show_limiter = new wxCheckBox(this, wxID_ANY, _("Show frame limiter status"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);
	m_show_videomode = new wxCheckBox(this, wxID_ANY, _("Show current video mode"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);
	m_show_saveslot = new wxCheckBox(this, wxID_ANY, _("Show current save slot"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);
	m_show_threadusage_percentage = new wxCheckBox(this, wxID_ANY, _("Show thread usage"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);

	Y_axis->Add(m_show_limiter, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_show_videomode, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_show_saveslot, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_show_threadusage_percentage, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	X_axis->Add(OkButton, wxSizerFlags().HorzBorder());
	X_axis->Add(CancelButton, wxSizerFlags().HorzBorder());

	*this += Y_axis;
	*this += X_axis | pxSizerFlags::StdButton();

	Bind(wxEVT_BUTTON, &Dialogs::GSFrameConfigurationDialog::ApplySettings, this, OkButton->GetId());

	InitValues();
	OkButton->SetFocus();
	SetSizerAndFit(GetSizer());
}

void Dialogs::GSFrameConfigurationDialog::ApplySettings(wxCommandEvent& evt)
{
	AppConfig::UiTemplateOptions& Options = g_Conf->Templates;
	Options.ShowLimiter = m_show_limiter->GetValue();
	Options.ShowSaveSlot = m_show_saveslot->GetValue();
	Options.ShowVideoMode = m_show_videomode->GetValue();
	Options.ShowThreadUsage = m_show_threadusage_percentage->GetValue();
	evt.Skip();
}

void Dialogs::GSFrameConfigurationDialog::InitValues()
{
	AppConfig::UiTemplateOptions& Options = g_Conf->Templates;
	m_show_limiter->SetValue(Options.ShowLimiter);
	m_show_saveslot->SetValue(Options.ShowSaveSlot);
	m_show_videomode->SetValue(Options.ShowVideoMode);
	m_show_threadusage_percentage->SetValue(Options.ShowThreadUsage);
}
