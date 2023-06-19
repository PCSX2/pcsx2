/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "Achievements.h"
#include "GS.h"
#include "Host.h"
#include "IconsFontAwesome5.h"
#include "ImGui/FullscreenUI.h"
#include "Input/InputManager.h"
#include "Recording/InputRecording.h"
#include "SPU2/spu2.h"
#include "VMManager.h"

#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/Path.h"

static s32 s_current_save_slot = 1;
static std::optional<LimiterModeType> s_limiter_mode_prior_to_hold_interaction;

void VMManager::Internal::ResetVMHotkeyState()
{
	s_current_save_slot = 1;
	s_limiter_mode_prior_to_hold_interaction.reset();
}

static void HotkeyAdjustTargetSpeed(double delta)
{
	const double min_speed = Achievements::ChallengeModeActive() ? 1.0 : 0.1;
	EmuConfig.Framerate.NominalScalar = std::max(min_speed, EmuConfig.Framerate.NominalScalar + delta);
	EmuConfig.LimiterMode = LimiterModeType::Unlimited; // force update
	VMManager::SetLimiterMode(LimiterModeType::Nominal);
	Host::AddIconOSDMessage("SpeedChanged", ICON_FA_CLOCK,
		fmt::format(TRANSLATE_SV("Hotkeys", "Target speed set to {:.0f}%."),
			std::round(EmuConfig.Framerate.NominalScalar * 100.0)),
		Host::OSD_QUICK_DURATION);
}

static void HotkeyAdjustVolume(s32 fixed, s32 delta)
{
	if (!VMManager::HasValidVM())
		return;

	const s32 current_vol = SPU2::GetOutputVolume();
	const s32 new_volume =
		std::clamp((fixed >= 0) ? fixed : (current_vol + delta), 0, Pcsx2Config::SPU2Options::MAX_VOLUME);
	if (current_vol != new_volume)
		SPU2::SetOutputVolume(new_volume);

	if (new_volume == 0)
	{
		Host::AddIconOSDMessage("VolumeChanged", ICON_FA_VOLUME_MUTE, TRANSLATE_STR("Hotkeys", "Volume: Muted"));
	}
	else
	{
		Host::AddIconOSDMessage("VolumeChanged", (current_vol < new_volume) ? ICON_FA_VOLUME_UP : ICON_FA_VOLUME_DOWN,
			fmt::format(TRANSLATE_SV("Hotkeys", "Volume: {}%"), new_volume));
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

	const u32 crc = VMManager::GetDiscCRC();
	const std::string serial = VMManager::GetDiscSerial();
	const std::string filename = VMManager::GetSaveStateFileName(serial.c_str(), crc, s_current_save_slot);
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
			fmt::format(
				TRANSLATE_SV("Hotkeys", "Save slot {} selected (last save: {})."), s_current_save_slot, date_buf),
			Host::OSD_QUICK_DURATION);
	}
	else
	{
		Host::AddIconOSDMessage("CycleSaveSlot", ICON_FA_SEARCH,
			fmt::format(TRANSLATE_SV("Hotkeys", "Save slot {} selected (no save yet)."), s_current_save_slot),
			Host::OSD_QUICK_DURATION);
	}
}

static void HotkeyLoadStateSlot(s32 slot)
{
	// Can reapply settings and thus binds, therefore must be deferred.
	Host::RunOnCPUThread([slot]() {
		if (!VMManager::HasSaveStateInSlot(VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC(), slot))
		{
			Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_EXCLAMATION_TRIANGLE,
				fmt::format(TRANSLATE_SV("Hotkeys", "No save state found in slot {}."), slot), Host::OSD_INFO_DURATION);
			return;
		}

		VMManager::LoadStateFromSlot(slot);
	});
}

static void HotkeySaveStateSlot(s32 slot)
{
	VMManager::SaveStateToSlot(slot);
}

BEGIN_HOTKEY_LIST(g_common_hotkeys)
DEFINE_HOTKEY("OpenPauseMenu", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Open Pause Menu"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			FullscreenUI::OpenPauseMenu();
	})
#ifdef ENABLE_ACHIEVEMENTS
DEFINE_HOTKEY("OpenAchievementsList", TRANSLATE_NOOP("Hotkeys", "System"),
	TRANSLATE_NOOP("Hotkeys", "Open Achievements List"), [](s32 pressed) {
		if (!pressed)
			FullscreenUI::OpenAchievementsWindow();
	})
DEFINE_HOTKEY("OpenLeaderboardsList", TRANSLATE_NOOP("Hotkeys", "System"),
	TRANSLATE_NOOP("Hotkeys", "Open Leaderboards List"), [](s32 pressed) {
		if (!pressed)
			FullscreenUI::OpenLeaderboardsWindow();
	})
#endif
DEFINE_HOTKEY(
	"TogglePause", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Pause"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			VMManager::SetPaused(VMManager::GetState() != VMState::Paused);
	})
DEFINE_HOTKEY("ToggleFullscreen", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Fullscreen"),
	[](s32 pressed) {
		if (!pressed)
			Host::SetFullscreen(!Host::IsFullscreen());
	})
DEFINE_HOTKEY("ToggleFrameLimit", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Frame Limit"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			VMManager::SetLimiterMode((EmuConfig.LimiterMode != LimiterModeType::Unlimited) ?
										  LimiterModeType::Unlimited :
										  LimiterModeType::Nominal);
		}
	})
DEFINE_HOTKEY("ToggleTurbo", TRANSLATE_NOOP("Hotkeys", "System"),
	TRANSLATE_NOOP("Hotkeys", "Toggle Turbo / Fast Forward"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			VMManager::SetLimiterMode(
				(EmuConfig.LimiterMode != LimiterModeType::Turbo) ? LimiterModeType::Turbo : LimiterModeType::Nominal);
		}
	})
