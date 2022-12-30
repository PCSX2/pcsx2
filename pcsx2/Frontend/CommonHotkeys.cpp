/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "Frontend/CommonHost.h"
#include "Frontend/FullscreenUI.h"
#include "Frontend/InputManager.h"
#include "GS.h"
#include "Host.h"
#include "HostDisplay.h"
#include "IconsFontAwesome5.h"
#include "Recording/InputRecording.h"
#include "SPU2/spu2.h"
#include "VMManager.h"

#ifdef ENABLE_ACHIEVEMENTS
#include "Frontend/Achievements.h"
#endif

static s32 s_current_save_slot = 1;
static std::optional<LimiterModeType> s_limiter_mode_prior_to_hold_interaction;

void CommonHost::Internal::ResetVMHotkeyState()
{
	s_current_save_slot = 1;
	s_limiter_mode_prior_to_hold_interaction.reset();
}

static void HotkeyAdjustTargetSpeed(double delta)
{
	const double min_speed = Achievements::ChallengeModeActive() ? 1.0 : 0.1;
	EmuConfig.Framerate.NominalScalar = std::max(min_speed, EmuConfig.GS.LimitScalar + delta);
	VMManager::SetLimiterMode(LimiterModeType::Nominal);
	Host::AddIconOSDMessage("SpeedChanged", ICON_FA_CLOCK,
		fmt::format("Target speed set to {:.0f}%.", std::round(EmuConfig.Framerate.NominalScalar * 100.0)), Host::OSD_QUICK_DURATION);
}

static void HotkeyAdjustVolume(s32 fixed, s32 delta)
{
	if (!VMManager::HasValidVM())
		return;

	const s32 current_vol = SPU2::GetOutputVolume();
	const s32 new_volume = std::clamp((fixed >= 0) ? fixed : (current_vol + delta), 0, Pcsx2Config::SPU2Options::MAX_VOLUME);
	if (current_vol != new_volume)
		SPU2::SetOutputVolume(new_volume);

	if (new_volume == 0)
	{
		Host::AddIconOSDMessage("VolumeChanged", ICON_FA_VOLUME_MUTE, "Volume: Muted");
	}
	else
	{
		Host::AddIconOSDMessage(
			"VolumeChanged", (current_vol < new_volume) ? ICON_FA_VOLUME_UP : ICON_FA_VOLUME_DOWN, fmt::format("Volume: {}%", new_volume));
	}
}

static constexpr s32 CYCLE_SAVE_STATE_SLOTS = 10;

static void HotkeyCycleSaveSlot(s32 delta)
{
	// 1..10
	s_current_save_slot = ((s_current_save_slot - 1) + delta);
	if (s_current_save_slot < 0)
		s_current_save_slot = CYCLE_SAVE_STATE_SLOTS;
	else
		s_current_save_slot = (s_current_save_slot % CYCLE_SAVE_STATE_SLOTS) + 1;

	const u32 crc = VMManager::GetGameCRC();
	const std::string serial(VMManager::GetGameSerial());
	const std::string filename(VMManager::GetSaveStateFileName(serial.c_str(), crc, s_current_save_slot));
	FILESYSTEM_STAT_DATA sd;
	if (!filename.empty() && FileSystem::StatFile(filename.c_str(), &sd))
	{
		char date_buf[128] = {};
#ifdef _WIN32
		ctime_s(date_buf, std::size(date_buf), &sd.ModificationTime);
#else
		ctime_r(&sd.ModificationTime, date_buf);
#endif

		// remove terminating \n
		size_t len = std::strlen(date_buf);
		if (len > 0 && date_buf[len - 1] == '\n')
			date_buf[len - 1] = 0;

		Host::AddIconOSDMessage("CycleSaveSlot", ICON_FA_SEARCH,
			fmt::format("Save slot {} selected (last save: {}).", s_current_save_slot, date_buf), Host::OSD_QUICK_DURATION);
	}
	else
	{
		Host::AddIconOSDMessage("CycleSaveSlot", ICON_FA_SEARCH, fmt::format("Save slot {} selected (no save yet).", s_current_save_slot),
			Host::OSD_QUICK_DURATION);
	}
}

