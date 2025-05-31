// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/CocoaTools.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/SmallString.h"
#include "Config.h"
#include "GS.h"
#include "CDVD/CDVDcommon.h"
#include "Host.h"
#include "Host/AudioStream.h"
#include "SIO/Memcard/MemoryCardFile.h"
#include "SIO/Pad/Pad.h"
#include "USB/USB.h"

#include "fmt/format.h"
#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <KnownFolders.h>
#include <ShlObj.h>
#endif

// This macro is actually useful for about any and every possible application of C++ equality operators.
// Stuck here because of legacy code, new code shouldn't rely on it, it's difficult to read.
#define OpEqu(field) (field == right.field)

// Default EE/VU control registers have exceptions off, DaZ/FTZ, and the rounding mode set to Chop/Zero.
static constexpr FPControlRegister DEFAULT_FPU_FP_CONTROL_REGISTER = FPControlRegister::GetDefault()
																		 .DisableExceptions()
																		 .SetDenormalsAreZero(true)
																		 .SetFlushToZero(true)
																		 .SetRoundMode(FPRoundMode::ChopZero);
static constexpr FPControlRegister DEFAULT_VU_FP_CONTROL_REGISTER = FPControlRegister::GetDefault()
																		.DisableExceptions()
																		.SetDenormalsAreZero(true)
																		.SetFlushToZero(true)
																		.SetRoundMode(FPRoundMode::ChopZero);

Pcsx2Config EmuConfig;

const char* SettingInfo::StringDefaultValue() const
{
	return default_value ? default_value : "";
}

bool SettingInfo::BooleanDefaultValue() const
{
	return default_value ? StringUtil::FromChars<bool>(default_value).value_or(false) : false;
}

s32 SettingInfo::IntegerDefaultValue() const
{
	return default_value ? StringUtil::FromChars<s32>(default_value).value_or(0) : 0;
}

s32 SettingInfo::IntegerMinValue() const
{
	static constexpr s32 fallback_value = std::numeric_limits<s32>::min();
	return min_value ? StringUtil::FromChars<s32>(min_value).value_or(fallback_value) : fallback_value;
}

s32 SettingInfo::IntegerMaxValue() const
{
	static constexpr s32 fallback_value = std::numeric_limits<s32>::max();
	return max_value ? StringUtil::FromChars<s32>(max_value).value_or(fallback_value) : fallback_value;
}

s32 SettingInfo::IntegerStepValue() const
{
	static constexpr s32 fallback_value = 1;
	return step_value ? StringUtil::FromChars<s32>(step_value).value_or(fallback_value) : fallback_value;
}

float SettingInfo::FloatDefaultValue() const
{
	return default_value ? StringUtil::FromChars<float>(default_value).value_or(0.0f) : 0.0f;
}

float SettingInfo::FloatMinValue() const
{
	static constexpr float fallback_value = std::numeric_limits<float>::min();
	return min_value ? StringUtil::FromChars<float>(min_value).value_or(fallback_value) : fallback_value;
}

float SettingInfo::FloatMaxValue() const
{
	static constexpr float fallback_value = std::numeric_limits<float>::max();
	return max_value ? StringUtil::FromChars<float>(max_value).value_or(fallback_value) : fallback_value;
}

float SettingInfo::FloatStepValue() const
{
	static constexpr float fallback_value = 0.1f;
	return step_value ? StringUtil::FromChars<float>(step_value).value_or(fallback_value) : fallback_value;
}

void SettingInfo::SetDefaultValue(SettingsInterface* si, const char* section, const char* key) const
{
	switch (type)
	{
		case SettingInfo::Type::Boolean:
			si->SetBoolValue(section, key, BooleanDefaultValue());
			break;
		case SettingInfo::Type::Integer:
		case SettingInfo::Type::IntegerList:
			si->SetIntValue(section, key, IntegerDefaultValue());
			break;
		case SettingInfo::Type::Float:
			si->SetFloatValue(section, key, FloatDefaultValue());
			break;
		case SettingInfo::Type::String:
		case SettingInfo::Type::StringList:
		case SettingInfo::Type::Path:
			si->SetStringValue(section, key, StringDefaultValue());
			break;
		default:
			break;
	}
}

void SettingInfo::CopyValue(SettingsInterface* dest_si, const SettingsInterface& src_si,
	const char* section, const char* key) const
{
	switch (type)
	{
		case SettingInfo::Type::Boolean:
			dest_si->CopyBoolValue(src_si, section, key);
			break;
		case SettingInfo::Type::Integer:
		case SettingInfo::Type::IntegerList:
			dest_si->CopyIntValue(src_si, section, key);
			break;
		case SettingInfo::Type::Float:
			dest_si->CopyFloatValue(src_si, section, key);
			break;
		case SettingInfo::Type::String:
		case SettingInfo::Type::StringList:
		case SettingInfo::Type::Path:
			dest_si->CopyStringValue(src_si, section, key);
			break;
		default:
			break;
	}
}

namespace EmuFolders
{
	std::string AppRoot;
	std::string DataRoot;
	std::string Settings;
	std::string DebuggerLayouts;
	std::string DebuggerSettings;
	std::string Bios;
	std::string Snapshots;
	std::string Savestates;
	std::string MemoryCards;
	std::string Logs;
	std::string Cheats;
	std::string Patches;
	std::string Resources;
	std::string UserResources;
	std::string Cache;
	std::string Covers;
	std::string GameSettings;
	std::string Textures;
	std::string InputProfiles;
	std::string Videos;

	static bool ShouldUsePortableMode();
	static std::string GetPortableModePath();
} // namespace EmuFolders

TraceLogsEE::TraceLogsEE()
{
	bitset = 0;
}

bool TraceLogsEE::operator==(const TraceLogsEE& right) const
{
	return OpEqu(bitset);
}

bool TraceLogsEE::operator!=(const TraceLogsEE& right) const
{
	return !this->operator==(right);
}

TraceLogsIOP::TraceLogsIOP()
{
	bitset = 0;
}

bool TraceLogsIOP::operator==(const TraceLogsIOP& right) const
{
	return OpEqu(bitset);
}

bool TraceLogsIOP::operator!=(const TraceLogsIOP& right) const
{
	return !this->operator==(right);
}

TraceLogsMISC::TraceLogsMISC()
{
	bitset = 0;
}

bool TraceLogsMISC::operator==(const TraceLogsMISC& right) const
{
	return OpEqu(bitset);
}

bool TraceLogsMISC::operator!=(const TraceLogsMISC& right) const
{
	return !this->operator==(right);
}

TraceLogFilters::TraceLogFilters()
{
	Enabled = false;
}

void TraceLogFilters::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/TraceLog");

	SettingsWrapEntry(Enabled);

	SettingsWrapBitBool(EE.bios);
	SettingsWrapBitBool(EE.memory);
	SettingsWrapBitBool(EE.giftag);
	SettingsWrapBitBool(EE.vifcode);
	SettingsWrapBitBool(EE.mskpath3);
	SettingsWrapBitBool(EE.r5900);
	SettingsWrapBitBool(EE.cop0);
	SettingsWrapBitBool(EE.cop1);
	SettingsWrapBitBool(EE.cop2);
	SettingsWrapBitBool(EE.cache);
	SettingsWrapBitBool(EE.knownhw);
	SettingsWrapBitBool(EE.unknownhw);
	SettingsWrapBitBool(EE.dmahw);
	SettingsWrapBitBool(EE.ipu);
	SettingsWrapBitBool(EE.dmac);
	SettingsWrapBitBool(EE.counters);
	SettingsWrapBitBool(EE.spr);
	SettingsWrapBitBool(EE.vif);
	SettingsWrapBitBool(EE.gif);

	SettingsWrapBitBool(IOP.bios);
	SettingsWrapBitBool(IOP.memcards);
	SettingsWrapBitBool(IOP.pad);
	SettingsWrapBitBool(IOP.r3000a);
	SettingsWrapBitBool(IOP.cop2);
	SettingsWrapBitBool(IOP.memory);
	SettingsWrapBitBool(IOP.knownhw);
	SettingsWrapBitBool(IOP.unknownhw);
	SettingsWrapBitBool(IOP.dmahw);
	SettingsWrapBitBool(IOP.dmac);
	SettingsWrapBitBool(IOP.counters);
	SettingsWrapBitBool(IOP.cdvd);
	SettingsWrapBitBool(IOP.mdec);

	SettingsWrapBitBool(MISC.sif);
}

void TraceLogFilters::SyncToConfig() const
{
	auto& ee = TraceLogging.EE;
	ee.Bios.Enabled = EE.bios;
	ee.Memory.Enabled = EE.memory;
	ee.GIFtag.Enabled = EE.giftag;
	ee.VIFcode.Enabled = EE.vifcode;
	ee.MSKPATH3.Enabled = EE.mskpath3;
	ee.R5900.Enabled = EE.r5900;
	ee.COP0.Enabled = EE.cop0;
	ee.COP1.Enabled = EE.cop1;
	ee.COP2.Enabled = EE.cop2;
	ee.KnownHw.Enabled = EE.knownhw;
	ee.UnknownHw.Enabled = EE.unknownhw;
	ee.DMAhw.Enabled = EE.dmahw;
	ee.IPU.Enabled = EE.ipu;
	ee.DMAC.Enabled = EE.dmac;
	ee.Counters.Enabled = EE.counters;
	ee.SPR.Enabled = EE.spr;
	ee.VIF.Enabled = EE.vif;
	ee.GIF.Enabled = EE.gif;

	auto& iop = TraceLogging.IOP;
	iop.Bios.Enabled = IOP.bios;
	iop.Memcards.Enabled = IOP.memcards;
	iop.PAD.Enabled = IOP.pad;
	iop.R3000A.Enabled = IOP.r3000a;
	iop.COP2.Enabled = IOP.cop2;
	iop.Memory.Enabled = IOP.memory;
	iop.KnownHw.Enabled = IOP.knownhw;
	iop.UnknownHw.Enabled = IOP.unknownhw;
	iop.DMAhw.Enabled = IOP.dmahw;
	iop.DMAC.Enabled = IOP.dmac;
	iop.Counters.Enabled = IOP.counters;
	iop.CDVD.Enabled = IOP.cdvd;
	iop.MDEC.Enabled = IOP.mdec;

	TraceLogging.SIF.Enabled = MISC.sif;

	EmuConfig.Trace.Enabled = Enabled;
}

bool TraceLogFilters::operator==(const TraceLogFilters& right) const
{
	return OpEqu(Enabled) && OpEqu(EE) && OpEqu(IOP) && OpEqu(MISC);
}

bool TraceLogFilters::operator!=(const TraceLogFilters& right) const
{
	return !this->operator==(right);
}

static constexpr const char* s_speed_hack_names[] = {
	"mvuFlag",
	"instantVU1",
	"mtvu",
	"eeCycleRate",
};

const char* Pcsx2Config::SpeedhackOptions::GetSpeedHackName(SpeedHack id)
{
	pxAssert(static_cast<u32>(id) < std::size(s_speed_hack_names));
	return s_speed_hack_names[static_cast<u32>(id)];
}

std::optional<SpeedHack> Pcsx2Config::SpeedhackOptions::ParseSpeedHackName(const std::string_view name)
{
	for (u32 i = 0; i < std::size(s_speed_hack_names); i++)
	{
		if (name == s_speed_hack_names[i])
			return static_cast<SpeedHack>(i);
	}

	return std::nullopt;
}