DEFINE_HOTKEY("ToggleSlowMotion", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Slow Motion"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			VMManager::SetLimiterMode(
				(EmuConfig.LimiterMode != LimiterModeType::Slomo) ? LimiterModeType::Slomo : LimiterModeType::Nominal);
		}
	})
DEFINE_HOTKEY("HoldTurbo", TRANSLATE_NOOP("Hotkeys", "System"),
	TRANSLATE_NOOP("Hotkeys", "Turbo / Fast Forward (Hold)"), [](s32 pressed) {
		if (!VMManager::HasValidVM())
			return;
		if (pressed > 0 && !s_limiter_mode_prior_to_hold_interaction.has_value())
		{
			s_limiter_mode_prior_to_hold_interaction = VMManager::GetLimiterMode();
			VMManager::SetLimiterMode((s_limiter_mode_prior_to_hold_interaction.value() != LimiterModeType::Turbo) ?
										  LimiterModeType::Turbo :
										  LimiterModeType::Nominal);
		}
		else if (pressed >= 0 && s_limiter_mode_prior_to_hold_interaction.has_value())
		{
			VMManager::SetLimiterMode(s_limiter_mode_prior_to_hold_interaction.value());
			s_limiter_mode_prior_to_hold_interaction.reset();
		}
	})
DEFINE_HOTKEY("IncreaseSpeed", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Increase Target Speed"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustTargetSpeed(0.1);
	})
DEFINE_HOTKEY("DecreaseSpeed", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Decrease Target Speed"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustTargetSpeed(-0.1);
	})
DEFINE_HOTKEY("IncreaseVolume", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Increase Volume"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustVolume(-1, 5);
	})
DEFINE_HOTKEY("DecreaseVolume", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Decrease Volume"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustVolume(-1, -5);
	})
DEFINE_HOTKEY("Mute", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Mute"), [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyAdjustVolume((SPU2::GetOutputVolume() == 0) ? EmuConfig.SPU2.FinalVolume : 0, 0);
})
DEFINE_HOTKEY(
	"FrameAdvance", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Frame Advance"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			VMManager::FrameAdvance(1);
	})
DEFINE_HOTKEY("ShutdownVM", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Shut Down Virtual Machine"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			Host::RequestVMShutdown(true, true, EmuConfig.SaveStateOnShutdown);
	})
DEFINE_HOTKEY("ResetVM", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Reset Virtual Machine"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			VMManager::Reset();
	})
DEFINE_HOTKEY("InputRecToggleMode", TRANSLATE_NOOP("Hotkeys", "System"),
	TRANSLATE_NOOP("Hotkeys", "Toggle Input Recording Mode"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			g_InputRecording.getControls().toggleRecordMode();
	})

DEFINE_HOTKEY("PreviousSaveStateSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Select Previous Save Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyCycleSaveSlot(-1);
	})
DEFINE_HOTKEY("NextSaveStateSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Select Next Save Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyCycleSaveSlot(1);
	})
DEFINE_HOTKEY("SaveStateToSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Save State To Selected Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			VMManager::SaveStateToSlot(s_current_save_slot);
	})
DEFINE_HOTKEY("LoadStateFromSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Load State From Selected Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyLoadStateSlot(s_current_save_slot);
	})

#define DEFINE_HOTKEY_SAVESTATE_X(slotnum, title) \
	DEFINE_HOTKEY("SaveStateToSlot" #slotnum, "Save States", title, [](s32 pressed) { \
		if (!pressed) \
			HotkeySaveStateSlot(slotnum); \
	})
#define DEFINE_HOTKEY_LOADSTATE_X(slotnum, title) \
	DEFINE_HOTKEY("LoadStateFromSlot" #slotnum, "Save States", title, [](s32 pressed) { \
		if (!pressed) \
			HotkeyLoadStateSlot(slotnum); \
	})
DEFINE_HOTKEY_SAVESTATE_X(1, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 1"))
DEFINE_HOTKEY_LOADSTATE_X(1, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 1"))
DEFINE_HOTKEY_SAVESTATE_X(2, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 2"))
DEFINE_HOTKEY_LOADSTATE_X(2, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 2"))
DEFINE_HOTKEY_SAVESTATE_X(3, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 3"))
DEFINE_HOTKEY_LOADSTATE_X(3, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 3"))
DEFINE_HOTKEY_SAVESTATE_X(4, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 4"))
DEFINE_HOTKEY_LOADSTATE_X(4, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 4"))
DEFINE_HOTKEY_SAVESTATE_X(5, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 5"))
DEFINE_HOTKEY_LOADSTATE_X(5, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 5"))
DEFINE_HOTKEY_SAVESTATE_X(6, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 6"))
DEFINE_HOTKEY_LOADSTATE_X(6, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 6"))
DEFINE_HOTKEY_SAVESTATE_X(7, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 7"))
DEFINE_HOTKEY_LOADSTATE_X(7, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 7"))
DEFINE_HOTKEY_SAVESTATE_X(8, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 8"))
DEFINE_HOTKEY_LOADSTATE_X(8, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 8"))
DEFINE_HOTKEY_SAVESTATE_X(9, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 9"))
DEFINE_HOTKEY_LOADSTATE_X(9, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 9"))
DEFINE_HOTKEY_SAVESTATE_X(10, TRANSLATE_NOOP("Hotkeys", "Save State To Slot 10"))
DEFINE_HOTKEY_LOADSTATE_X(10, TRANSLATE_NOOP("Hotkeys", "Load State From Slot 10"))
#undef DEFINE_HOTKEY_SAVESTATE_X
#undef DEFINE_HOTKEY_LOADSTATE_X
END_HOTKEY_LIST()
