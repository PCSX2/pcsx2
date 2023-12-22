// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSTexture.h"

#include <atomic>

class GSTextureSW final : public GSTexture
{
	// mem texture, always 32-bit rgba (might add 8-bit for palette if needed)

	int m_pitch;
	void* m_data;
	std::atomic_flag m_mapped;

public:
	GSTextureSW(Type type, int width, int height);
	~GSTextureSW() override;

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void Unmap() override;
	bool Save(const std::string& fn) override;
	void Swap(GSTexture* tex) override;
	void* GetNativeHandle() const override;
};
