// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Achievements.h"
#include "GS.h"
#include "Host.h"
#include "IconsFontAwesome.h"
#include "ImGui/FullscreenUI.h"
#include "ImGui/ImGuiOverlays.h"
#include "Input/InputManager.h"
#include "Recording/InputRecording.h"
#include "SPU2/spu2.h"
#include "VMManager.h"
#include "INISettingsInterface.h"
#include "Patch.h"
#include "SIO/Memcard/MemoryCardFile.h"

#include "common/Assertions.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/Timer.h"

static std::optional<LimiterModeType> s_limiter_mode_prior_to_hold_interaction;

void VMManager::Internal::ResetVMHotkeyState()
{
	s_limiter_mode_prior_to_hold_interaction.reset();
}

static void HotkeyAdjustTargetSpeed(double delta)
{
	const double min_speed = Achievements::IsHardcoreModeActive() ? 1.0 : 0.1;
	EmuConfig.EmulationSpeed.NominalScalar = std::max(min_speed, EmuConfig.EmulationSpeed.NominalScalar + delta);
	if (VMManager::GetLimiterMode() != LimiterModeType::Nominal)
		VMManager::SetLimiterMode(LimiterModeType::Nominal);
	else
		VMManager::UpdateTargetSpeed();

	Host::AddIconOSDMessage("SpeedChanged", ICON_FA_CLOCK,
		fmt::format(TRANSLATE_FS("Hotkeys", "Target speed set to {:.0f}%."),
			std::round(EmuConfig.EmulationSpeed.NominalScalar * 100.0)),
		Host::OSD_QUICK_DURATION);
}

static void HotkeyAdjustVolume(const s32 delta)
{
	if (!VMManager::HasValidVM())
		return;

	// Volume-adjusting hotkeys override mute toggle hotkey. EmuConfig.SPU2.OutputMuted overrides hotkeys.
	if (!SPU2::SetOutputMuted(false))
	{
		Host::AddIconOSDMessage("VolumeChanged", ICON_FA_VOLUME_XMARK, TRANSLATE_STR("Hotkeys_Volume", "Volume: Muted in Settings"));
		return;
	}

	const s32 current_volume = static_cast<s32>(SPU2::GetOutputVolume());
	const s32 maximum_volume = static_cast<s32>(Pcsx2Config::SPU2Options::MAX_VOLUME);
	const s32 new_volume = std::clamp(current_volume + delta, 0, maximum_volume);

	if (current_volume != new_volume)
		SPU2::SetOutputVolume(static_cast<u32>(new_volume));

	if (new_volume > 0 && new_volume < maximum_volume)
	{
		Host::AddIconOSDMessage("VolumeChanged", new_volume < 100 ? ICON_FA_VOLUME_LOW : ICON_FA_VOLUME_HIGH,
			fmt::format(TRANSLATE_FS("Hotkeys_Volume", "Volume: {} to {}%"), delta < 0 ? TRANSLATE_STR("Hotkeys_Volume", "Decreased") : TRANSLATE_STR("Hotkeys_Volume", "Increased"), new_volume));
	}
	else
	{
		Host::AddIconOSDMessage("VolumeChanged", delta < 0 ? ICON_FA_VOLUME_OFF : ICON_FA_VOLUME_HIGH,
			fmt::format(TRANSLATE_FS("Hotkeys_Volume", "Volume: {} {}% Reached"), delta < 0 ? TRANSLATE_STR("Hotkeys_Volume", "Minimum") : TRANSLATE_STR("Hotkeys_Volume", "Maximum"), new_volume));
	}
}

static void HotkeyToggleMute()
{
	if (!VMManager::HasValidVM())
		return;

	// Attempt to toggle output muting. EmuConfig.SPU2.OutputMuted overrides hotkeys.
	if (SPU2::SetOutputMuted(!SPU2::IsOutputMuted()))
	{
		if (SPU2::IsOutputMuted())
			Host::AddIconOSDMessage("VolumeChanged", ICON_FA_VOLUME_XMARK, TRANSLATE_STR("Hotkeys_Volume", "Volume: Muted"));
		else
		{
			const u32 current_volume = SPU2::GetOutputVolume();
			Host::AddIconOSDMessage("VolumeChanged", current_volume < 100 ? (current_volume == 0 ? ICON_FA_VOLUME_OFF : ICON_FA_VOLUME_LOW) : ICON_FA_VOLUME_HIGH,
				fmt::format(TRANSLATE_FS("Hotkeys_Volume", "Volume: Unmuted to {}%"), current_volume));
		}
	}
	else
		Host::AddIconOSDMessage("VolumeChanged", ICON_FA_VOLUME_XMARK, TRANSLATE_STR("Hotkeys_Volume", "Volume: Muted in Settings"));
}