void Pcsx2Config::SpeedhackOptions::Set(SpeedHack id, int value)
{
	pxAssert(static_cast<u32>(id) < std::size(s_speed_hack_names));

	switch (id)
	{
		case SpeedHack::MVUFlag:
			vuFlagHack = (value != 0);
			break;
		case SpeedHack::InstantVU1:
			vu1Instant = (value != 0);
			break;
		case SpeedHack::MTVU:
			vuThread = (value != 0);
			break;
		case SpeedHack::EECycleRate:
			EECycleRate = static_cast<int>(std::clamp<int>(value, MIN_EE_CYCLE_RATE, MAX_EE_CYCLE_RATE));
			break;
			jNO_DEFAULT
	}
}

bool Pcsx2Config::SpeedhackOptions::operator==(const SpeedhackOptions& right) const
{
	return OpEqu(bitset) && OpEqu(EECycleRate) && OpEqu(EECycleSkip);
}

bool Pcsx2Config::SpeedhackOptions::operator!=(const SpeedhackOptions& right) const
{
	return !operator==(right);
}

Pcsx2Config::SpeedhackOptions::SpeedhackOptions()
{
	DisableAll();

	// Set recommended speedhacks to enabled by default. They'll still be off globally on resets.
	WaitLoop = true;
	IntcStat = true;
	vuFlagHack = true;
	vu1Instant = true;
}

Pcsx2Config::SpeedhackOptions& Pcsx2Config::SpeedhackOptions::DisableAll()
{
	bitset = 0;
	EECycleRate = 0;
	EECycleSkip = 0;

	return *this;
}

void Pcsx2Config::SpeedhackOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/Speedhacks");

	SettingsWrapBitfield(EECycleRate);
	SettingsWrapBitfield(EECycleSkip);
	SettingsWrapBitBool(fastCDVD);
	SettingsWrapBitBool(IntcStat);
	SettingsWrapBitBool(WaitLoop);
	SettingsWrapBitBool(vuFlagHack);
	SettingsWrapBitBool(vuThread);
	SettingsWrapBitBool(vu1Instant);

	EECycleRate = std::clamp(EECycleRate, MIN_EE_CYCLE_RATE, MAX_EE_CYCLE_RATE);
	EECycleSkip = std::min(EECycleSkip, MAX_EE_CYCLE_SKIP);
}

Pcsx2Config::ProfilerOptions::ProfilerOptions()
	: bitset(0xfffffffe)
{
}

void Pcsx2Config::ProfilerOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/Profiler");

	SettingsWrapBitBool(Enabled);
	SettingsWrapBitBool(RecBlocks_EE);
	SettingsWrapBitBool(RecBlocks_IOP);
	SettingsWrapBitBool(RecBlocks_VU0);
	SettingsWrapBitBool(RecBlocks_VU1);
}

bool Pcsx2Config::ProfilerOptions::operator!=(const ProfilerOptions& right) const
{
	return !OpEqu(bitset);
}

bool Pcsx2Config::ProfilerOptions::operator==(const ProfilerOptions& right) const
{
	return OpEqu(bitset);
}

Pcsx2Config::RecompilerOptions::RecompilerOptions()
{
	bitset = 0;

	//StackFrameChecks	= false;
	//PreBlockCheckEE	= false;

	// All recs are enabled by default.

	EnableEE = true;
	EnableEECache = false;
	EnableIOP = true;
	EnableVU0 = true;
	EnableVU1 = true;
	EnableFastmem = true;
	PauseOnTLBMiss = false;

	// vu and fpu clamping default to standard overflow.
	vu0Overflow = true;
	//vu0ExtraOverflow = false;
	//vu0SignOverflow = false;
	//vu0Underflow = false;
	vu1Overflow = true;
	//vu1ExtraOverflow = false;
	//vu1SignOverflow = false;
	//vu1Underflow = false;

	fpuOverflow = true;
	//fpuExtraOverflow = false;
	//fpuFullMode = false;
}

void Pcsx2Config::RecompilerOptions::ApplySanityCheck()
{
	bool fpuIsRight = true;

	if (fpuExtraOverflow)
		fpuIsRight = fpuOverflow;

	if (fpuFullMode)
		fpuIsRight = fpuOverflow && fpuExtraOverflow;

	if (!fpuIsRight)
	{
		// Values are wonky; assume the defaults.
		fpuOverflow = RecompilerOptions().fpuOverflow;
		fpuExtraOverflow = RecompilerOptions().fpuExtraOverflow;
		fpuFullMode = RecompilerOptions().fpuFullMode;
	}

	bool vuIsOk = true;

	if (vu0ExtraOverflow)
		vuIsOk = vuIsOk && vu0Overflow;
	if (vu0SignOverflow)
		vuIsOk = vuIsOk && vu0ExtraOverflow;

	if (!vuIsOk)
	{
		// Values are wonky; assume the defaults.
		vu0Overflow = RecompilerOptions().vu0Overflow;
		vu0ExtraOverflow = RecompilerOptions().vu0ExtraOverflow;
		vu0SignOverflow = RecompilerOptions().vu0SignOverflow;
		vu0Underflow = RecompilerOptions().vu0Underflow;
	}

	vuIsOk = true;

	if (vu1ExtraOverflow)
		vuIsOk = vuIsOk && vu1Overflow;
	if (vu1SignOverflow)
		vuIsOk = vuIsOk && vu1ExtraOverflow;

	if (!vuIsOk)
	{
		// Values are wonky; assume the defaults.
		vu1Overflow = RecompilerOptions().vu1Overflow;
		vu1ExtraOverflow = RecompilerOptions().vu1ExtraOverflow;
		vu1SignOverflow = RecompilerOptions().vu1SignOverflow;
		vu1Underflow = RecompilerOptions().vu1Underflow;
	}
}

void Pcsx2Config::RecompilerOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/CPU/Recompiler");

	SettingsWrapBitBool(EnableEE);
	SettingsWrapBitBool(EnableIOP);
	SettingsWrapBitBool(EnableEECache);
	SettingsWrapBitBool(EnableVU0);
	SettingsWrapBitBool(EnableVU1);
	SettingsWrapBitBool(EnableFastmem);
	SettingsWrapBitBool(PauseOnTLBMiss);

	SettingsWrapBitBool(vu0Overflow);
	SettingsWrapBitBool(vu0ExtraOverflow);
	SettingsWrapBitBool(vu0SignOverflow);
	SettingsWrapBitBool(vu0Underflow);
	SettingsWrapBitBool(vu1Overflow);
	SettingsWrapBitBool(vu1ExtraOverflow);
	SettingsWrapBitBool(vu1SignOverflow);
	SettingsWrapBitBool(vu1Underflow);

	SettingsWrapBitBool(fpuOverflow);
	SettingsWrapBitBool(fpuExtraOverflow);
	SettingsWrapBitBool(fpuFullMode);
}

u32 Pcsx2Config::RecompilerOptions::GetEEClampMode() const
{
	return fpuFullMode ? 3 : (fpuExtraOverflow ? 2 : (fpuOverflow ? 1 : 0));
}

void Pcsx2Config::RecompilerOptions::SetEEClampMode(u32 value)
{
	fpuOverflow = (value >= 1);
	fpuExtraOverflow = (value >= 2);
	fpuFullMode = (value >= 3);
}

u32 Pcsx2Config::RecompilerOptions::GetVUClampMode() const
{
	return vu0SignOverflow ? 3 : (vu0ExtraOverflow ? 2 : (vu0Overflow ? 1 : 0));
}

bool Pcsx2Config::RecompilerOptions::operator!=(const RecompilerOptions& right) const
{
	return !OpEqu(bitset);
}

bool Pcsx2Config::RecompilerOptions::operator==(const RecompilerOptions& right) const
{
	return OpEqu(bitset);
}

bool Pcsx2Config::CpuOptions::CpusChanged(const CpuOptions& right) const
{
	return (Recompiler.EnableEE != right.Recompiler.EnableEE ||
			Recompiler.EnableIOP != right.Recompiler.EnableIOP ||
			Recompiler.EnableVU0 != right.Recompiler.EnableVU0 ||
			Recompiler.EnableVU1 != right.Recompiler.EnableVU1);
}

bool Pcsx2Config::CpuOptions::operator!=(const CpuOptions& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::CpuOptions::operator==(const CpuOptions& right) const
{
	return OpEqu(FPUFPCR) && OpEqu(FPUDivFPCR) && OpEqu(VU0FPCR) && OpEqu(VU1FPCR) && OpEqu(Recompiler);
}

Pcsx2Config::CpuOptions::CpuOptions()
{
	FPUFPCR = DEFAULT_FPU_FP_CONTROL_REGISTER;

	// Rounding defaults to nearest to match old behavior.
	// TODO: Make it default to the same as the rest of the FPU operations, at some point.
	FPUDivFPCR = FPControlRegister(DEFAULT_FPU_FP_CONTROL_REGISTER).SetRoundMode(FPRoundMode::Nearest);

	VU0FPCR = DEFAULT_VU_FP_CONTROL_REGISTER;
	VU1FPCR = DEFAULT_VU_FP_CONTROL_REGISTER;
	ExtraMemory = false;
}

void Pcsx2Config::CpuOptions::ApplySanityCheck()
{
	Recompiler.ApplySanityCheck();
}

void Pcsx2Config::CpuOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/CPU");

	const auto read_fpcr = [&wrap, &CURRENT_SETTINGS_SECTION](FPControlRegister& fpcr, std::string_view prefix) {
		fpcr.SetDenormalsAreZero(wrap.EntryBitBool(CURRENT_SETTINGS_SECTION, TinyString::from_format("{}.DenormalsAreZero", prefix),
			fpcr.GetDenormalsAreZero(), fpcr.GetDenormalsAreZero()));
		fpcr.SetFlushToZero(wrap.EntryBitBool(CURRENT_SETTINGS_SECTION, TinyString::from_format("{}.DenormalsAreZero", prefix),
			fpcr.GetFlushToZero(), fpcr.GetFlushToZero()));

		uint round_mode = static_cast<uint>(fpcr.GetRoundMode());
		wrap.Entry(CURRENT_SETTINGS_SECTION, TinyString::from_format("{}.Roundmode", prefix), round_mode, round_mode);
		round_mode = std::min(round_mode, static_cast<uint>(FPRoundMode::MaxCount) - 1u);
		fpcr.SetRoundMode(static_cast<FPRoundMode>(round_mode));
	};

	read_fpcr(FPUFPCR, "FPU");
	read_fpcr(FPUDivFPCR, "FPUDiv");
	read_fpcr(VU0FPCR, "VU0");
	read_fpcr(VU1FPCR, "VU1");

	SettingsWrapBitBool(ExtraMemory);

	Recompiler.LoadSave(wrap);
}

const char* Pcsx2Config::GSOptions::AspectRatioNames[(size_t)AspectRatioType::MaxCount + 1] = {
	"Stretch",
	"Auto 4:3/3:2",
	"4:3",
	"16:9",
	"10:7",
	nullptr};

const char* Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames[(size_t)FMVAspectRatioSwitchType::MaxCount + 1] = {
	"Off",
	"Auto 4:3/3:2",
	"4:3",
	"16:9",
	"10:7",
	nullptr};

const char* Pcsx2Config::GSOptions::BlendingLevelNames[] = {
	"Minimum",
	"Basic",
	"Medium",
	"High",
	"Full",
	"Maximum",
	nullptr};

const char* Pcsx2Config::GSOptions::CaptureContainers[] = {
	"mp4",
	"mkv",
	"mov",
	"avi",
	"wav",
	"mp3",
	nullptr};
const char* Pcsx2Config::GSOptions::DEFAULT_CAPTURE_CONTAINER = "mp4";

