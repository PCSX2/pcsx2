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

#include "Resources/EmbeddedImage.h"
#include "Resources/Logo.h"

#include <wx/mstream.h>
#include <wx/hyperlink.h>

using namespace pxSizerFlags;

// --------------------------------------------------------------------------------------
//  AboutBoxDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::AboutBoxDialog::AboutBoxDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, AddAppName(_("About %s")), pxDialogFlags())
	, m_bitmap_logo(this, wxID_ANY, wxBitmap(EmbeddedImage<res_Logo>().Get()),
		wxDefaultPosition, wxDefaultSize
		)
{
	// [TODO] : About box should be upgraded to use scrollable read-only text boxes.

	wxString developsString = wxsFormat(
		L"avih, Refraction, rama, pseudonym, gregory.hainaut"
		L"\n\n"
		L"%s: \n"
		L"Arcum42, Aumatt, drk||raziel, "
		L"cottonvibes, gigaherz, saqib, "
		L"Alexey silinov, Aumatt, "
		L"Florin, goldfinger, Linuzappz, loser, "
		L"Nachbrenner, shadow, Zerofrog, tmkk, Jake.Stine"
		L"\n\n"
		L"%s:\n"
		L"CKemu, Falcon4ever, Bositman",
		_("Previous versions"), _("Webmasters"));

	wxString contribsString = wxsFormat(
		L"%s: \n"
		L"ChickenLiver(Lilypad), Gabest (Gsdx, Cdvdolio, Xpad)"
		L"\n\n"
		L"%s: \n"
		L"black_wd, Belmont, BGome, _Demo_, Dreamtime, Hiryu and Sjeep, nneeve, Shadow Lady,"
		L"F|RES, Jake.Stine, MrBrown, razorblade, Seta-san, Skarmeth, feal87, Athos",
		_("Plugin Specialists"), _("Special thanks to"));

	wxFlexGridSizer& boxesContainer = *new wxFlexGridSizer(2, 0, StdPadding);
	boxesContainer.AddGrowableCol(0, 1);
	boxesContainer.AddGrowableCol(1, 1);

	wxStaticBoxSizer& developsBox = *new wxStaticBoxSizer(wxVERTICAL, this);
	wxStaticBoxSizer& contribsBox = *new wxStaticBoxSizer(wxVERTICAL, this);

	pxStaticText& developsText = Text(developsString).SetMinWidth(240);
	pxStaticText& contribsText = Text(contribsString).SetMinWidth(240);

	developsBox += Heading(_("Developers")).Bold() | StdExpand();
	developsBox += developsText | StdExpand();
	contribsBox += Heading(_("Contributors")).Bold() | StdExpand();
	contribsBox += contribsText | StdExpand();

	boxesContainer += developsBox | StdExpand();
	boxesContainer += contribsBox | StdExpand();

	// Main layout
	*this += m_bitmap_logo | StdCenter();

	*this += Text(_("PlayStation 2 Emulator"));

	*this += new wxHyperlinkCtrl(this, wxID_ANY,
		_("PCSX2 Official Website and Forums"), L"http://www.pcsx2.net"
		) | pxProportion(1).Center().Border(wxALL, 3);

	*this += new wxHyperlinkCtrl(this, wxID_ANY,
		_("PCSX2 Official Git Repository at GitHub"), L"https://github.com/PCSX2/pcsx2"
		) | pxProportion(1).Center().Border(wxALL, 3);

	*this += boxesContainer | StdCenter();

	wxButton& closeButton = *new wxButton(this, wxID_OK, _("I've seen enough"));
	closeButton.SetFocus();
	*this += closeButton | StdCenter();

	SetSizerAndFit(GetSizer());
}