static void HotkeyLoadStateSlot(s32 slot)
{
	// Can reapply settings and thus binds, therefore must be deferred.
	Host::RunOnCPUThread([slot]() {
		if (!VMManager::HasSaveStateInSlot(VMManager::GetDiscSerial().c_str(), VMManager::GetDiscCRC(), slot))
		{
			Host::AddIconOSDMessage("LoadStateFromSlot", ICON_FA_TRIANGLE_EXCLAMATION,
				fmt::format(TRANSLATE_FS("Hotkeys", "No save state found in slot {}."), slot), Host::OSD_INFO_DURATION);
			return;
		}

		Error error;
		if (!VMManager::LoadStateFromSlot(slot, false, &error))
			FullscreenUI::ReportStateLoadError(error.GetDescription(), slot, false);
	});
}

static void HotkeySaveStateSlot(s32 slot)
{
	VMManager::SaveStateToSlot(slot, true, [slot](const std::string& error) {
		FullscreenUI::ReportStateSaveError(error, slot);
	});
}

static bool CanPause()
{
	static constexpr const float PAUSE_INTERVAL = 3.0f;
	static Common::Timer::Value s_last_pause_time = 0;

	if (!Achievements::IsHardcoreModeActive() || VMManager::GetState() == VMState::Paused)
		return true;

	const Common::Timer::Value time = Common::Timer::GetCurrentValue();
	const float delta = static_cast<float>(Common::Timer::ConvertValueToSeconds(time - s_last_pause_time));
	if (delta < PAUSE_INTERVAL)
	{
		Host::AddIconOSDMessage("PauseCooldown", ICON_FA_CLOCK,
			TRANSLATE_PLURAL_STR("Hotkeys", "You cannot pause until another %n second(s) have passed.",
				"", static_cast<int>(std::ceil(PAUSE_INTERVAL - delta))),
			Host::OSD_QUICK_DURATION);
		return false;
	}

	Host::RemoveKeyedOSDMessage("PauseCooldown");
	s_last_pause_time = time;

	return true;
}

static bool UseSavestateSelector()
{
	return EmuConfig.UseSavestateSelector;
}

BEGIN_HOTKEY_LIST(g_common_hotkeys)
DEFINE_HOTKEY("ToggleFullscreen", TRANSLATE_NOOP("Hotkeys", "Navigation"), TRANSLATE_NOOP("Hotkeys", "Toggle Fullscreen"),
	[](s32 pressed) {
		if (!pressed)
			Host::SetFullscreen(!Host::IsFullscreen());
	})
DEFINE_HOTKEY("OpenPauseMenu", TRANSLATE_NOOP("Hotkeys", "Navigation"), TRANSLATE_NOOP("Hotkeys", "Open Pause Menu"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM() && CanPause())
			FullscreenUI::OpenPauseMenu();
	})
DEFINE_HOTKEY("OpenAchievementsList", TRANSLATE_NOOP("Hotkeys", "Navigation"),
	TRANSLATE_NOOP("Hotkeys", "Open Achievements List"), [](s32 pressed) {
		if (!pressed && CanPause())
			FullscreenUI::OpenAchievementsWindow();
	})
DEFINE_HOTKEY("OpenLeaderboardsList", TRANSLATE_NOOP("Hotkeys", "Navigation"),
	TRANSLATE_NOOP("Hotkeys", "Open Leaderboards List"), [](s32 pressed) {
		if (!pressed && CanPause())
			FullscreenUI::OpenLeaderboardsWindow();
	})
DEFINE_HOTKEY(
	"TogglePause", TRANSLATE_NOOP("Hotkeys", "Speed"), TRANSLATE_NOOP("Hotkeys", "Toggle Pause"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM() && CanPause())
			VMManager::SetPaused(VMManager::GetState() != VMState::Paused);
	})
DEFINE_HOTKEY(
	"FrameAdvance", TRANSLATE_NOOP("Hotkeys", "Speed"), TRANSLATE_NOOP("Hotkeys", "Frame Advance"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			VMManager::FrameAdvance(1);
	})