const char* Pcsx2Config::GSOptions::GetRendererName(GSRendererType type)
{
	switch (type)
	{
			// clang-format off
		case GSRendererType::Auto:  return "Auto";
		case GSRendererType::DX11:  return "Direct3D 11";
		case GSRendererType::DX12:  return "Direct3D 12";
		case GSRendererType::Metal: return "Metal";
		case GSRendererType::OGL:   return "OpenGL";
		case GSRendererType::VK:    return "Vulkan";
		case GSRendererType::SW:    return "Software";
		case GSRendererType::Null:  return "Null";
		default:                    return "";
			// clang-format on
	}
}

std::optional<bool> Pcsx2Config::GSOptions::TriStateToOptionalBoolean(int value)
{
	return (value < 0) ? std::optional<bool>(std::nullopt) : std::optional<bool>((value != 0));
}

Pcsx2Config::GSOptions::GSOptions()
{
	bitset = 0;

	PCRTCAntiBlur = true;
	DisableInterlaceOffset = false;
	PCRTCOffsets = false;
	PCRTCOverscan = false;
	IntegerScaling = false;
	LinearPresent = GSPostBilinearMode::BilinearSmooth;
	UseDebugDevice = false;
	UseBlitSwapChain = false;
	DisableShaderCache = false;
	DisableFramebufferFetch = false;
	DisableVertexShaderExpand = false;
	SkipDuplicateFrames = false;
	OsdMessagesPos = OsdOverlayPos::TopLeft;
	OsdPerformancePos = OsdOverlayPos::TopRight;
	OsdShowSpeed = false;
	OsdShowFPS = false;
	OsdShowVPS = false;
	OsdShowCPU = false;
	OsdShowGPU = false;
	OsdShowResolution = false;
	OsdShowGSStats = false;
	OsdShowIndicators = true;
	OsdShowSettings = false;
	OsdShowInputs = false;
	OsdShowFrameTimes = false;
	OsdShowVersion = false;
	OsdShowHardwareInfo = false;
	OsdShowVideoCapture = true;
	OsdShowInputRec = true;

	HWDownloadMode = GSHardwareDownloadMode::Enabled;
	HWSpinGPUForReadbacks = false;
	HWSpinCPUForReadbacks = false;
	GPUPaletteConversion = false;
	AutoFlushSW = true;
	PreloadFrameWithGSData = false;
	Mipmap = true;
	HWMipmap = true;

	ManualUserHacks = false;
	UserHacks_AlignSpriteX = false;
	UserHacks_AutoFlush = GSHWAutoFlushLevel::Disabled;
	UserHacks_CPUFBConversion = false;
	UserHacks_ReadTCOnClose = false;
	UserHacks_DisableDepthSupport = false;
	UserHacks_DisablePartialInvalidation = false;
	UserHacks_DisableSafeFeatures = false;
	UserHacks_DisableRenderFixes = false;
	UserHacks_MergePPSprite = false;
	UserHacks_ForceEvenSpritePosition = false;
	UserHacks_BilinearHack = GSBilinearDirtyMode::Automatic;
	UserHacks_NativePaletteDraw = false;

	DumpReplaceableTextures = false;
	DumpReplaceableMipmaps = false;
	DumpTexturesWithFMVActive = false;
	DumpDirectTextures = true;
	DumpPaletteTextures = true;
	LoadTextureReplacements = false;
	LoadTextureReplacementsAsync = true;
	PrecacheTextureReplacements = false;

	EnableVideoCapture = true;
	EnableVideoCaptureParameters = false;
	EnableAudioCapture = true;
	EnableAudioCaptureParameters = false;
}

bool Pcsx2Config::GSOptions::operator==(const GSOptions& right) const
{
	return (
		OpEqu(SynchronousMTGS) &&
		OpEqu(VsyncQueueSize) &&

		OpEqu(FramerateNTSC) &&
		OpEqu(FrameratePAL) &&

		OpEqu(AspectRatio) &&
		OpEqu(FMVAspectRatioSwitch) &&

		OptionsAreEqual(right));
}

bool Pcsx2Config::GSOptions::OptionsAreEqual(const GSOptions& right) const
{
	return (
		OpEqu(bitset) &&

		OpEqu(InterlaceMode) &&
		OpEqu(LinearPresent) &&

		OpEqu(StretchY) &&
		OpEqu(Crop[0]) &&
		OpEqu(Crop[1]) &&
		OpEqu(Crop[2]) &&
		OpEqu(Crop[3]) &&

		OpEqu(OsdScale) &&
		OpEqu(OsdMessagesPos) &&
		OpEqu(OsdPerformancePos) &&

		OpEqu(Renderer) &&
		OpEqu(UpscaleMultiplier) &&

		OpEqu(AccurateBlendingUnit) &&
		OpEqu(TextureFiltering) &&
		OpEqu(TexturePreloading) &&
		OpEqu(GSDumpCompression) &&
		OpEqu(HWDownloadMode) &&
		OpEqu(CASMode) &&
		OpEqu(Dithering) &&
		OpEqu(MaxAnisotropy) &&
		OpEqu(SWExtraThreads) &&
		OpEqu(SWExtraThreadsHeight) &&
		OpEqu(TriFilter) &&
		OpEqu(TVShader) &&
		OpEqu(GetSkipCountFunctionId) &&
		OpEqu(BeforeDrawFunctionId) &&
		OpEqu(MoveHandlerFunctionId) &&
		OpEqu(SkipDrawEnd) &&
		OpEqu(SkipDrawStart) &&

		OpEqu(UserHacks_AutoFlush) &&
		OpEqu(UserHacks_HalfPixelOffset) &&
		OpEqu(UserHacks_RoundSprite) &&
		OpEqu(UserHacks_NativeScaling) &&
		OpEqu(UserHacks_TCOffsetX) &&
		OpEqu(UserHacks_TCOffsetY) &&
		OpEqu(UserHacks_CPUSpriteRenderBW) &&
		OpEqu(UserHacks_CPUSpriteRenderLevel) &&
		OpEqu(UserHacks_CPUCLUTRender) &&
		OpEqu(UserHacks_GPUTargetCLUTMode) &&
		OpEqu(UserHacks_TextureInsideRt) &&
		OpEqu(UserHacks_BilinearHack) &&
		OpEqu(OverrideTextureBarriers) &&

		OpEqu(CAS_Sharpness) &&
		OpEqu(ShadeBoost_Brightness) &&
		OpEqu(ShadeBoost_Contrast) &&
		OpEqu(ShadeBoost_Saturation) &&
		OpEqu(PNGCompressionLevel) &&
		OpEqu(SaveDrawStart) &&
		OpEqu(SaveDrawCount) &&
		OpEqu(SaveDrawBy) &&
		OpEqu(SaveFrameStart) &&
		OpEqu(SaveFrameCount) &&
		OpEqu(SaveFrameBy) &&

		OpEqu(ExclusiveFullscreenControl) &&
		OpEqu(ScreenshotSize) &&
		OpEqu(ScreenshotFormat) &&
		OpEqu(ScreenshotQuality) &&

		OpEqu(CaptureContainer) &&
		OpEqu(VideoCaptureCodec) &&
		OpEqu(VideoCaptureFormat) &&
		OpEqu(VideoCaptureParameters) &&
		OpEqu(AudioCaptureCodec) &&
		OpEqu(AudioCaptureParameters) &&
		OpEqu(VideoCaptureBitrate) &&
		OpEqu(VideoCaptureWidth) &&
		OpEqu(VideoCaptureHeight) &&
		OpEqu(AudioCaptureBitrate) &&

		OpEqu(Adapter) &&

		OpEqu(HWDumpDirectory) &&
		OpEqu(SWDumpDirectory));
}

bool Pcsx2Config::GSOptions::operator!=(const GSOptions& right) const
{
	return !operator==(right);
}

bool Pcsx2Config::GSOptions::RestartOptionsAreEqual(const GSOptions& right) const
{
	return OpEqu(Renderer) &&
		   OpEqu(Adapter) &&
		   OpEqu(UseDebugDevice) &&
		   OpEqu(UseBlitSwapChain) &&
		   OpEqu(DisableShaderCache) &&
		   OpEqu(DisableFramebufferFetch) &&
		   OpEqu(DisableVertexShaderExpand) &&
		   OpEqu(OverrideTextureBarriers) &&
		   OpEqu(ExclusiveFullscreenControl);
}

void Pcsx2Config::GSOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/GS");

#ifdef PCSX2_DEVBUILD
	SettingsWrapBitBool(SynchronousMTGS);
