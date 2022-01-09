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

#include "GS/GSState.h"
#include "GS/GSCapture.h"
#include <memory>

struct HostKeyEvent;

class GSRenderer : public GSState
{
	GSCapture m_capture;
	std::string m_snapshot;

	bool Merge(int field);

	bool m_shift_key;
	bool m_control_key;

protected:
	bool m_texture_shuffle;
	GSVector2i m_real_size;

	virtual GSTexture* GetOutput(int i, int& y_offset) = 0;
	virtual GSTexture* GetFeedbackOutput() { return nullptr; }

public:
	GSRenderer();
	virtual ~GSRenderer();

	virtual void Destroy();

	virtual void VSync(u32 field, bool registers_written);
	virtual bool MakeSnapshot(const std::string& path);
	virtual void KeyEvent(const HostKeyEvent& e);
	virtual bool CanUpscale() { return false; }
	virtual int GetUpscaleMultiplier() { return 1; }
	virtual GSVector2i GetCustomResolution() { return GSVector2i(0, 0); }
	virtual GSVector2 GetTextureScaleFactor() { return { 1.0f, 1.0f }; }
	GSVector2i GetInternalResolution();

	virtual bool BeginCapture(std::string& filename);
	virtual void EndCapture();

	virtual void PurgePool() override;
	virtual void PurgeTextureCache();

	bool SaveSnapshotToMemory(u32 width, u32 height, std::vector<u32>* pixels);
};