DEFINE_HOTKEY("ToggleFrameLimit", TRANSLATE_NOOP("Hotkeys", "Speed"), TRANSLATE_NOOP("Hotkeys", "Toggle Frame Limit"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			VMManager::SetLimiterMode((VMManager::GetLimiterMode() != LimiterModeType::Unlimited) ?
										  LimiterModeType::Unlimited :
										  LimiterModeType::Nominal);
		}
	})
DEFINE_HOTKEY("ToggleTurbo", TRANSLATE_NOOP("Hotkeys", "Speed"),
	TRANSLATE_NOOP("Hotkeys", "Toggle Turbo / Fast Forward"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			VMManager::SetLimiterMode(
				(VMManager::GetLimiterMode() != LimiterModeType::Turbo) ? LimiterModeType::Turbo : LimiterModeType::Nominal);
		}
	})
DEFINE_HOTKEY("HoldTurbo", TRANSLATE_NOOP("Hotkeys", "Speed"),
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
DEFINE_HOTKEY("ToggleSlowMotion", TRANSLATE_NOOP("Hotkeys", "Speed"), TRANSLATE_NOOP("Hotkeys", "Toggle Slow Motion"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			VMManager::SetLimiterMode(
				(VMManager::GetLimiterMode() != LimiterModeType::Slomo) ? LimiterModeType::Slomo : LimiterModeType::Nominal);
		}
	})
DEFINE_HOTKEY("IncreaseSpeed", TRANSLATE_NOOP("Hotkeys", "Speed"), TRANSLATE_NOOP("Hotkeys", "Increase Target Speed"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustTargetSpeed(0.1);
	})
DEFINE_HOTKEY("DecreaseSpeed", TRANSLATE_NOOP("Hotkeys", "Speed"), TRANSLATE_NOOP("Hotkeys", "Decrease Target Speed"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustTargetSpeed(-0.1);
	})
DEFINE_HOTKEY("ShutdownVM", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Shut Down Virtual Machine"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			Host::RequestVMShutdown(true, true, EmuConfig.SaveStateOnShutdown);
	})
DEFINE_HOTKEY("ResetVM", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Reset Virtual Machine"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			VMManager::RequestReset();
	})
DEFINE_HOTKEY("ReloadPatches", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Reload Patches"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			Host::RunOnCPUThread([]() {
				Host::AddKeyedOSDMessage("ReloadPatchHotkey", "Reloading Patches...");
				VMManager::ReloadPatches(true, true, true, true);
			});
		}
	})
DEFINE_HOTKEY("SwapMemCards", TRANSLATE_NOOP("Hotkeys", "System"),
	TRANSLATE_NOOP("Hotkeys", "Swap Memory Cards"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			Host::RunOnCPUThread([]() {
				FileMcd_Swap();
			});
	})
DEFINE_HOTKEY("InputRecToggleMode", TRANSLATE_NOOP("Hotkeys", "System"),
	TRANSLATE_NOOP("Hotkeys", "Toggle Input Recording Mode"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			g_InputRecording.getControls().toggleRecordMode();
	})
DEFINE_HOTKEY("PreviousSaveStateSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Select Previous Save Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			SaveStateSelectorUI::SelectPreviousSlot(UseSavestateSelector());
	})
DEFINE_HOTKEY("NextSaveStateSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Select Next Save Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			SaveStateSelectorUI::SelectNextSlot(UseSavestateSelector());
	})
DEFINE_HOTKEY("SaveStateToSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Save State To Selected Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			SaveStateSelectorUI::SaveCurrentSlot();
	})
DEFINE_HOTKEY("LoadStateFromSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Load State From Selected Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			SaveStateSelectorUI::LoadCurrentSlot();
	})
DEFINE_HOTKEY("LoadBackupStateFromSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Load Backup State From Selected Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			SaveStateSelectorUI::LoadCurrentBackupSlot();
	})
DEFINE_HOTKEY("SaveStateAndSelectNextSlot", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Save State and Select Next Slot"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			SaveStateSelectorUI::SaveCurrentSlot();
			SaveStateSelectorUI::SelectNextSlot(false);
		}
	})
