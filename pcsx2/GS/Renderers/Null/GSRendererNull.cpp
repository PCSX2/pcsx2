// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSRendererNull.h"

GSRendererNull::GSRendererNull() = default;

void GSRendererNull::VSync(u32 field, bool registers_written, bool idle_frame)
{
	GSRenderer::VSync(field, registers_written, idle_frame);

	m_draw_transfers.clear();
}

void GSRendererNull::Draw()
{
}

GSTexture* GSRendererNull::GetOutput(int i, float& scale, int& y_offset)
{
	return nullptr;
}
