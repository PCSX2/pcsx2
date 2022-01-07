#pragma once
#include "Host.h"

namespace Host
{
	// UI thread
	void GSWindowResized(int width, int height, float scale);

	// MTGS thread
	void CheckForGSWindowResize();
} // namespace Host