DEFINE_HOTKEY("SelectNextSlotAndSaveState", TRANSLATE_NOOP("Hotkeys", "Save States"),
	TRANSLATE_NOOP("Hotkeys", "Select Next Slot and Save State"), [](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
		{
			SaveStateSelectorUI::SelectNextSlot(false);
			SaveStateSelectorUI::SaveCurrentSlot();
		}
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
DEFINE_HOTKEY("Mute", TRANSLATE_NOOP("Hotkeys", "Audio"), TRANSLATE_NOOP("Hotkeys", "Toggle Mute"), [](s32 pressed) {
	if (!pressed && VMManager::HasValidVM())
		HotkeyToggleMute();
})
DEFINE_HOTKEY("IncreaseVolume", TRANSLATE_NOOP("Hotkeys", "Audio"), TRANSLATE_NOOP("Hotkeys", "Increase Volume"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustVolume(5);
	})
DEFINE_HOTKEY("DecreaseVolume", TRANSLATE_NOOP("Hotkeys", "Audio"), TRANSLATE_NOOP("Hotkeys", "Decrease Volume"),
	[](s32 pressed) {
		if (!pressed && VMManager::HasValidVM())
			HotkeyAdjustVolume(-5);
	})
DEFINE_HOTKEY("ToggleMouseLock", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Mouse Lock"),
	[](s32 pressed) {
		if (!pressed)
			Host::SetMouseLock(!Host::GetBoolSettingValue("EmuCore", "EnableMouseLock"));
	})


DEFINE_HOTKEY("ToggleCheatSlot1", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 1"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot1", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot1", fmt::format("Cheat Slot 1 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot1", fmt::format("Cheat Slot 1 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot2", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 2"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot2", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot2", fmt::format("Cheat Slot 2 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot2", fmt::format("Cheat Slot 2 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot3", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 3"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot3", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot3", fmt::format("Cheat Slot 3 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot3", fmt::format("Cheat Slot 3 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot4", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 4"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot4", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot4", fmt::format("Cheat Slot 4 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot4", fmt::format("Cheat Slot 4 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot5", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 5"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot5", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot5", fmt::format("Cheat Slot 5 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot5", fmt::format("Cheat Slot 5 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot6", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 6"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot6", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot6", fmt::format("Cheat Slot 6 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot6", fmt::format("Cheat Slot 6 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot7", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 7"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot7", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot7", fmt::format("Cheat Slot 7 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot7", fmt::format("Cheat Slot 7 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot8", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 8"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot8", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot8", fmt::format("Cheat Slot 8 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot8", fmt::format("Cheat Slot 8 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot9", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 9"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot9", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot9", fmt::format("Cheat Slot 9 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot9", fmt::format("Cheat Slot 9 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot10", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 10"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot10", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot10", fmt::format("Cheat Slot 10 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot10", fmt::format("Cheat Slot 10 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot11", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 11"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot11", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot11", fmt::format("Cheat Slot 11 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot11", fmt::format("Cheat Slot 11 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot12", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 12"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot12", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot12", fmt::format("Cheat Slot 12 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot12", fmt::format("Cheat Slot 12 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot13", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 13"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot13", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot13", fmt::format("Cheat Slot 13 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot13", fmt::format("Cheat Slot 13 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot14", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 14"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot14", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot14", fmt::format("Cheat Slot 14 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot14", fmt::format("Cheat Slot 14 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot15", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 15"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot15", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot15", fmt::format("Cheat Slot 15 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot15", fmt::format("Cheat Slot 15 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

DEFINE_HOTKEY("ToggleCheatSlot16", TRANSLATE_NOOP("Hotkeys", "System"), TRANSLATE_NOOP("Hotkeys", "Toggle Cheat Slot 16"), [](s32 pressed) {
        if (!pressed && VMManager::HasValidVM())
        {
                Host::RunOnCPUThread([]() {
                        SettingsInterface* si = Host::Internal::GetGameSettingsLayer();
                        if (!si)
                                return;
                        const std::string cheat_name = Host::GetStringSettingValue("CheatHotkeys", "CheatToggleSlot16", "");
                        if (cheat_name.empty())
                                return;
                        std::vector<std::string> enabled = si->GetStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY);
                        auto it = std::find(enabled.begin(), enabled.end(), cheat_name);
                        if (it != enabled.end())
                        {
                                si->RemoveFromStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot16", fmt::format("Cheat Slot 16 ({}): OFF", cheat_name));
                        }
                        else
                        {
                                si->AddToStringList(Patch::CHEATS_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, cheat_name.c_str());
                                Host::AddKeyedOSDMessage("CheatSlot16", fmt::format("Cheat Slot 16 ({}): ON", cheat_name));
                        }
                        si->Save();
                        VMManager::ReloadPatches(true, true, true, true);
                });
        }
})

END_HOTKEY_LIST()
