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
#include "ConsoleLogger.h"

#include <memory>

pxLogTextCtrl::pxLogTextCtrl( wxWindow* parent )
	: wxTextCtrl( parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_NOHIDESEL
	)
{
	Bind(wxEVT_SCROLLWIN_THUMBTRACK, &pxLogTextCtrl::OnThumbTrack, this);
	Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &pxLogTextCtrl::OnThumbRelease, this);
}

#ifdef __WXMSW__
void pxLogTextCtrl::WriteText(const wxString& text)
{
	// Don't need the update message -- saves some overhead.
	DoWriteText( text, SetValue_SelectionOnly );
}
#endif

void pxLogTextCtrl::OnThumbTrack(wxScrollWinEvent& evt)
{
	//Console.Warning( "Thumb Tracking!!!" );
	if( !m_IsPaused )
		m_IsPaused = std::unique_ptr<ScopedCoreThreadPause>(new ScopedCoreThreadPause());

	evt.Skip();
}

void pxLogTextCtrl::OnThumbRelease(wxScrollWinEvent& evt)
{
	//Console.Warning( "Thumb Releasing!!!" );
	if( m_IsPaused )
	{
		m_IsPaused->AllowResume();
		m_IsPaused = nullptr;
	}
	evt.Skip();
}

pxLogTextCtrl::~pxLogTextCtrl() throw()
{
}