#endif

	SettingsWrapBitBool(VsyncEnable);
	SettingsWrapBitBool(DisableMailboxPresentation);
	SettingsWrapBitBool(ExtendedUpscalingMultipliers);

	SettingsWrapEntry(VsyncQueueSize);

	SettingsWrapEntry(FramerateNTSC);
	SettingsWrapEntry(FrameratePAL);

	SettingsWrapEnumEx(AspectRatio, "AspectRatio", AspectRatioNames);
	SettingsWrapEnumEx(FMVAspectRatioSwitch, "FMVAspectRatioSwitch", FMVAspectRatioSwitchNames);
	SettingsWrapIntEnumEx(ScreenshotSize, "ScreenshotSize");
	SettingsWrapIntEnumEx(ScreenshotFormat, "ScreenshotFormat");
	SettingsWrapEntry(ScreenshotQuality);
	SettingsWrapEntry(StretchY);
	SettingsWrapEntryEx(Crop[0], "CropLeft");
	SettingsWrapEntryEx(Crop[1], "CropTop");
	SettingsWrapEntryEx(Crop[2], "CropRight");
	SettingsWrapEntryEx(Crop[3], "CropBottom");

	// Unfortunately, because code in the GS still reads the setting by key instead of
	// using these variables, we need to use the old names. Maybe post 2.0 we can change this.
	SettingsWrapBitBoolEx(PCRTCAntiBlur, "pcrtc_antiblur");
	SettingsWrapBitBoolEx(DisableInterlaceOffset, "disable_interlace_offset");
	SettingsWrapBitBoolEx(PCRTCOffsets, "pcrtc_offsets");
	SettingsWrapBitBoolEx(PCRTCOverscan, "pcrtc_overscan");
	SettingsWrapBitBool(IntegerScaling);
	SettingsWrapBitBool(UseDebugDevice);
	SettingsWrapBitBool(UseBlitSwapChain);
	SettingsWrapBitBool(DisableShaderCache);
	SettingsWrapBitBool(DisableFramebufferFetch);
	SettingsWrapBitBool(DisableVertexShaderExpand);
	SettingsWrapBitBool(SkipDuplicateFrames);
	SettingsWrapBitBool(OsdShowSpeed);
	SettingsWrapBitBool(OsdShowFPS);
	SettingsWrapBitBool(OsdShowVPS);
	SettingsWrapBitBool(OsdShowCPU);
	SettingsWrapBitBool(OsdShowGPU);
	SettingsWrapBitBool(OsdShowResolution);
	SettingsWrapBitBool(OsdShowGSStats);
	SettingsWrapBitBool(OsdShowIndicators);
	SettingsWrapBitBool(OsdShowSettings);
	SettingsWrapBitBool(OsdShowInputs);
	SettingsWrapBitBool(OsdShowFrameTimes);
	SettingsWrapBitBool(OsdShowVersion);
	SettingsWrapBitBool(OsdShowHardwareInfo);
	SettingsWrapBitBool(OsdShowVideoCapture);
	SettingsWrapBitBool(OsdShowInputRec);

	SettingsWrapBitBool(HWSpinGPUForReadbacks);
	SettingsWrapBitBool(HWSpinCPUForReadbacks);
	SettingsWrapBitBoolEx(GPUPaletteConversion, "paltex");
	SettingsWrapBitBoolEx(AutoFlushSW, "autoflush_sw");
	SettingsWrapBitBoolEx(PreloadFrameWithGSData, "preload_frame_with_gs_data");
	SettingsWrapBitBoolEx(Mipmap, "mipmap");
	SettingsWrapBitBoolEx(ManualUserHacks, "UserHacks");
	SettingsWrapBitBoolEx(UserHacks_AlignSpriteX, "UserHacks_align_sprite_X");
	SettingsWrapIntEnumEx(UserHacks_AutoFlush, "UserHacks_AutoFlushLevel");
	SettingsWrapBitBoolEx(UserHacks_CPUFBConversion, "UserHacks_CPU_FB_Conversion");
	SettingsWrapBitBoolEx(UserHacks_ReadTCOnClose, "UserHacks_ReadTCOnClose");
	SettingsWrapBitBoolEx(UserHacks_DisableDepthSupport, "UserHacks_DisableDepthSupport");
	SettingsWrapBitBoolEx(UserHacks_DisablePartialInvalidation, "UserHacks_DisablePartialInvalidation");
	SettingsWrapBitBoolEx(UserHacks_DisableSafeFeatures, "UserHacks_Disable_Safe_Features");
	SettingsWrapBitBoolEx(UserHacks_DisableRenderFixes, "UserHacks_DisableRenderFixes");
	SettingsWrapBitBoolEx(UserHacks_MergePPSprite, "UserHacks_merge_pp_sprite");
	SettingsWrapBitBoolEx(UserHacks_ForceEvenSpritePosition, "UserHacks_ForceEvenSpritePosition");
	SettingsWrapIntEnumEx(UserHacks_BilinearHack, "UserHacks_BilinearHack");
	SettingsWrapBitBoolEx(UserHacks_NativePaletteDraw, "UserHacks_NativePaletteDraw");
	SettingsWrapIntEnumEx(UserHacks_TextureInsideRt, "UserHacks_TextureInsideRt");
	SettingsWrapBitBoolEx(UserHacks_EstimateTextureRegion, "UserHacks_EstimateTextureRegion");
	SettingsWrapBitBoolEx(FXAA, "fxaa");
	SettingsWrapBitBool(ShadeBoost);
	SettingsWrapBitBoolEx(DumpGSData, "DumpGSData");
	SettingsWrapBitBoolEx(SaveRT, "SaveRT");
	SettingsWrapBitBoolEx(SaveFrame, "SaveFrame");
	SettingsWrapBitBoolEx(SaveTexture, "SaveTexture");
	SettingsWrapBitBoolEx(SaveDepth, "SaveDepth");
	SettingsWrapBitBoolEx(SaveAlpha, "SaveAlpha");
	SettingsWrapBitBoolEx(SaveInfo, "SaveInfo");
	SettingsWrapBitBool(DumpReplaceableTextures);
	SettingsWrapBitBool(DumpReplaceableMipmaps);
	SettingsWrapBitBool(DumpTexturesWithFMVActive);
	SettingsWrapBitBool(DumpDirectTextures);
	SettingsWrapBitBool(DumpPaletteTextures);
	SettingsWrapBitBool(LoadTextureReplacements);
	SettingsWrapBitBool(LoadTextureReplacementsAsync);
	SettingsWrapBitBool(PrecacheTextureReplacements);
	SettingsWrapBitBool(EnableVideoCapture);
	SettingsWrapBitBool(EnableVideoCaptureParameters);
	SettingsWrapBitBool(VideoCaptureAutoResolution);
	SettingsWrapBitBool(EnableAudioCapture);
	SettingsWrapBitBool(EnableAudioCaptureParameters);

	SettingsWrapIntEnumEx(LinearPresent, "linear_present_mode");
	SettingsWrapIntEnumEx(InterlaceMode, "deinterlace_mode");

	SettingsWrapEntry(OsdScale);
	SettingsWrapIntEnumEx(OsdMessagesPos, "OsdMessagesPos");
	SettingsWrapIntEnumEx(OsdPerformancePos, "OsdPerformancePos");

	SettingsWrapIntEnumEx(Renderer, "Renderer");
	SettingsWrapEntryEx(UpscaleMultiplier, "upscale_multiplier");

	SettingsWrapBitBoolEx(HWMipmap, "hw_mipmap");
	SettingsWrapIntEnumEx(AccurateBlendingUnit, "accurate_blending_unit");
	SettingsWrapIntEnumEx(TextureFiltering, "filter");
	SettingsWrapIntEnumEx(TexturePreloading, "texture_preloading");
	SettingsWrapIntEnumEx(GSDumpCompression, "GSDumpCompression");
	SettingsWrapIntEnumEx(HWDownloadMode, "HWDownloadMode");
	SettingsWrapIntEnumEx(CASMode, "CASMode");
	SettingsWrapBitfieldEx(CAS_Sharpness, "CASSharpness");
	SettingsWrapBitfieldEx(Dithering, "dithering_ps2");
	SettingsWrapBitfieldEx(MaxAnisotropy, "MaxAnisotropy");
	SettingsWrapBitfieldEx(SWExtraThreads, "extrathreads");
	SettingsWrapBitfieldEx(SWExtraThreadsHeight, "extrathreads_height");
	SettingsWrapBitfieldEx(TVShader, "TVShader");
	SettingsWrapBitfieldEx(SkipDrawStart, "UserHacks_SkipDraw_Start");
	SettingsWrapBitfieldEx(SkipDrawEnd, "UserHacks_SkipDraw_End");
	SkipDrawEnd = std::max(SkipDrawStart, SkipDrawEnd);

	SettingsWrapIntEnumEx(UserHacks_HalfPixelOffset, "UserHacks_HalfPixelOffset");
	SettingsWrapBitfieldEx(UserHacks_RoundSprite, "UserHacks_round_sprite_offset");
	SettingsWrapIntEnumEx(UserHacks_NativeScaling, "UserHacks_native_scaling");
	SettingsWrapBitfieldEx(UserHacks_TCOffsetX, "UserHacks_TCOffsetX");
	SettingsWrapBitfieldEx(UserHacks_TCOffsetY, "UserHacks_TCOffsetY");
	SettingsWrapBitfieldEx(UserHacks_CPUSpriteRenderBW, "UserHacks_CPUSpriteRenderBW");
	SettingsWrapBitfieldEx(UserHacks_CPUSpriteRenderLevel, "UserHacks_CPUSpriteRenderLevel");
	SettingsWrapBitfieldEx(UserHacks_CPUCLUTRender, "UserHacks_CPUCLUTRender");
	SettingsWrapIntEnumEx(UserHacks_GPUTargetCLUTMode, "UserHacks_GPUTargetCLUTMode");
	SettingsWrapIntEnumEx(TriFilter, "TriFilter");
	SettingsWrapBitfieldEx(OverrideTextureBarriers, "OverrideTextureBarriers");

	SettingsWrapBitfield(ShadeBoost_Brightness);
	SettingsWrapBitfield(ShadeBoost_Contrast);
	SettingsWrapBitfield(ShadeBoost_Saturation);
	SettingsWrapBitfield(ExclusiveFullscreenControl);
	SettingsWrapBitfieldEx(PNGCompressionLevel, "png_compression_level");
	SettingsWrapBitfieldEx(SaveDrawStart, "SaveDrawStart");
	SettingsWrapBitfieldEx(SaveDrawCount, "SaveDrawCount");
	SettingsWrapBitfieldEx(SaveDrawBy, "SaveDrawBy");
	SettingsWrapBitfieldEx(SaveFrameStart, "SaveFrameStart");
	SettingsWrapBitfieldEx(SaveFrameCount, "SaveFrameCount");
	SettingsWrapBitfieldEx(SaveFrameBy, "SaveFrameBy");

	SettingsWrapEntryEx(CaptureContainer, "CaptureContainer");
	SettingsWrapEntryEx(VideoCaptureCodec, "VideoCaptureCodec");
	SettingsWrapEntryEx(VideoCaptureFormat, "VideoCaptureFormat");
	SettingsWrapEntryEx(VideoCaptureParameters, "VideoCaptureParameters");
	SettingsWrapEntryEx(AudioCaptureCodec, "AudioCaptureCodec");
	SettingsWrapEntryEx(AudioCaptureParameters, "AudioCaptureParameters");
	SettingsWrapBitfieldEx(VideoCaptureBitrate, "VideoCaptureBitrate");
	SettingsWrapBitfieldEx(VideoCaptureWidth, "VideoCaptureWidth");
	SettingsWrapBitfieldEx(VideoCaptureHeight, "VideoCaptureHeight");
	SettingsWrapBitfieldEx(AudioCaptureBitrate, "AudioCaptureBitrate");

	SettingsWrapEntry(Adapter);
	SettingsWrapEntry(HWDumpDirectory);
	if (!HWDumpDirectory.empty() && !Path::IsAbsolute(HWDumpDirectory))
		HWDumpDirectory = Path::Combine(EmuFolders::DataRoot, HWDumpDirectory);
	SettingsWrapEntry(SWDumpDirectory);
	if (!SWDumpDirectory.empty() && !Path::IsAbsolute(SWDumpDirectory))
		SWDumpDirectory = Path::Combine(EmuFolders::DataRoot, SWDumpDirectory);

	// Sanity check: don't dump a bunch of crap in the current working directory.
	if (DumpGSData && (HWDumpDirectory.empty() || SWDumpDirectory.empty()))
	{
		Console.Error("Draw dumping is enabled but directory is unconfigured, please set one.");
		DumpGSData = false;
	}
}

void Pcsx2Config::GSOptions::MaskUserHacks()
{
	if (ManualUserHacks)
		return;

	UserHacks_AlignSpriteX = false;
	UserHacks_MergePPSprite = false;
	UserHacks_ForceEvenSpritePosition = false;
	UserHacks_NativePaletteDraw = false;
	UserHacks_DisableSafeFeatures = false;
	UserHacks_DisableRenderFixes = false;
	UserHacks_HalfPixelOffset = GSHalfPixelOffset::Off;
	UserHacks_RoundSprite = 0;
	UserHacks_NativeScaling = GSNativeScaling::Off;
	UserHacks_AutoFlush = GSHWAutoFlushLevel::Disabled;
	GPUPaletteConversion = false;
	PreloadFrameWithGSData = false;
	UserHacks_DisablePartialInvalidation = false;
	UserHacks_DisableDepthSupport = false;
	UserHacks_CPUFBConversion = false;
	UserHacks_ReadTCOnClose = false;
	UserHacks_TextureInsideRt = GSTextureInRtMode::Disabled;
	UserHacks_EstimateTextureRegion = false;
	UserHacks_TCOffsetX = 0;
	UserHacks_TCOffsetY = 0;
	UserHacks_CPUSpriteRenderBW = 0;
	UserHacks_CPUSpriteRenderLevel = 0;
	UserHacks_CPUCLUTRender = 0;
	UserHacks_GPUTargetCLUTMode = GSGPUTargetCLUTMode::Disabled;
	UserHacks_BilinearHack = GSBilinearDirtyMode::Automatic;
	SkipDrawStart = 0;
	SkipDrawEnd = 0;
}

void Pcsx2Config::GSOptions::MaskUpscalingHacks()
{
	if (UpscaleMultiplier > 1.0f)
		return;

	UserHacks_AlignSpriteX = false;
	UserHacks_MergePPSprite = false;
	UserHacks_ForceEvenSpritePosition = false;
	UserHacks_BilinearHack = GSBilinearDirtyMode::Automatic;
	UserHacks_NativePaletteDraw = false;
	UserHacks_HalfPixelOffset = GSHalfPixelOffset::Off;
	UserHacks_RoundSprite = 0;
	UserHacks_NativeScaling = GSNativeScaling::Off;
	UserHacks_TCOffsetX = 0;
	UserHacks_TCOffsetY = 0;
}

