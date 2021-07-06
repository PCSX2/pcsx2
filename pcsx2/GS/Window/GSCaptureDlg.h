/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

#include "GSDialog.h"
#include "GS/resource.h"
#include <streams.h>
#include <wil/com.h>

class GSCaptureDlg : public GSDialog
{
	struct Codec
	{
		wil::com_ptr_nothrow<IMoniker> moniker;
		wil::com_ptr_nothrow<IBaseFilter> filter;
		std::wstring FriendlyName;
		std::wstring DisplayName;
	};

	std::list<Codec> m_codecs;

	int GetSelCodec(Codec& c);
	void UpdateConfigureButton();

protected:
	void OnInit();
	bool OnCommand(HWND hWnd, UINT id, UINT code);

public:
	GSCaptureDlg();
	void InvalidFile();

	int m_width;
	int m_height;
	std::wstring m_filename;
	INT_PTR m_colorspace;
	wil::com_ptr_nothrow<IBaseFilter> m_enc;
};
