/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "SaveslotPreview.h"

#include "wx/dcbuffer.h"
#include "wx/event.h"

SaveslotPreview::SaveslotPreview(wxWindow *parent) : wxPopupWindow(parent)
{
	Bind(wxEVT_ERASE_BACKGROUND, &SaveslotPreview::OnEraseBackground, this);
	Bind(wxEVT_PAINT, &SaveslotPreview::OnPaint, this);

	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetDoubleBuffered(true);
}

void SaveslotPreview::SetImagePath(wxString path)
{
	imagePath = path;
	Refresh();
}

void SaveslotPreview::OnPaint(wxPaintEvent &event)
{
	wxPaintDC dc(this);
	if (imagePath == wxEmptyString)
	{
		return;
	}
	wxImage *img = new wxImage(imagePath, wxBITMAP_TYPE_PNG);
	if (img->IsOk())
	{
		dc.DrawBitmap(img->Scale(this->GetClientSize().x, this->GetClientSize().y, wxIMAGE_QUALITY_HIGH), 0, 0, false);
	}
}

void SaveslotPreview::OnEraseBackground(wxEraseEvent &event)
{
	// Intentionally Empty
	// See - https://wiki.wxwidgets.org/Flicker-Free_Drawing
}
