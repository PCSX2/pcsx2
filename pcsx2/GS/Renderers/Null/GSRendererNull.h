// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSRenderer.h"

class GSRendererNull final : public GSRenderer
{
public:
	GSRendererNull();

protected:
	void VSync(u32 field, bool registers_written, bool idle_frame) override;
	void Draw() override;
	GSTexture* GetOutput(int i, float& scale, int& y_offset) override;

	// The Null backend draws nothing, so it supports no coverage-alpha (AA1) path.
	// HW and SW override this; without an override here the base GSState version
	// pxFailRel("Not implemented")s, which aborts on AA1 games (e.g. Shadow of the
	// Colossus) under --renderer null.
	bool IsCoverageAlphaSupported() override;
};