bool Pcsx2Config::GSOptions::UseHardwareRenderer() const
{
	return (Renderer != GSRendererType::Null && Renderer != GSRendererType::SW);
}

bool Pcsx2Config::GSOptions::ShouldDump(int draw, int frame) const
{
	int drawOffset = draw - SaveDrawStart;
	int frameOffset = frame - SaveFrameStart;
	return DumpGSData &&
		   (drawOffset >= 0) && ((SaveDrawCount < 0) || (drawOffset < SaveDrawCount)) && (drawOffset % SaveDrawBy == 0) &&
		   (frameOffset >= 0) && ((SaveFrameCount < 0) || (frameOffset < SaveFrameCount)) && (frameOffset % SaveFrameBy == 0);
}

static constexpr const std::array s_spu2_sync_mode_names = {
	"Disabled",
	"TimeStretch",
};
static constexpr const std::array s_spu2_sync_mode_display_names = {
	TRANSLATE_NOOP("Pcsx2Config", "Disabled (Noisy)"),
	TRANSLATE_NOOP("Pcsx2Config", "TimeStretch (Recommended)"),
};

const char* Pcsx2Config::SPU2Options::GetSyncModeName(SPU2SyncMode mode)
{
	return (static_cast<size_t>(mode) < s_spu2_sync_mode_names.size()) ? s_spu2_sync_mode_names[static_cast<size_t>(mode)] : "";
}

const char* Pcsx2Config::SPU2Options::GetSyncModeDisplayName(SPU2SyncMode mode)
{
	return (static_cast<size_t>(mode) < s_spu2_sync_mode_display_names.size()) ?
			   Host::TranslateToCString("Pcsx2Config", s_spu2_sync_mode_display_names[static_cast<size_t>(mode)]) :
			   "";
}

std::optional<Pcsx2Config::SPU2Options::SPU2SyncMode> Pcsx2Config::SPU2Options::ParseSyncMode(const char* name)
{
	for (u8 i = 0; i < static_cast<u8>(SPU2SyncMode::Count); i++)
	{
		if (std::strcmp(name, s_spu2_sync_mode_names[i]) == 0)
			return static_cast<SPU2SyncMode>(i);
	}

	return std::nullopt;
}


Pcsx2Config::SPU2Options::SPU2Options()
{
	bitset = 0;
}

void Pcsx2Config::SPU2Options::LoadSave(SettingsWrapper& wrap)
{
	{
		SettingsWrapSection("SPU2/Debug");

		SettingsWrapBitBoolEx(DebugEnabled, "Global_Enable");
		SettingsWrapBitBoolEx(MsgToConsole, "Show_Messages");
		SettingsWrapBitBoolEx(MsgKeyOnOff, "Show_Messages_Key_On_Off");
		SettingsWrapBitBoolEx(MsgVoiceOff, "Show_Messages_Voice_Off");
		SettingsWrapBitBoolEx(MsgDMA, "Show_Messages_DMA_Transfer");
		SettingsWrapBitBoolEx(MsgAutoDMA, "Show_Messages_AutoDMA");
		SettingsWrapBitBoolEx(MsgCache, "Show_Messages_CacheStats");

		SettingsWrapBitBoolEx(AccessLog, "Log_Register_Access");
		SettingsWrapBitBoolEx(DMALog, "Log_DMA_Transfers");
		SettingsWrapBitBoolEx(WaveLog, "Log_WAVE_Output");

		SettingsWrapBitBoolEx(CoresDump, "Dump_Info");
		SettingsWrapBitBoolEx(MemDump, "Dump_Memory");
		SettingsWrapBitBoolEx(RegDump, "Dump_Regs");

		// If the global switch is off, save runtime checks.
		if (wrap.IsLoading() && !DebugEnabled)
		{
			MsgToConsole = false;
			MsgKeyOnOff = false;
			MsgVoiceOff = false;
			MsgDMA = false;
			MsgAutoDMA = false;
			MsgCache = false;
			AccessLog = false;
			DMALog = false;
			WaveLog = false;
			CoresDump = false;
			MemDump = false;
			RegDump = false;
		}
	}

	{
		SettingsWrapSection("SPU2/Output");
		SettingsWrapEntry(OutputVolume);
		SettingsWrapEntry(FastForwardVolume);
		SettingsWrapEntry(OutputMuted);
		SettingsWrapParsedEnum(Backend, "Backend", &AudioStream::ParseBackendName, &AudioStream::GetBackendName);
		SettingsWrapParsedEnum(SyncMode, "SyncMode", &ParseSyncMode, &GetSyncModeName);
		SettingsWrapEntry(DriverName);
		SettingsWrapEntry(DeviceName);
		StreamParameters.LoadSave(wrap, CURRENT_SETTINGS_SECTION);
	}
}

