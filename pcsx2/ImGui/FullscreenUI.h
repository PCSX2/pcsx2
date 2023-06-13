/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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
#include "common/Pcsx2Defs.h"
#include "common/ProgressCallback.h"
#include <string>
#include <memory>

struct Pcsx2Config;

namespace FullscreenUI
{
	bool Initialize();
	bool IsInitialized();
	bool HasActiveWindow();
	void CheckForConfigChanges(const Pcsx2Config& old_config);
	void OnVMStarted();
	void OnVMDestroyed();
	void GameChanged(std::string title, std::string path, std::string serial, u32 disc_crc, u32 crc);
	void OpenPauseMenu();
	void OpenAchievementsWindow();
	void OpenLeaderboardsWindow();

	void Shutdown(bool clear_state);
	void Render();
	void InvalidateCoverCache();

	class ProgressCallback final : public BaseProgressCallback
	{
	public:
		ProgressCallback(std::string name);
		~ProgressCallback() override;

		__fi const std::string& GetName() const { return m_name; }

		void PushState() override;
		void PopState() override;

		void SetCancellable(bool cancellable) override;
		void SetTitle(const char* title) override;
		void SetStatusText(const char* text) override;
		void SetProgressRange(u32 range) override;
		void SetProgressValue(u32 value) override;

		void DisplayError(const char* message) override;
		void DisplayWarning(const char* message) override;
		void DisplayInformation(const char* message) override;
		void DisplayDebugMessage(const char* message) override;

		void ModalError(const char* message) override;
		bool ModalConfirmation(const char* message) override;
		void ModalInformation(const char* message) override;

		void SetCancelled();

	private:
		void Redraw(bool force);

		std::string m_name;
		int m_last_progress_percent = -1;
	};
} // namespace FullscreenUI
