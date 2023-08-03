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
#include <memory>
#include <string>

class GSRenderer : public GSState
{
private:
	bool Merge(int field);
	bool BeginPresentFrame(bool frame_skip);
	void EndPresentFrame();

	u64 m_shader_time_start = 0;

	std::string m_snapshot;
	u32 m_dump_frames = 0;
	u32 m_skipped_duplicate_frames = 0;

	// Tracking draw counters for idle frame detection.
	int m_last_draw_n = 0;
	int m_last_transfer_n = 0;

protected:
	GSVector2i m_real_size{0, 0};
	bool m_texture_shuffle = false;
	bool m_copy_16bit_to_target_shuffle = false;
	bool m_same_group_texture_shuffle = false;

	virtual GSTexture* GetOutput(int i, float& scale, int& y_offset) = 0;
	virtual GSTexture* GetFeedbackOutput(float& scale) { return nullptr; }

public:
	GSRenderer();
	virtual ~GSRenderer();

	virtual void Reset(bool hardware_reset) override;

	virtual void Destroy();

	virtual void UpdateRenderFixes();

	void PurgePool();

	virtual void VSync(u32 field, bool registers_written, bool idle_frame);
	virtual bool CanUpscale() { return false; }
	virtual float GetUpscaleMultiplier() { return 1.0f; }
	virtual float GetTextureScaleFactor() { return 1.0f; }
	GSVector2i GetInternalResolution();
	float GetModXYOffset();

	virtual GSTexture* LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, float* scale, const GSVector2i& size);

	bool IsIdleFrame() const;

	bool SaveSnapshotToMemory(u32 window_width, u32 window_height, bool apply_aspect, bool crop_borders,
		u32* width, u32* height, std::vector<u32>* pixels);

	void QueueSnapshot(const std::string& path, u32 gsdump_frames);
	void StopGSDump();
	void PresentCurrentFrame();

	bool BeginCapture(std::string filename, const GSVector2i& size = GSVector2i(0, 0));
	void EndCapture();
};

extern std::unique_ptr<GSRenderer> g_gs_renderer;
