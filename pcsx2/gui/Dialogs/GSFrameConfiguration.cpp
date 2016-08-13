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
	const wxSize textctrl_size(400, 40);
	wxBoxSizer *Y_axis = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *X_axis = new wxBoxSizer(wxHORIZONTAL);
	wxButton *CancelButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
	wxButton *OkButton = new wxButton(this, wxID_OK, _("OK"));
	wxStaticText *AutomaticText = new wxStaticText(this, wxID_ANY, _("Automatic Configuration"));
	wxStaticText *ManualText = new wxStaticText(this, wxID_ANY , _("Manual Configuration"));

	//Apply bold font style to the Automatic/Manual Configuration headers.
	SetBold(AutomaticText);
	SetBold(ManualText);


	m_template_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, textctrl_size, wxTE_PROCESS_ENTER, wxDefaultValidator);
	m_show_limiter = new wxCheckBox(this, wxID_ANY, _("Show frame limiter status"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);
	m_show_videomode = new wxCheckBox(this, wxID_ANY, _("Show current video mode"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);
	m_show_saveslot = new wxCheckBox(this, wxID_ANY, _("Show current save slot"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);
	m_show_threadusage_percentage = new wxCheckBox(this, wxID_ANY, _("Show thread usage"), wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxDefaultValidator);
	m_restore_defaults = new wxButton(this, wxID_DEFAULT, _("Restore Defaults"));

	Y_axis->Add(AutomaticText, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_show_limiter, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_show_videomode, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_show_saveslot, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_show_threadusage_percentage, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(2 * spacer_size);

	Y_axis->Add(ManualText, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	Y_axis->Add(m_template_text, wxSizerFlags().DoubleHorzBorder());
	Y_axis->Add(m_restore_defaults, wxSizerFlags().DoubleHorzBorder());
	Y_axis->AddSpacer(spacer_size);

	X_axis->Add(OkButton, wxSizerFlags().HorzBorder());
	X_axis->Add(CancelButton, wxSizerFlags().HorzBorder());

	*this += Y_axis;
	*this += X_axis | pxSizerFlags::StdButton();

	Bind(wxEVT_BUTTON, &Dialogs::GSFrameConfigurationDialog::ApplySettings, this, OkButton->GetId());
	Bind(wxEVT_TEXT, &Dialogs::GSFrameConfigurationDialog::GrayOut, this, m_template_text->GetId());
	Bind(wxEVT_BUTTON, &Dialogs::GSFrameConfigurationDialog::RestoreDefaults, this, m_restore_defaults->GetId());

	InitValues();
	OkButton->SetFocus();
	SetSizerAndFit(GetSizer());
}

void Dialogs::GSFrameConfigurationDialog::ApplySettings(wxCommandEvent& evt)
{
	AppConfig::UiTemplateOptions& Options = g_Conf->Templates;
	Options.TitleTemplate = m_template_text->GetLineText(m_template_text->GetNumberOfLines());
	Options.ShowLimiter = m_show_limiter->GetValue();
	Options.ShowSaveSlot = m_show_saveslot->GetValue();
	Options.ShowVideoMode = m_show_videomode->GetValue();
	Options.ShowThreadUsage = m_show_threadusage_percentage->GetValue();
	evt.Skip();
}

void Dialogs::GSFrameConfigurationDialog::SetBold(wxStaticText* Text)
{
	wxFont Font = Text->GetFont();
	Font.SetWeight(wxFONTWEIGHT_BOLD);
	Text->SetFont(Font);
}

void Dialogs::GSFrameConfigurationDialog::GrayOutCheckboxes()
{
	bool show = m_template_text->GetValue() == DefaultTitleTemplate;
	m_show_limiter->Enable(show);
	m_show_saveslot->Enable(show);
	m_show_threadusage_percentage->Enable(show);
	m_show_videomode->Enable(show);
}

void Dialogs::GSFrameConfigurationDialog::GrayOut(wxCommandEvent& evt)
{
	GrayOutCheckboxes();
	evt.Skip();
}

void Dialogs::GSFrameConfigurationDialog::RestoreDefaults(wxCommandEvent& evt)
{
	m_template_text->SetValue(DefaultTitleTemplate);
	m_show_limiter->SetValue(true);
	m_show_saveslot->SetValue(true);
	m_show_videomode->SetValue(false);
	m_show_threadusage_percentage->SetValue(true);
	GrayOutCheckboxes();
	evt.Skip();
}

void Dialogs::GSFrameConfigurationDialog::InitValues()
{
	AppConfig::UiTemplateOptions& Options = g_Conf->Templates;
	m_template_text->SetValue(Options.TitleTemplate);
	m_show_limiter->SetValue(Options.ShowLimiter);
	m_show_saveslot->SetValue(Options.ShowSaveSlot);
	m_show_videomode->SetValue(Options.ShowVideoMode);
	m_show_threadusage_percentage->SetValue(Options.ShowThreadUsage);
}
