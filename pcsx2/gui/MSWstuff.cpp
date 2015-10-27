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

#include "MSWstuff.h"

#ifdef __WXMSW__
#	include <wx/msw/wrapwin.h>		// needed for OutputDebugString
#endif

void MSW_SetWindowAfter( WXWidget hwnd, WXWidget hwndAfter )
{
#ifdef __WXMSW__
	SetWindowPos( (HWND)hwnd, (HWND)hwndAfter, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE );
#endif
}

// Text scales automatically on Windows but that's about it. The dialog widths
// and images need to be scaled manually.
float MSW_GetDPIScale()
{
#ifdef __WXMSW__
	HDC screen = GetDC(0);
	float scale = GetDeviceCaps(screen, LOGPIXELSX) / 96.0; // 96.0 dpi = 100% scale
	ReleaseDC(NULL, screen);

	return scale;
#else
	return 1.0;
#endif
}
