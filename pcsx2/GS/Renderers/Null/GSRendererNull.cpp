// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSRendererNull.h"

GSRendererNull::GSRendererNull() = default;

void GSRendererNull::Draw()
{
}

GSTexture* GSRendererNull::GetOutput(int i, float& scale, int& y_offset)
{
	return nullptr;
}
