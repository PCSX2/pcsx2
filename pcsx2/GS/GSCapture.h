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

#include "GSVector.h"
#include "GSPng.h"

#include <ghc/filesystem.h>

#ifdef _WIN32
#include <wil/com.h>
#include <streams.h>
#endif

#include "Window/wx/GSCaptureDlg.h"

class GSCapture
{
	std::recursive_mutex m_lock;
	bool m_capturing;
	GSVector2i m_size;
	uint64 m_frame;
	int m_threads;

#ifdef _WIN32

	wil::com_ptr_failfast<IGraphBuilder> m_graph;
	wil::com_ptr_failfast<IBaseFilter> m_src;

#elif defined(__unix__)

	std::vector<std::unique_ptr<GSPng::Worker>> m_workers;
	int m_compression_level;

#endif

public:
	GSCapture();
	virtual ~GSCapture();

	bool BeginCapture(wxWindow* parentWindow, float fps, GSVector2i recommendedResolution, float aspect, ghc::filesystem::path& savedToPath);
	bool DeliverFrame(const void* bits, int pitch, bool rgba);
	bool EndCapture();

	bool IsCapturing() { return m_capturing; }
	GSVector2i GetSize() { return m_size; }

private:
	GSCaptureDlg* m_settingsDialog = NULL;
};
