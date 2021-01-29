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

#include <wx/mstream.h>
#include <wx/listctrl.h>
#include <wx/filepicker.h>
#include <wx/radiobut.h>
#include <wx/button.h>
#include <wx/treectrl.h>
#include <wx/checkbox.h>

using namespace pxSizerFlags;

// --------------------------------------------------------------------------------------
//  GSDumpDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::GSDumpDialog::GSDumpDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, _("GSDumpGov"), pxDialogFlags())
{
	const float scale = MSW_GetDPIScale();
	SetMinWidth(scale * 460);

#ifdef _WIN32
	const int padding = 15;
#else
	const int padding = 8;
#endif


	wxFlexGridSizer& general(*new wxFlexGridSizer(2, StdPadding, StdPadding));
	wxBoxSizer& dump_info(*new wxBoxSizer(wxVERTICAL));
	wxFlexGridSizer& debugger(*new wxFlexGridSizer(2, StdPadding, StdPadding));
	wxBoxSizer& dbg_tree(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& dbg_actions(*new wxBoxSizer(wxVERTICAL));
	wxBoxSizer& gif(*new wxBoxSizer(wxVERTICAL));

	// dump list
	//general += new wxListView(this, wxID_ANY);

	// dump directory
	//general += new wxDirPickerCtrl(this, wxID_ANY);
	//general += padding;

	// renderer override
	dump_info += new wxRadioButton(this, wxID_ANY, _("None"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("D3D11 HW"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("OGL HW"));
	dump_info += new wxRadioButton(this, wxID_ANY, _("OGL SW"));
	dump_info += padding;

	// dump screenshot
	//
	// wxImage img = EmbeddedImage<res_Logo>().Get();
	// img.Rescale(img.GetWidth() * scale, img.GetHeight() * scale, wxIMAGE_QUALITY_HIGH);
	// auto bitmap_logo = new wxStaticBitmap(this, wxID_ANY, wxBitmap(img));

	// launch dump
	dump_info += new wxButton(this, wxID_ANY, _("Run"));
	dump_info += padding;



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

	general += new wxListView(this, wxID_ANY);
	general += dump_info;
	general += debugger;
	general += gif;

	*this += general;

	SetSizerAndFit(GetSizer());
}