static void HotkeyLoadStateSlot(s32 slot)
{
	const u32 crc = VMManager::GetGameCRC();
	if (crc == 0)
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE, "Cannot load state from a slot without a game running.",
			Host::OSD_INFO_DURATION);
		return;
	}

	const std::string serial(VMManager::GetGameSerial());
	if (!VMManager::HasSaveStateInSlot(serial.c_str(), crc, slot))
	{
		Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE, fmt::format("No save state found in slot {}.", slot),
			Host::OSD_INFO_DURATION);
		return;
	}

	VMManager::LoadStateFromSlot(slot);
}

static void HotkeySaveStateSlot(s32 slot)
{
	if (VMManager::GetGameCRC() == 0)
	{
		Host::AddIconOSDMessage("SaveStateToSlot", ICON_FA_EXCLAMATION_TRIANGLE, "Cannot save state to a slot without a game running.",
			Host::OSD_INFO_DURATION);
		return;
	}

	VMManager::SaveStateToSlot(slot);
}

BEGIN_HOTKEY_LIST(g_common_hotkeys)
DEFINE_HOTKEY("OpenPauseMenu", "System", "Open Pause Menu", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		FullscreenUI::OpenPauseMenu();
})
#ifdef ENABLE_ACHIEVEMENTS
DEFINE_HOTKEY("OpenAchievementsList", "System", "Open Achievements List", [](s32 pressed) {
	if (!pressed)
		FullscreenUI::OpenAchievementsWindow();
})
DEFINE_HOTKEY("OpenLeaderboardsList", "System", "Open Leaderboards List", [](s32 pressed) {
	if (!pressed)
		FullscreenUI::OpenLeaderboardsWindow();
})
#endif
DEFINE_HOTKEY("TogglePause", "System", "Toggle Pause", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		VMManager::SetPaused(VMManager::GetState() != VMState::Paused);
})
DEFINE_HOTKEY("ToggleFullscreen", "System", "Toggle Fullscreen", [](s32 pressed) {
	if (!pressed)
		Host::SetFullscreen(!Host::IsFullscreen());
})
DEFINE_HOTKEY("ToggleFrameLimit", "System", "Toggle Frame Limit", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
	{
		VMManager::SetLimiterMode(
			(EmuConfig.LimiterMode != LimiterModeType::Unlimited) ? LimiterModeType::Unlimited : LimiterModeType::Nominal);
	}
})
DEFINE_HOTKEY("ToggleTurbo", "System", "Toggle Turbo / Fast Forward", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
	{
		VMManager::SetLimiterMode((EmuConfig.LimiterMode != LimiterModeType::Turbo) ? LimiterModeType::Turbo : LimiterModeType::Nominal);
	}
})
DEFINE_HOTKEY("ToggleSlowMotion", "System", "Toggle Slow Motion", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
	{
		VMManager::SetLimiterMode((EmuConfig.LimiterMode != LimiterModeType::Slomo) ? LimiterModeType::Slomo : LimiterModeType::Nominal);
	}
})
DEFINE_HOTKEY("HoldTurbo", "System", "Turbo / Fast Forward (Hold)", [](s32 pressed) {
	if (!VMManager::HasValidVM())
		return;
	if (pressed > 0 && !s_limiter_mode_prior_to_hold_interaction.has_value())
	{
		s_limiter_mode_prior_to_hold_interaction = VMManager::GetLimiterMode();
		VMManager::SetLimiterMode((s_limiter_mode_prior_to_hold_interaction.value() != LimiterModeType::Turbo) ? LimiterModeType::Turbo :
																												 LimiterModeType::Nominal);
	}
	else if (pressed >= 0 && s_limiter_mode_prior_to_hold_interaction.has_value())
	{
		VMManager::SetLimiterMode(s_limiter_mode_prior_to_hold_interaction.value());
		s_limiter_mode_prior_to_hold_interaction.reset();
	}
})
DEFINE_HOTKEY("IncreaseSpeed", "System", "Increase Target Speed", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyAdjustTargetSpeed(0.1);
})
DEFINE_HOTKEY("DecreaseSpeed", "System", "Decrease Target Speed", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyAdjustTargetSpeed(-0.1);
})
DEFINE_HOTKEY("IncreaseVolume", "System", "Increase Volume", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyAdjustVolume(-1, 5);
})
DEFINE_HOTKEY("DecreaseVolume", "System", "Decrease Volume", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyAdjustVolume(-1, -5);
})
DEFINE_HOTKEY("Mute", "System", "Toggle Mute", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyAdjustVolume((SPU2::GetOutputVolume() == 0) ? EmuConfig.SPU2.FinalVolume : 0, 0);
})
DEFINE_HOTKEY("FrameAdvance", "System", "Frame Advance", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		VMManager::FrameAdvance(1);
})
DEFINE_HOTKEY("ShutdownVM", "System", "Shut Down Virtual Machine", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		Host::RequestVMShutdown(true, true, EmuConfig.SaveStateOnShutdown);
})
DEFINE_HOTKEY("ResetVM", "System", "Reset Virtual Machine", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		VMManager::Reset();
})
DEFINE_HOTKEY("InputRecToggleMode", "System", "Toggle Input Recording Mode", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		g_InputRecording.getControls().toggleRecordMode();
})

