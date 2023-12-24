// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSRenderer.h"

class GSRendererNull final : public GSRenderer
{
public:
	GSRendererNull();

protected:
	void Draw() override;
	GSTexture* GetOutput(int i, float& scale, int& y_offset) override;
};