bool Pcsx2Config::SPU2Options::operator!=(const SPU2Options& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::SPU2Options::operator==(const SPU2Options& right) const
{
	return OpEqu(bitset) &&
		   OpEqu(OutputVolume) &&
		   OpEqu(FastForwardVolume) &&
		   OpEqu(OutputMuted) &&
		   OpEqu(Backend) &&
		   OpEqu(StreamParameters) &&
		   OpEqu(DriverName) &&
		   OpEqu(DeviceName);
}

const char* Pcsx2Config::DEV9Options::NetApiNames[] = {
	"Unset",
	"PCAP Bridged",
	"PCAP Switched",
	"TAP",
	"Sockets",
	nullptr};

const char* Pcsx2Config::DEV9Options::DnsModeNames[] = {
	"Manual",
	"Auto",
	"Internal",
	nullptr};

Pcsx2Config::DEV9Options::DEV9Options()
{
	HddFile = "DEV9hdd.raw";
}

void Pcsx2Config::DEV9Options::LoadSave(SettingsWrapper& wrap)
{
	{
		SettingsWrapSection("DEV9/Eth");
		SettingsWrapEntry(EthEnable);
		SettingsWrapEnumEx(EthApi, "EthApi", NetApiNames);
		SettingsWrapEntry(EthDevice);
		SettingsWrapEntry(EthLogDHCP);
		SettingsWrapEntry(EthLogDNS);

		SettingsWrapEntry(InterceptDHCP);

		std::string ps2IPStr = "0.0.0.0";
		std::string maskStr = "0.0.0.0";
		std::string gatewayStr = "0.0.0.0";
		std::string dns1Str = "0.0.0.0";
		std::string dns2Str = "0.0.0.0";
		if (wrap.IsSaving())
		{
			ps2IPStr = SaveIPHelper(PS2IP);
			maskStr = SaveIPHelper(Mask);
			gatewayStr = SaveIPHelper(Gateway);
			dns1Str = SaveIPHelper(DNS1);
			dns2Str = SaveIPHelper(DNS2);
		}
		SettingsWrapEntryEx(ps2IPStr, "PS2IP");
		SettingsWrapEntryEx(maskStr, "Mask");
		SettingsWrapEntryEx(gatewayStr, "Gateway");
		SettingsWrapEntryEx(dns1Str, "DNS1");
		SettingsWrapEntryEx(dns2Str, "DNS2");
		if (wrap.IsLoading())
		{
			LoadIPHelper(PS2IP, ps2IPStr);
			LoadIPHelper(Mask, maskStr);
			LoadIPHelper(Gateway, gatewayStr);
			LoadIPHelper(DNS1, dns1Str);
			LoadIPHelper(DNS1, dns1Str);
		}

		SettingsWrapEntry(AutoMask);
		SettingsWrapEntry(AutoGateway);
		SettingsWrapEnumEx(ModeDNS1, "ModeDNS1", DnsModeNames);
		SettingsWrapEnumEx(ModeDNS2, "ModeDNS2", DnsModeNames);
	}

	if (wrap.IsLoading())
		EthHosts.clear();

	int hostCount = static_cast<int>(EthHosts.size());
	{
		SettingsWrapSection("DEV9/Eth/Hosts");
		SettingsWrapEntryEx(hostCount, "Count");
	}

	for (int i = 0; i < hostCount; i++)
	{
		std::string section = "DEV9/Eth/Hosts/Host" + std::to_string(i);
		SettingsWrapSection(section.c_str());

		HostEntry entry;
		if (wrap.IsSaving())
			entry = EthHosts[i];

		SettingsWrapEntryEx(entry.Url, "Url");
		SettingsWrapEntryEx(entry.Desc, "Desc");

		std::string addrStr = "0.0.0.0";
		if (wrap.IsSaving())
			addrStr = SaveIPHelper(entry.Address);
		SettingsWrapEntryEx(addrStr, "Address");
		if (wrap.IsLoading())
			LoadIPHelper(entry.Address, addrStr);

		SettingsWrapEntryEx(entry.Enabled, "Enabled");

		if (wrap.IsLoading())
		{
			EthHosts.push_back(entry);

			if (EthLogDNS && entry.Enabled)
				Console.WriteLn("DEV9: Host entry %i: url %s mapped to %s", i, entry.Url.c_str(), addrStr.c_str());
		}
	}

	{
		SettingsWrapSection("DEV9/Hdd");
		SettingsWrapEntry(HddEnable);
		SettingsWrapEntry(HddFile);
	}
}

bool Pcsx2Config::DEV9Options::operator!=(const DEV9Options& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::DEV9Options::operator==(const DEV9Options& right) const
{
	return OpEqu(EthEnable) &&
		   OpEqu(EthApi) &&
		   OpEqu(EthDevice) &&
		   OpEqu(EthLogDHCP) &&
		   OpEqu(EthLogDNS) &&

		   OpEqu(InterceptDHCP) &&
		   (*(int*)PS2IP == *(int*)right.PS2IP) &&
		   (*(int*)Gateway == *(int*)right.Gateway) &&
		   (*(int*)DNS1 == *(int*)right.DNS1) &&
		   (*(int*)DNS2 == *(int*)right.DNS2) &&

		   OpEqu(AutoMask) &&
		   OpEqu(AutoGateway) &&
		   OpEqu(ModeDNS1) &&
		   OpEqu(ModeDNS2) &&

		   OpEqu(EthHosts) &&

		   OpEqu(HddEnable) &&
		   OpEqu(HddFile);
}

void Pcsx2Config::DEV9Options::LoadIPHelper(u8* field, const std::string& setting)
{
	if (4 == sscanf(setting.c_str(), "%hhu.%hhu.%hhu.%hhu", &field[0], &field[1], &field[2], &field[3]))
		return;
	Console.Error("Invalid IP address in settings file");
	std::fill(field, field + 4, 0);
}
std::string Pcsx2Config::DEV9Options::SaveIPHelper(u8* field)
{
	return StringUtil::StdStringFromFormat("%u.%u.%u.%u", field[0], field[1], field[2], field[3]);
}

bool Pcsx2Config::DEV9Options::HostEntry::operator==(const HostEntry& right) const
{
	return OpEqu(Url) &&
		   OpEqu(Desc) &&
		   (*(int*)Address == *(int*)right.Address) &&
		   OpEqu(Enabled);
}

bool Pcsx2Config::DEV9Options::HostEntry::operator!=(const HostEntry& right) const
{
	return !this->operator==(right);
}

static const char* const tbl_GamefixNames[] =
	{
		"FpuMul",
		"GoemonTlb",
		"SoftwareRendererFMV",
		"SkipMPEG",
		"OPHFlag",
		"EETiming",
		"InstantDMA",
		"DMABusy",
		"GIFFIFO",
		"VIFFIFO",
		"VIF1Stall",
		"VuAddSub",
		"Ibit",
		"VUSync",
		"VUOverflow",
		"XGKick",
		"BlitInternalFPS",
		"FullVU0Sync",
};

const char* Pcsx2Config::GamefixOptions::GetGameFixName(GamefixId id)
{
	return tbl_GamefixNames[id];
}

// all gamefixes are disabled by default.
Pcsx2Config::GamefixOptions::GamefixOptions()
{
	DisableAll();
}

Pcsx2Config::GamefixOptions& Pcsx2Config::GamefixOptions::DisableAll()
{
	bitset = 0;
	return *this;
}

void Pcsx2Config::GamefixOptions::Set(GamefixId id, bool enabled)
{
	switch (id)
	{
			// clang-format off
		case Fix_VuAddSub:            VuAddSubHack            = enabled; break;
		case Fix_FpuMultiply:         FpuMulHack              = enabled; break;
		case Fix_XGKick:              XgKickHack              = enabled; break;
		case Fix_EETiming:            EETimingHack            = enabled; break;
		case Fix_InstantDMA:          InstantDMAHack          = enabled; break;
		case Fix_SoftwareRendererFMV: SoftwareRendererFMVHack = enabled; break;
		case Fix_SkipMpeg:            SkipMPEGHack            = enabled; break;
		case Fix_OPHFlag:             OPHFlagHack             = enabled; break;
		case Fix_DMABusy:             DMABusyHack             = enabled; break;
		case Fix_VIFFIFO:             VIFFIFOHack             = enabled; break;
		case Fix_VIF1Stall:           VIF1StallHack           = enabled; break;
		case Fix_GIFFIFO:             GIFFIFOHack             = enabled; break;
		case Fix_GoemonTlbMiss:       GoemonTlbHack           = enabled; break;
		case Fix_Ibit:                IbitHack                = enabled; break;
		case Fix_VUSync:              VUSyncHack              = enabled; break;
		case Fix_VUOverflow:          VUOverflowHack          = enabled; break;
		case Fix_BlitInternalFPS:     BlitInternalFPSHack     = enabled; break;
		case Fix_FullVU0Sync:         FullVU0SyncHack         = enabled; break;
		default:                                                         break;
			// clang-format on
	}
}

bool Pcsx2Config::GamefixOptions::operator!=(const GamefixOptions& right) const
{
	return !OpEqu(bitset);
}

bool Pcsx2Config::GamefixOptions::operator==(const GamefixOptions& right) const
{
	return OpEqu(bitset);
}

bool Pcsx2Config::GamefixOptions::Get(GamefixId id) const
{
	switch (id)
	{
			// clang-format off
		case Fix_VuAddSub:            return VuAddSubHack;
		case Fix_FpuMultiply:         return FpuMulHack;
		case Fix_XGKick:              return XgKickHack;
		case Fix_EETiming:            return EETimingHack;
		case Fix_InstantDMA:          return InstantDMAHack;
		case Fix_SoftwareRendererFMV: return SoftwareRendererFMVHack;
		case Fix_SkipMpeg:            return SkipMPEGHack;
		case Fix_OPHFlag:             return OPHFlagHack;
		case Fix_DMABusy:             return DMABusyHack;
		case Fix_VIFFIFO:             return VIFFIFOHack;
		case Fix_VIF1Stall:           return VIF1StallHack;
		case Fix_GIFFIFO:             return GIFFIFOHack;
		case Fix_GoemonTlbMiss:       return GoemonTlbHack;
		case Fix_Ibit:                return IbitHack;
		case Fix_VUSync:              return VUSyncHack;
		case Fix_VUOverflow:          return VUOverflowHack;
		case Fix_BlitInternalFPS:     return BlitInternalFPSHack;
		case Fix_FullVU0Sync:         return FullVU0SyncHack;
		default:                      return false;
			// clang-format on
	}
	return false; // unreachable, but we still need to suppress warnings >_<
}

void Pcsx2Config::GamefixOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/Gamefixes");

	SettingsWrapBitBool(VuAddSubHack);
	SettingsWrapBitBool(FpuMulHack);
	SettingsWrapBitBool(XgKickHack);
	SettingsWrapBitBool(EETimingHack);
	SettingsWrapBitBool(InstantDMAHack);
	SettingsWrapBitBool(SoftwareRendererFMVHack);
	SettingsWrapBitBool(SkipMPEGHack);
	SettingsWrapBitBool(OPHFlagHack);
	SettingsWrapBitBool(DMABusyHack);
	SettingsWrapBitBool(VIFFIFOHack);
	SettingsWrapBitBool(VIF1StallHack);
	SettingsWrapBitBool(GIFFIFOHack);
	SettingsWrapBitBool(GoemonTlbHack);
	SettingsWrapBitBool(IbitHack);
	SettingsWrapBitBool(VUSyncHack);
	SettingsWrapBitBool(VUOverflowHack);
	SettingsWrapBitBool(BlitInternalFPSHack);
	SettingsWrapBitBool(FullVU0SyncHack);
}

const char* Pcsx2Config::DebugAnalysisOptions::RunConditionNames[] = {
	"Always",
	"If Debugger Is Open",
	"Never",
	nullptr,
};

const char* Pcsx2Config::DebugAnalysisOptions::FunctionScanModeNames[] = {
	"Scan From ELF",
	"Scan From Memory",
	"Skip",
	nullptr,
};

void Pcsx2Config::DebugAnalysisOptions::LoadSave(SettingsWrapper& wrap)
{
	{
		SettingsWrapSection("Debugger/Analysis");

		SettingsWrapEnumEx(RunCondition, "RunCondition", RunConditionNames);
		SettingsWrapBitBool(GenerateSymbolsForIRXExports);

		SettingsWrapBitBool(AutomaticallySelectSymbolsToClear);

		SettingsWrapBitBool(ImportSymbolsFromELF);
		SettingsWrapBitBool(DemangleSymbols);
		SettingsWrapBitBool(DemangleParameters);

		SettingsWrapEnumEx(FunctionScanMode, "FunctionScanMode", FunctionScanModeNames);
		SettingsWrapBitBool(CustomFunctionScanRange);
		SettingsWrapEntry(FunctionScanStartAddress);
		SettingsWrapEntry(FunctionScanEndAddress);

		SettingsWrapBitBool(GenerateFunctionHashes);
	}

	int symbolSourceCount = static_cast<int>(SymbolSources.size());
	{
		SettingsWrapSection("Debugger/Analysis/SymbolSources");
		SettingsWrapEntryEx(symbolSourceCount, "Count");
	}

	for (int i = 0; i < symbolSourceCount; i++)
	{
		std::string section = "Debugger/Analysis/SymbolSources/" + std::to_string(i);
		SettingsWrapSection(section.c_str());

		DebugSymbolSource Source;
		if (wrap.IsSaving())
			Source = SymbolSources[i];

		SettingsWrapEntryEx(Source.Name, "Name");
		SettingsWrapBitBoolEx(Source.ClearDuringAnalysis, "ClearDuringAnalysis");

		if (wrap.IsLoading())
			SymbolSources.emplace_back(std::move(Source));
	}

	int extraSymbolFileCount = static_cast<int>(ExtraSymbolFiles.size());
	{
		SettingsWrapSection("Debugger/Analysis/ExtraSymbolFiles");
		SettingsWrapEntryEx(extraSymbolFileCount, "Count");
	}

	for (int i = 0; i < extraSymbolFileCount; i++)
	{
		std::string section = "Debugger/Analysis/ExtraSymbolFiles/" + std::to_string(i);
		SettingsWrapSection(section.c_str());

		DebugExtraSymbolFile file;
		if (wrap.IsSaving())
			file = ExtraSymbolFiles[i];

		SettingsWrapEntryEx(file.Path, "Path");
		SettingsWrapEntryEx(file.BaseAddress, "BaseAddress");
		SettingsWrapEntryEx(file.Condition, "Condition");

		if (wrap.IsLoading())
			ExtraSymbolFiles.emplace_back(std::move(file));
	}
}

Pcsx2Config::SavestateOptions::SavestateOptions()
{
}

void Pcsx2Config::SavestateOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore");

	SettingsWrapIntEnumEx(CompressionType, "SavestateCompressionType");
	SettingsWrapIntEnumEx(CompressionRatio, "SavestateCompressionRatio");
}

bool Pcsx2Config::SavestateOptions::operator!=(const SavestateOptions& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::SavestateOptions::operator==(const SavestateOptions& right) const
{
	return OpEqu(CompressionType) && OpEqu(CompressionRatio);
};

Pcsx2Config::FilenameOptions::FilenameOptions()
{
}

void Pcsx2Config::FilenameOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("Filenames");

	wrap.Entry(CURRENT_SETTINGS_SECTION, "BIOS", Bios, Bios);
}

bool Pcsx2Config::FilenameOptions::operator!=(const FilenameOptions& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::FilenameOptions::operator==(const FilenameOptions& right) const
{
	return OpEqu(Bios);
}

Pcsx2Config::EmulationSpeedOptions::EmulationSpeedOptions()
{
	bitset = 0;

	SyncToHostRefreshRate = false;
}

void Pcsx2Config::EmulationSpeedOptions::SanityCheck()
{
	// Ensure Conformation of various options...

	NominalScalar = std::clamp(NominalScalar, 0.05f, 10.0f);
	TurboScalar = std::clamp(TurboScalar, 0.05f, 10.0f);
	SlomoScalar = std::clamp(SlomoScalar, 0.05f, 10.0f);
}

void Pcsx2Config::EmulationSpeedOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("Framerate");

	SettingsWrapEntry(NominalScalar);
	SettingsWrapEntry(TurboScalar);
	SettingsWrapEntry(SlomoScalar);

	// This was in the wrong place... but we can't change it without breaking existing configs.
	//SettingsWrapBitBool(SyncToHostRefreshRate);
	SyncToHostRefreshRate = wrap.EntryBitBool("EmuCore/GS", "SyncToHostRefreshRate", SyncToHostRefreshRate, SyncToHostRefreshRate);
	UseVSyncForTiming = wrap.EntryBitBool("EmuCore/GS", "UseVSyncForTiming", UseVSyncForTiming, UseVSyncForTiming);
}

bool Pcsx2Config::EmulationSpeedOptions::operator==(const EmulationSpeedOptions& right) const
{
	return OpEqu(bitset) && OpEqu(NominalScalar) && OpEqu(TurboScalar) && OpEqu(SlomoScalar);
}

bool Pcsx2Config::EmulationSpeedOptions::operator!=(const EmulationSpeedOptions& right) const
{
	return !this->operator==(right);
}

Pcsx2Config::USBOptions::USBOptions()
{
	for (u32 i = 0; i < static_cast<u32>(Ports.size()); i++)
	{
		Ports[i].DeviceType = -1;
		Ports[i].DeviceSubtype = 0;
	}
}

void Pcsx2Config::USBOptions::LoadSave(SettingsWrapper& wrap)
{
	for (u32 i = 0; i < static_cast<u32>(Ports.size()); i++)
	{
		const std::string section(USB::GetConfigSection(i));

		std::string device = USB::DeviceTypeIndexToName(Ports[i].DeviceType);
		wrap.Entry(section.c_str(), "Type", device, device);

		if (wrap.IsLoading())
			Ports[i].DeviceType = USB::DeviceTypeNameToIndex(device);

		if (Ports[i].DeviceType >= 0)
		{
			const std::string subtype_key(fmt::format("{}_subtype", USB::DeviceTypeIndexToName(Ports[i].DeviceType)));
			wrap.Entry(section.c_str(), subtype_key.c_str(), Ports[i].DeviceSubtype);
		}
	}
}

bool Pcsx2Config::USBOptions::Port::operator==(const USBOptions::Port& right) const
{
	return OpEqu(DeviceType) && OpEqu(DeviceSubtype);
}

