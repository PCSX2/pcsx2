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
#include "gui/AppCommon.h"
#include "gui/MSWstuff.h"

#include "gui/Dialogs/ModalPopups.h"

#include "gui/EmbeddedImage.h"
#include "gui/Resources/Logo.h"

#include <wx/mstream.h>
#include <wx/hyperlink.h>

using namespace pxSizerFlags;

// --------------------------------------------------------------------------------------
//  AboutBoxDialog  Implementation
// --------------------------------------------------------------------------------------

Dialogs::AboutBoxDialog::AboutBoxDialog(wxWindow* parent)
	: wxDialogWithHelpers(parent, AddAppName(_("About %s")), pxDialogFlags())
{
	const float scale = MSW_GetDPIScale();
	SetMinWidth(scale * 460);

	wxImage img = EmbeddedImage<res_Logo>().Get();
	img.Rescale(img.GetWidth() * scale, img.GetHeight() * scale, wxIMAGE_QUALITY_HIGH);
	auto bitmap_logo = new wxStaticBitmap(this, wxID_ANY, wxBitmap(img));

	*this += bitmap_logo | StdCenter();

#ifdef _WIN32
	const int padding = 15;
#else
	const int padding = 8;
#endif

	wxBoxSizer& general(*new wxBoxSizer(wxHORIZONTAL));
	general += new wxHyperlinkCtrl(this, wxID_ANY, _("Website"), L"https://pcsx2.net");
	general += padding;
	general += new wxHyperlinkCtrl(this, wxID_ANY, _("Support Forums"), L"https://forums.pcsx2.net");
	general += padding;
	general += new wxHyperlinkCtrl(this, wxID_ANY, _("GitHub Repository"), L"https://github.com/PCSX2/pcsx2");
	general += padding;
	general += new wxHyperlinkCtrl(this, wxID_ANY, _("License"), L"https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Docs/License.txt");

	*this += Text(_("PlayStation 2 Emulator:"));
	*this += general | StdCenter();
	*this += Text(_("Big thanks to everyone who contributed to the project throughout the years."));

	wxButton& closeButton = *new wxButton(this, wxID_OK, _("Close"));
	closeButton.SetFocus();
	*this += closeButton | StdCenter();

	SetSizerAndFit(GetSizer());
}