DEFINE_HOTKEY("PreviousSaveStateSlot", "Save States", "Select Previous Save Slot", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyCycleSaveSlot(-1);
})
DEFINE_HOTKEY("NextSaveStateSlot", "Save States", "Select Next Save Slot", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyCycleSaveSlot(1);
})
DEFINE_HOTKEY("SaveStateToSlot", "Save States", "Save State To Selected Slot", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		VMManager::SaveStateToSlot(s_current_save_slot);
})
DEFINE_HOTKEY("LoadStateFromSlot", "Save States", "Load State From Selected Slot", [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyLoadStateSlot(s_current_save_slot);
})

#define DEFINE_HOTKEY_SAVESTATE_X(slotnum) \
	DEFINE_HOTKEY("SaveStateToSlot" #slotnum, "Save States", "Save State To Slot " #slotnum, [](s32 pressed) { \
		if (!pressed) \
			HotkeySaveStateSlot(slotnum); \
	})
#define DEFINE_HOTKEY_LOADSTATE_X(slotnum) \
	DEFINE_HOTKEY("LoadStateFromSlot" #slotnum, "Save States", "Load State From Slot " #slotnum, [](s32 pressed) { \
		if (!pressed) \
			HotkeyLoadStateSlot(slotnum); \
	})
DEFINE_HOTKEY_SAVESTATE_X(1)
DEFINE_HOTKEY_LOADSTATE_X(1)
DEFINE_HOTKEY_SAVESTATE_X(2)
DEFINE_HOTKEY_LOADSTATE_X(2)
DEFINE_HOTKEY_SAVESTATE_X(3)
DEFINE_HOTKEY_LOADSTATE_X(3)
DEFINE_HOTKEY_SAVESTATE_X(4)
DEFINE_HOTKEY_LOADSTATE_X(4)
DEFINE_HOTKEY_SAVESTATE_X(5)
DEFINE_HOTKEY_LOADSTATE_X(5)
DEFINE_HOTKEY_SAVESTATE_X(6)
DEFINE_HOTKEY_LOADSTATE_X(6)
DEFINE_HOTKEY_SAVESTATE_X(7)
DEFINE_HOTKEY_LOADSTATE_X(7)
DEFINE_HOTKEY_SAVESTATE_X(8)
DEFINE_HOTKEY_LOADSTATE_X(8)
DEFINE_HOTKEY_SAVESTATE_X(9)
DEFINE_HOTKEY_LOADSTATE_X(9)
DEFINE_HOTKEY_SAVESTATE_X(10)
DEFINE_HOTKEY_LOADSTATE_X(10)
#undef DEFINE_HOTKEY_SAVESTATE_X
#undef DEFINE_HOTKEY_LOADSTATE_X
END_HOTKEY_LIST()