bool Pcsx2Config::USBOptions::Port::operator!=(const USBOptions::Port& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::USBOptions::operator==(const USBOptions& right) const
{
	for (u32 i = 0; i < static_cast<u32>(Ports.size()); i++)
	{
		if (!OpEqu(Ports[i]))
			return false;
	}

	return true;
}

bool Pcsx2Config::USBOptions::operator!=(const USBOptions& right) const
{
	return !this->operator==(right);
}

Pcsx2Config::PadOptions::PadOptions()
{
	for (u32 i = 0; i < static_cast<u32>(Ports.size()); i++)
	{
		Port& port = Ports[i];
		port.Type = Pad::GetDefaultPadType(i);
	}

	bitset = 0;
}

void Pcsx2Config::PadOptions::LoadSave(SettingsWrapper& wrap)
{
	for (u32 i = 0; i < static_cast<u32>(Ports.size()); i++)
	{
		Port& port = Ports[i];

		std::string section = Pad::GetConfigSection(i);
		std::string type_name = Pad::GetControllerInfo(port.Type)->name;
		wrap.Entry(section.c_str(), "Type", type_name, type_name);

		if (wrap.IsLoading())
		{
			const Pad::ControllerInfo* cinfo = Pad::GetControllerInfoByName(type_name);
			if (cinfo)
			{
				port.Type = cinfo->type;
			}
			else
			{
				Console.Error(fmt::format("Invalid controller type {} specified in config, disconnecting.", type_name));
				port.Type = Pad::ControllerType::NotConnected;
			}
		}
	}

	SettingsWrapSection("Pad");
	SettingsWrapBitBoolEx(MultitapPort0_Enabled, "MultitapPort1");
	SettingsWrapBitBoolEx(MultitapPort1_Enabled, "MultitapPort2");
}


bool Pcsx2Config::PadOptions::operator==(const PadOptions& right) const
{
	for (u32 i = 0; i < static_cast<u32>(Ports.size()); i++)
	{
		if (!OpEqu(Ports[i]))
			return false;
	}

	return true;
}

bool Pcsx2Config::PadOptions::operator!=(const PadOptions& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::PadOptions::Port::operator==(const PadOptions::Port& right) const
{
	return OpEqu(Type);
}

bool Pcsx2Config::PadOptions::Port::operator!=(const PadOptions::Port& right) const
{
	return !this->operator==(right);
}

Pcsx2Config::AchievementsOptions::AchievementsOptions()
{
	Enabled = false;
	HardcoreMode = false;
	EncoreMode = false;
	SpectatorMode = false;
	UnofficialTestMode = false;
	Notifications = true;
	LeaderboardNotifications = true;
	SoundEffects = true;
	InfoSound = true;
	UnlockSound = true;
	LBSubmitSound = true;
	Overlays = true;
	LBOverlays = true;
}

void Pcsx2Config::AchievementsOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("Achievements");

	if (InfoSoundName.empty())
		InfoSoundName = Path::Combine(EmuFolders::Resources, DEFAULT_INFO_SOUND_NAME);

	if (UnlockSoundName.empty())
		UnlockSoundName = Path::Combine(EmuFolders::Resources, DEFAULT_UNLOCK_SOUND_NAME);

	if (LBSubmitSoundName.empty())
		LBSubmitSoundName = Path::Combine(EmuFolders::Resources, DEFAULT_LBSUBMIT_SOUND_NAME);

	SettingsWrapBitBool(Enabled);
	SettingsWrapBitBoolEx(HardcoreMode, "ChallengeMode");
	SettingsWrapBitBool(EncoreMode);
	SettingsWrapBitBool(SpectatorMode);
	SettingsWrapBitBool(UnofficialTestMode);
	SettingsWrapBitBool(Notifications);
	SettingsWrapBitBool(LeaderboardNotifications);
	SettingsWrapBitBool(SoundEffects);
	SettingsWrapBitBool(InfoSound);
	SettingsWrapBitBool(UnlockSound);
	SettingsWrapBitBool(LBSubmitSound);
	SettingsWrapBitBool(Overlays);
	SettingsWrapBitBool(LBOverlays);
	SettingsWrapEntry(NotificationsDuration);
	SettingsWrapEntry(LeaderboardsDuration);
	SettingsWrapEntry(InfoSoundName);
	SettingsWrapEntry(UnlockSoundName);
	SettingsWrapEntry(LBSubmitSoundName);

	if (wrap.IsLoading())
	{
		//Clamp in case setting was updated manually using the INI
		NotificationsDuration = std::clamp(NotificationsDuration, MINIMUM_NOTIFICATION_DURATION, MAXIMUM_NOTIFICATION_DURATION);
		LeaderboardsDuration = std::clamp(LeaderboardsDuration, MINIMUM_NOTIFICATION_DURATION, MAXIMUM_NOTIFICATION_DURATION);
	}
}

bool Pcsx2Config::AchievementsOptions::operator==(const AchievementsOptions& right) const
{
	return OpEqu(bitset) && OpEqu(NotificationsDuration) && OpEqu(LeaderboardsDuration);
}

bool Pcsx2Config::AchievementsOptions::operator!=(const AchievementsOptions& right) const
{
	return !this->operator==(right);
}

Pcsx2Config::Pcsx2Config()
{
	bitset = 0;
	// Set defaults for fresh installs / reset settings
	McdFolderAutoManage = true;
	EnablePatches = true;
	EnableFastBoot = true;
	EnableRecordingTools = true;
	EnableGameFixes = true;
	InhibitScreensaver = true;
	UseSavestateSelector = true;
	BackupSavestate = true;
	WarnAboutUnsafeSettings = true;
	ManuallySetRealTimeClock = false;

	// To be moved to FileMemoryCard pluign (someday)
	for (uint slot = 0; slot < 8; ++slot)
	{
		Mcd[slot].Enabled = !FileMcd_IsMultitapSlot(slot); // enables main 2 slots
		Mcd[slot].Filename = FileMcd_GetDefaultName(slot);
		// Folder memory card is autodetected later.
		Mcd[slot].Type = MemoryCardType::File;
	}

	GzipIsoIndexTemplate = "$(f).pindex.tmp";
	PINESlot = 28011;
	RtcYear = 0;
	RtcMonth = 1;
	RtcDay = 1;
	RtcHour = 0;
	RtcMinute = 0;
	RtcSecond = 0;
}

void Pcsx2Config::LoadSaveCore(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore");

	SettingsWrapBitBool(CdvdVerboseReads);
	SettingsWrapBitBool(CdvdDumpBlocks);
	SettingsWrapBitBool(CdvdPrecache);
	SettingsWrapBitBool(EnablePatches);
	SettingsWrapBitBool(EnableCheats);
	SettingsWrapBitBool(EnablePINE);
	SettingsWrapBitBool(EnableWideScreenPatches);
	SettingsWrapBitBool(EnableNoInterlacingPatches);
	SettingsWrapBitBool(EnableFastBoot);
	SettingsWrapBitBool(EnableFastBootFastForward);
	SettingsWrapBitBool(EnableThreadPinning);
	SettingsWrapBitBool(EnableRecordingTools);
	SettingsWrapBitBool(EnableGameFixes);
	SettingsWrapBitBool(SaveStateOnShutdown);
	SettingsWrapBitBool(UseSavestateSelector);
	SettingsWrapBitBool(EnableDiscordPresence);
	SettingsWrapBitBool(InhibitScreensaver);
	SettingsWrapBitBool(HostFs);

	SettingsWrapBitBool(BackupSavestate);
	SettingsWrapBitBool(McdFolderAutoManage);

	SettingsWrapBitBool(WarnAboutUnsafeSettings);

	SettingsWrapBitBool(ManuallySetRealTimeClock);

	// Process various sub-components:

	Speedhacks.LoadSave(wrap);
	Cpu.LoadSave(wrap);
	GS.LoadSave(wrap);
	SPU2.LoadSave(wrap);
	DEV9.LoadSave(wrap);
	Gamefixes.LoadSave(wrap);
	Profiler.LoadSave(wrap);
	Savestate.LoadSave(wrap);

	DebuggerAnalysis.LoadSave(wrap);
	Trace.LoadSave(wrap);

	Achievements.LoadSave(wrap);

	SettingsWrapEntry(GzipIsoIndexTemplate);
	SettingsWrapEntry(PINESlot);
	SettingsWrapEntry(RtcYear);
	SettingsWrapEntry(RtcMonth);
	SettingsWrapEntry(RtcDay);
	SettingsWrapEntry(RtcHour);
	SettingsWrapEntry(RtcMinute);
	SettingsWrapEntry(RtcSecond);

	// For now, this in the derived config for backwards ini compatibility.
	SettingsWrapEntryEx(CurrentBlockdump, "BlockDumpSaveDirectory");

	BaseFilenames.LoadSave(wrap);
	EmulationSpeed.LoadSave(wrap);
	LoadSaveMemcards(wrap);

	if (wrap.IsLoading())
	{
		// Patches will get re-applied after loading the state so this doesn't matter too much
		CurrentAspectRatio = GS.AspectRatio;
		if (CurrentAspectRatio == AspectRatioType::RAuto4_3_3_2)
		{
			CurrentCustomAspectRatio = 0.f;
		}
	}
}

void Pcsx2Config::LoadSave(SettingsWrapper& wrap)
{
	LoadSaveCore(wrap);
	USB.LoadSave(wrap);
	Pad.LoadSave(wrap);
}

void Pcsx2Config::LoadSaveMemcards(SettingsWrapper& wrap)
{
	for (uint slot = 0; slot < 2; ++slot)
	{
		wrap.Entry("MemoryCards", StringUtil::StdStringFromFormat("Slot%u_Enable", slot + 1).c_str(),
			Mcd[slot].Enabled, Mcd[slot].Enabled);
		wrap.Entry("MemoryCards", StringUtil::StdStringFromFormat("Slot%u_Filename", slot + 1).c_str(),
			Mcd[slot].Filename, Mcd[slot].Filename);
	}

	for (uint slot = 2; slot < 8; ++slot)
	{
		int mtport = FileMcd_GetMtapPort(slot) + 1;
		int mtslot = FileMcd_GetMtapSlot(slot) + 1;

		wrap.Entry("MemoryCards", StringUtil::StdStringFromFormat("Multitap%u_Slot%u_Enable", mtport, mtslot).c_str(),
			Mcd[slot].Enabled, Mcd[slot].Enabled);
		wrap.Entry("MemoryCards", StringUtil::StdStringFromFormat("Multitap%u_Slot%u_Filename", mtport, mtslot).c_str(),
			Mcd[slot].Filename, Mcd[slot].Filename);
	}
}

std::string Pcsx2Config::FullpathToBios() const
{
	std::string ret;
	if (!BaseFilenames.Bios.empty())
		ret = Path::Combine(EmuFolders::Bios, BaseFilenames.Bios);
	return ret;
}

std::string Pcsx2Config::FullpathToMcd(uint slot) const
{
	return Path::Combine(EmuFolders::MemoryCards, Mcd[slot].Filename);
}

void Pcsx2Config::CopyRuntimeConfig(Pcsx2Config& cfg)
{
	CurrentBlockdump = std::move(cfg.CurrentBlockdump);
	CurrentIRX = std::move(cfg.CurrentIRX);
	CurrentGameArgs = std::move(cfg.CurrentGameArgs);
	CurrentAspectRatio = cfg.CurrentAspectRatio;
	CurrentCustomAspectRatio = cfg.CurrentCustomAspectRatio;
	IsPortableMode = cfg.IsPortableMode;

	for (u32 i = 0; i < sizeof(Mcd) / sizeof(Mcd[0]); i++)
	{
		Mcd[i].Type = cfg.Mcd[i].Type;
	}
}

void Pcsx2Config::CopyConfiguration(SettingsInterface* dest_si, SettingsInterface& src_si)
{
	FPControlRegisterBackup fpcr_backup(FPControlRegister::GetDefault());

	Pcsx2Config temp;
	{
		SettingsLoadWrapper wrapper(src_si);
		temp.LoadSaveCore(wrapper);
	}
	{
		SettingsSaveWrapper wrapper(*dest_si);
		temp.LoadSaveCore(wrapper);
	}
}

void Pcsx2Config::ClearConfiguration(SettingsInterface* dest_si)
{
	FPControlRegisterBackup fpcr_backup(FPControlRegister::GetDefault());

	Pcsx2Config temp;
	SettingsClearWrapper wrapper(*dest_si);
	temp.LoadSaveCore(wrapper);
}

void Pcsx2Config::ClearInvalidPerGameConfiguration(SettingsInterface* si)
{
	// Deprecated in favor of patches.
	si->DeleteValue("EmuCore", "EnableWideScreenPatches");
	si->DeleteValue("EmuCore", "EnableNoInterlacingPatches");
}

void EmuFolders::SetAppRoot()
{
	std::string program_path = FileSystem::GetProgramPath();
#ifdef __APPLE__
	const auto bundle_path = CocoaTools::GetNonTranslocatedBundlePath();
	if (bundle_path.has_value())
	{
		// On macOS, override with the bundle path if launched from a bundle.
		program_path = bundle_path.value();
	}
#endif
	Console.WriteLnFmt("Program Path: {}", program_path);

	AppRoot = Path::Canonicalize(Path::GetDirectory(program_path));

	// logging of directories in case something goes wrong super early
	Console.WriteLnFmt("AppRoot Directory: {}", AppRoot);
}

bool EmuFolders::SetResourcesDirectory()
{
#ifndef __APPLE__
#ifndef PCSX2_APP_DATADIR
	// On Windows/Linux, these are in the binary directory.
	Resources = Path::Combine(AppRoot, "resources");
#else
	Resources = Path::Canonicalize(Path::Combine(AppRoot, PCSX2_APP_DATADIR "/resources"));
#endif
#else
	// On macOS, this is in the bundle resources directory.
	const std::string program_path = FileSystem::GetProgramPath();
	Resources = Path::Canonicalize(Path::Combine(Path::GetDirectory(program_path), "../Resources"));
#endif

	Console.WriteLnFmt("Resources Directory: {}", Resources);

	// the resources directory should exist, bail out if not
	if (!FileSystem::DirectoryExists(Resources.c_str()))
	{
		Console.Error("Resources directory is missing.");
		return false;
	}

	return true;
}

bool EmuFolders::ShouldUsePortableMode()
{
	// Check whether portable.ini/txt exists in the program directory or the `-portable` launch arguments have been passed.
	if (FileSystem::FileExists(Path::Combine(AppRoot, "portable.ini").c_str()) ||
		FileSystem::FileExists(Path::Combine(AppRoot, "portable.txt").c_str()) ||
		EmuConfig.IsPortableMode)
	{
		return true;
	}

	return false;
}

std::string EmuFolders::GetPortableModePath()
{
	const auto portable_txt_path = Path::Combine(AppRoot, "portable.txt");
	const auto portable_path = FileSystem::ReadFileToString(portable_txt_path.c_str()).value_or("");
	const auto trimmed_path = StringUtil::StripWhitespace(portable_path);
	return std::string(trimmed_path);
}

bool EmuFolders::SetDataDirectory(Error* error)
{
	if (!ShouldUsePortableMode())
	{
#if defined(_WIN32)
		// On Windows, use My Documents\PCSX2 to match old installs.
		PWSTR documents_directory;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documents_directory)))
		{
			if (std::wcslen(documents_directory) > 0)
				DataRoot = Path::Combine(StringUtil::WideStringToUTF8String(documents_directory), "PCSX2");
			CoTaskMemFree(documents_directory);
		}
