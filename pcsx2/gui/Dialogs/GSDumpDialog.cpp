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
#include "MSWstuff.h"

#include "Dialogs/ModalPopups.h"


#include "Utilities/EmbeddedImage.h"
#include "Resources/NoIcon.h"

#include "PathDefs.h"
#include "AppConfig.h"

#include <wx/mstream.h>
#include <wx/listctrl.h>
#include <wx/filepicker.h>
#include <wx/radiobut.h>
#include <wx/button.h>
#include <wx/treectrl.h>
#include <wx/checkbox.h>
#include <wx/dir.h>

using namespace pxSizerFlags;

// --------------------------------------------------------------------------------------
//  GSDumpDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::GSDumpDialog::GSDumpDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, _("GSDumpGov"), pxDialogFlags())
	, m_dump_list(new wxListView(this, wxID_ANY, wxDefaultPosition, wxSize(250, 200)))
{
	const float scale = MSW_GetDPIScale();
	SetMinWidth(scale * 460);

	wxFlexGridSizer& general(*new wxFlexGridSizer(2, StdPadding, StdPadding));
	wxBoxSizer& dump_info(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dump_preview(*new wxBoxSizer(wxVERTICAL));
	wxFlexGridSizer& debugger(*new wxFlexGridSizer(2, StdPadding, StdPadding));
	wxBoxSizer& dumps(*new wxBoxSizer(wxHORIZONTAL));
	wxBoxSizer& dbg_tree(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dbg_actions(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& gif(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dumps_list(*new wxBoxSizer(wxVERTICAL));

	dump_info += new wxRadioButton(this, wxID_ANY, _("None"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("D3D11 HW"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("OGL HW"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("OGL SW"));
	dump_info += new wxButton(this, wxID_ANY, _("Run"));



	// debugger
	dbg_tree += new wxStaticText(this, wxID_ANY, _("GIF Packets"));
	dbg_tree += new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 200));
	dbg_actions += new wxCheckBox(this, wxID_ANY, _("Debug Mode"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Go to Start"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Step"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Run to Selection"));
	dbg_actions += new wxButton(this, wxID_ANY, _("Go to next VSync"));

	// gif
	gif += new wxStaticText(this, wxID_ANY, _("Packet Content"));
	gif += new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 200));


	debugger += dbg_tree;
	debugger += dbg_actions;

	GetDumpsList();

	dumps_list += new wxStaticText(this, wxID_ANY, _("GS Dumps List"));
	dumps_list += m_dump_list;

	dump_preview += new wxStaticText(this, wxID_ANY, _("Preview"));
	wxImage img = EmbeddedImage<res_NoIcon>().Get();
	img.Rescale(250 * scale, 200 * scale, wxIMAGE_QUALITY_HIGH);
	dump_preview += new wxStaticBitmap(this, wxID_ANY, wxBitmap(img));


	dumps += dumps_list;
	dumps += dump_info;
	dumps += dump_preview;

	general += dumps;
	general += dump_info;
	general += debugger;
	general += gif;

	*this += general;

	SetSizerAndFit(GetSizer());

	Bind(wxEVT_LIST_ITEM_SELECTED, &Dialogs::GSDumpDialog::SelectedDump, this, ID_DUMP_LIST);
}

void Dialogs::GSDumpDialog::GetDumpsList()
{
	wxDir snaps(g_Conf->Folders.Snapshots.GetFilename().GetName());
	wxString filename;
	bool cont = snaps.GetFirst(&filename, "*.gs", wxDIR_DEFAULT);
	int i = 0;
	while (cont)
	{
		m_dump_list->InsertItem(i, filename.c_str());
		i++;
		cont = snaps.GetNext(&filename);
	}
}

Dialogs::GSDumpDialog::~GSDumpDialog()
{
	// we can't use smart pointers because of wxWidgets operator overload so we
	// do the next best thing and handle the manual memory management ourselves
	delete m_dump_list;
}

void Dialogs::GSDumpDialog::SelectedDump(wxListEvent& evt)
{
	//evt->GetText();
}
