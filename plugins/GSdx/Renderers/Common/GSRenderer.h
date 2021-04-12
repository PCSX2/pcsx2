/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GSdx.h"
#include "Window/GSWnd.h"
#include "GSState.h"
#include "GSCapture.h"

class GSRenderer : public GSState
{
	GSCapture m_capture;
	std::string m_snapshot;
	int m_shader;

	bool Merge(int field);

	bool m_shift_key;
	bool m_control_key;

protected:
	int m_dithering;
	int m_interlace;
	int m_aspectratio;
	int m_vsync;
	bool m_aa1;
	bool m_shaderfx;
	bool m_fxaa;
	bool m_shadeboost;
	bool m_texture_shuffle;
	GSVector2i m_real_size;

	virtual GSTexture* GetOutput(int i, int& y_offset) = 0;
	virtual GSTexture* GetFeedbackOutput() { return nullptr; }

public:
	std::shared_ptr<GSWnd> m_wnd;
	GSDevice* m_dev;

public:
	GSRenderer();
	virtual ~GSRenderer();

	virtual bool CreateDevice(GSDevice* dev);
	virtual void ResetDevice();
	virtual void VSync(int field);
	virtual bool MakeSnapshot(const std::string& path);
	virtual void KeyEvent(GSKeyEventData* e);
	virtual bool CanUpscale() { return false; }
	virtual int GetUpscaleMultiplier() { return 1; }
	virtual GSVector2i GetCustomResolution() { return GSVector2i(0, 0); }
	GSVector2i GetInternalResolution();
	void SetAspectRatio(int aspect) { m_aspectratio = aspect; }
	void SetVSync(int vsync);

	virtual bool BeginCapture(std::string& filename);
	virtual void EndCapture();

	void PurgePool();

public:
	std::mutex m_pGSsetTitle_Crit;

	char m_GStitleInfoBuffer[128];
};