#elif defined(__linux__) || defined(__FreeBSD__)
		// Use $XDG_CONFIG_HOME/PCSX2 if it exists.
		const char* xdg_config_home = getenv("XDG_CONFIG_HOME");
		if (xdg_config_home && Path::IsAbsolute(xdg_config_home))
		{
			DataRoot = Path::RealPath(Path::Combine(xdg_config_home, "PCSX2"));
		}
		else
		{
			// Use ~/PCSX2 for non-XDG, and ~/.config/PCSX2 for XDG.
			const char* home_dir = getenv("HOME");
			if (home_dir)
			{
				// ~/.config should exist, but just in case it doesn't and this is a fresh profile..
				const std::string config_dir(Path::Combine(home_dir, ".config"));
				if (!FileSystem::DirectoryExists(config_dir.c_str()))
					FileSystem::CreateDirectoryPath(config_dir.c_str(), false);

				DataRoot = Path::RealPath(Path::Combine(config_dir, "PCSX2"));
			}
		}
#elif defined(__APPLE__)
		static constexpr char MAC_DATA_DIR[] = "Library/Application Support/PCSX2";
		const char* home_dir = getenv("HOME");
		if (home_dir)
			DataRoot = Path::RealPath(Path::Combine(home_dir, MAC_DATA_DIR));
#endif
	}

	// couldn't determine the data directory, or using portable mode? fallback to portable.
	if (DataRoot.empty())
	{
#if defined(__linux__)
		// special check if we're on appimage
		// always make sure that DataRoot
		// is adjacent next to the appimage
		if (getenv("APPIMAGE"))
		{
			std::string_view appimage_path = Path::GetDirectory(getenv("APPIMAGE"));
			DataRoot = Path::RealPath(Path::Combine(appimage_path, "PCSX2"));
		}
		else
			DataRoot = Path::Combine(AppRoot, GetPortableModePath());
#else
		DataRoot = Path::Combine(AppRoot, GetPortableModePath());
#endif
	}

	// inis is always below the data root
	Settings = Path::Combine(DataRoot, "inis");

	// make sure it exists
	Console.WriteLnFmt("DataRoot Directory: {}", DataRoot);
	return (FileSystem::EnsureDirectoryExists(DataRoot.c_str(), false, error) &&
			FileSystem::EnsureDirectoryExists(Settings.c_str(), false, error));
}

void EmuFolders::SetDefaults(SettingsInterface& si)
{
	si.SetStringValue("Folders", "Bios", "bios");
	si.SetStringValue("Folders", "Snapshots", "snaps");
	si.SetStringValue("Folders", "Savestates", "sstates");
	si.SetStringValue("Folders", "MemoryCards", "memcards");
	si.SetStringValue("Folders", "Logs", "logs");
	si.SetStringValue("Folders", "Cheats", "cheats");
	si.SetStringValue("Folders", "Patches", "patches");
	si.SetStringValue("Folders", "UserResources", "resources");
	si.SetStringValue("Folders", "Cache", "cache");
	si.SetStringValue("Folders", "Textures", "textures");
	si.SetStringValue("Folders", "InputProfiles", "inputprofiles");
	si.SetStringValue("Folders", "Videos", "videos");
	si.SetStringValue("Folders", "DebuggerLayouts", "debuggerlayouts");
	si.SetStringValue("Folders", "DebuggerSettings", "debuggersettings");
}

static std::string LoadPathFromSettings(SettingsInterface& si, const std::string& root, const char* name, const char* def)
{
	std::string value = si.GetStringValue("Folders", name, def);
	if (!Path::IsAbsolute(value))
		value = Path::Combine(root, value);
	return value;
}

void EmuFolders::LoadConfig(SettingsInterface& si)
{
	Bios = LoadPathFromSettings(si, DataRoot, "Bios", "bios");
	Snapshots = LoadPathFromSettings(si, DataRoot, "Snapshots", "snaps");
	Savestates = LoadPathFromSettings(si, DataRoot, "Savestates", "sstates");
	MemoryCards = LoadPathFromSettings(si, DataRoot, "MemoryCards", "memcards");
	Logs = LoadPathFromSettings(si, DataRoot, "Logs", "logs");
	Cheats = LoadPathFromSettings(si, DataRoot, "Cheats", "cheats");
	Patches = LoadPathFromSettings(si, DataRoot, "Patches", "patches");
	Covers = LoadPathFromSettings(si, DataRoot, "Covers", "covers");
	GameSettings = LoadPathFromSettings(si, DataRoot, "GameSettings", "gamesettings");
	UserResources = LoadPathFromSettings(si, DataRoot, "UserResources", "resources");
	Cache = LoadPathFromSettings(si, DataRoot, "Cache", "cache");
	Textures = LoadPathFromSettings(si, DataRoot, "Textures", "textures");
	InputProfiles = LoadPathFromSettings(si, DataRoot, "InputProfiles", "inputprofiles");
	Videos = LoadPathFromSettings(si, DataRoot, "Videos", "videos");
	DebuggerLayouts = LoadPathFromSettings(si, Settings, "DebuggerLayouts", "debuggerlayouts");
	DebuggerSettings = LoadPathFromSettings(si, Settings, "DebuggerSettings", "debuggersettings");

	Console.WriteLn("BIOS Directory: %s", Bios.c_str());
	Console.WriteLn("Snapshots Directory: %s", Snapshots.c_str());
	Console.WriteLn("Savestates Directory: %s", Savestates.c_str());
	Console.WriteLn("MemoryCards Directory: %s", MemoryCards.c_str());
	Console.WriteLn("Logs Directory: %s", Logs.c_str());
	Console.WriteLn("Cheats Directory: %s", Cheats.c_str());
	Console.WriteLn("Patches Directory: %s", Patches.c_str());
	Console.WriteLn("Covers Directory: %s", Covers.c_str());
	Console.WriteLn("Game Settings Directory: %s", GameSettings.c_str());
	Console.WriteLn("Resources Directory: %s", Resources.c_str());
	Console.WriteLn("User Resources Directory: %s", UserResources.c_str());
	Console.WriteLn("Cache Directory: %s", Cache.c_str());
	Console.WriteLn("Textures Directory: %s", Textures.c_str());
	Console.WriteLn("Input Profile Directory: %s", InputProfiles.c_str());
	Console.WriteLn("Video Dumping Directory: %s", Videos.c_str());
	Console.WriteLn("Debugger Layouts Directory: %s", DebuggerLayouts.c_str());
	Console.WriteLn("Debugger Settings Directory: %s", DebuggerSettings.c_str());
}

bool EmuFolders::EnsureFoldersExist()
{
	bool result = FileSystem::CreateDirectoryPath(Bios.c_str(), false);
	result = FileSystem::CreateDirectoryPath(Settings.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Snapshots.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Savestates.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(MemoryCards.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Logs.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Cheats.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Patches.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Covers.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(GameSettings.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(UserResources.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Cache.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Textures.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(InputProfiles.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Videos.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(DebuggerLayouts.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(DebuggerSettings.c_str(), false) && result;
	return result;
}

std::FILE* EmuFolders::OpenLogFile(std::string_view name, const char* mode)
{
	if (name.empty())
		return nullptr;

	const std::string path(Path::Combine(Logs, name));
	return FileSystem::OpenCFile(path.c_str(), mode);
}

std::string EmuFolders::GetOverridableResourcePath(std::string_view name)
{
	std::string upath = Path::Combine(UserResources, name);
	if (FileSystem::FileExists(upath.c_str()))
	{
		if (UserResources != Resources)
			Console.Warning(fmt::format("Using user-provided resource file {}", name));
	}
	else
	{
		upath = Path::Combine(Resources, name);
	}

	return upath;
}
