// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GameMode.h"

#if defined(__linux__)
#include <gamemode_client.h>
#include "common/Console.h"

bool GameMode::IsAvailable()
{
	static const bool available = []() {
		const bool ok = (gamemode_query_status() != -1);
		if (!ok)
			Console.Warning("GameMode: GameMode daemon is not available on this system.");
		return ok;
	}();
	return available;
}

void GameMode::Update(bool enabled)
{
	if (!IsAvailable())
		return;

	const int status = gamemode_query_status();
	if (status < 0)
		return;

	if (enabled)
	{
		if (status > 0)
			return;

		if (gamemode_request_start() == 0)
			Console.WriteLn("GameMode: GameMode has been started.");
		else
			Console.Warning("GameMode: Failed to start GameMode: %s", gamemode_error_string());
	}
	else
	{
		if (status != 2)
			return;

		if (gamemode_request_end() == 0)
			Console.WriteLn("GameMode: GameMode has been stopped.");
		else
			Console.Warning("GameMode: Failed to end GameMode: %s", gamemode_error_string());
	}
}
#endif
