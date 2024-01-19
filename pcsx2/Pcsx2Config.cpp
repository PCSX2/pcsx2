// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "common/SmallString.h"
#include "Config.h"
#include "GS.h"
#include "CDVD/CDVDcommon.h"
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
	std::string DebuggerSettings;
	std::string Bios;
	std::string Snapshots;
	std::string Savestates;
	std::string MemoryCards;
	std::string Langs;
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

	static void SetAppRoot();
	static void SetResourcesDirectory();
	static bool ShouldUsePortableMode();
	static void SetDataDirectory();
} // namespace EmuFolders

TraceFiltersEE::TraceFiltersEE()
{
	bitset = 0;
}

bool TraceFiltersEE::operator==(const TraceFiltersEE& right) const
{
	return OpEqu(bitset);
}

bool TraceFiltersEE::operator!=(const TraceFiltersEE& right) const
{
	return !this->operator==(right);
}

TraceFiltersIOP::TraceFiltersIOP()
{
	bitset = 0;
}

bool TraceFiltersIOP::operator==(const TraceFiltersIOP& right) const
{
	return OpEqu(bitset);
}

bool TraceFiltersIOP::operator!=(const TraceFiltersIOP& right) const
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

	// Retaining backwards compat of the trace log enablers isn't really important, and
	// doing each one by hand would be murder.  So let's cheat and just save it as an int:

	SettingsWrapEntry(EE.bitset);
	SettingsWrapEntry(IOP.bitset);
}

bool TraceLogFilters::operator==(const TraceLogFilters& right) const
{
	return OpEqu(Enabled) && OpEqu(EE) && OpEqu(IOP);
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

std::optional<SpeedHack> Pcsx2Config::SpeedhackOptions::ParseSpeedHackName(const std::string_view& name)
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
	return OpEqu(FPUFPCR) && OpEqu(FPUDivFPCR) && OpEqu(VU0FPCR) && OpEqu(VU1FPCR) && OpEqu(AffinityControlMode) && OpEqu(Recompiler);
}

Pcsx2Config::CpuOptions::CpuOptions()
{
	FPUFPCR = DEFAULT_FPU_FP_CONTROL_REGISTER;

	// Rounding defaults to nearest to match old behavior.
	// TODO: Make it default to the same as the rest of the FPU operations, at some point.
	FPUDivFPCR = FPControlRegister(DEFAULT_FPU_FP_CONTROL_REGISTER).SetRoundMode(FPRoundMode::Nearest);

	VU0FPCR = DEFAULT_VU_FP_CONTROL_REGISTER;
	VU1FPCR = DEFAULT_VU_FP_CONTROL_REGISTER;
	AffinityControlMode = 0;
}

void Pcsx2Config::CpuOptions::ApplySanityCheck()
{
	AffinityControlMode = std::min<u32>(AffinityControlMode, 6);

	Recompiler.ApplySanityCheck();
}

void Pcsx2Config::CpuOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/CPU");

	const auto read_fpcr = [&wrap, &CURRENT_SETTINGS_SECTION](FPControlRegister& fpcr, std::string_view prefix) {
		fpcr.SetDenormalsAreZero(wrap.EntryBitBool(CURRENT_SETTINGS_SECTION, TinyString::from_fmt("{}.DenormalsAreZero", prefix),
			fpcr.GetDenormalsAreZero(), fpcr.GetDenormalsAreZero()));
		fpcr.SetFlushToZero(wrap.EntryBitBool(CURRENT_SETTINGS_SECTION, TinyString::from_fmt("{}.DenormalsAreZero", prefix),
			fpcr.GetFlushToZero(), fpcr.GetFlushToZero()));

		uint round_mode = static_cast<uint>(fpcr.GetRoundMode());
		wrap.Entry(CURRENT_SETTINGS_SECTION, TinyString::from_fmt("{}.Roundmode", prefix), round_mode, round_mode);
		round_mode = std::min(round_mode, static_cast<uint>(FPRoundMode::MaxCount) - 1u);
		fpcr.SetRoundMode(static_cast<FPRoundMode>(round_mode));
	};

	read_fpcr(FPUFPCR, "FPU");
	read_fpcr(FPUDivFPCR, "FPUDiv");
	read_fpcr(VU0FPCR, "VU0");
	read_fpcr(VU1FPCR, "VU1");

	SettingsWrapEntry(AffinityControlMode);

	Recompiler.LoadSave(wrap);
}

const char* Pcsx2Config::GSOptions::AspectRatioNames[] = {
	"Stretch",
	"Auto 4:3/3:2",
	"4:3",
	"16:9",
	nullptr};

const char* Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames[] = {
	"Off",
	"Auto 4:3/3:2",
	"4:3",
	"16:9",
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
	DisableThreadedPresentation = false;
	SkipDuplicateFrames = false;
	OsdShowMessages = true;
	OsdShowSpeed = false;
	OsdShowFPS = false;
	OsdShowCPU = false;
	OsdShowGPU = false;
	OsdShowResolution = false;
	OsdShowGSStats = false;
	OsdShowIndicators = true;
	OsdShowSettings = false;
	OsdShowInputs = false;
	OsdShowFrameTimes = false;

	HWDownloadMode = GSHardwareDownloadMode::Enabled;
	HWSpinGPUForReadbacks = false;
	HWSpinCPUForReadbacks = false;
	GPUPaletteConversion = false;
	AutoFlushSW = true;
	PreloadFrameWithGSData = false;
	Mipmap = true;

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
	UserHacks_WildHack = false;
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

		OpEqu(VsyncEnable) &&

		OpEqu(InterlaceMode) &&
		OpEqu(LinearPresent) &&

		OpEqu(StretchY) &&
		OpEqu(Crop[0]) &&
		OpEqu(Crop[1]) &&
		OpEqu(Crop[2]) &&
		OpEqu(Crop[3]) &&

		OpEqu(OsdScale) &&

		OpEqu(Renderer) &&
		OpEqu(UpscaleMultiplier) &&

		OpEqu(HWMipmap) &&
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
		OpEqu(SaveN) &&
		OpEqu(SaveL) &&

		OpEqu(ExclusiveFullscreenControl) &&
		OpEqu(ScreenshotSize) &&
		OpEqu(ScreenshotFormat) &&
		OpEqu(ScreenshotQuality) &&

		OpEqu(CaptureContainer) &&
		OpEqu(VideoCaptureCodec) &&
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
		   OpEqu(DisableDualSourceBlend) &&
		   OpEqu(DisableFramebufferFetch) &&
		   OpEqu(DisableVertexShaderExpand) &&
		   OpEqu(DisableThreadedPresentation) &&
		   OpEqu(OverrideTextureBarriers) &&
		   OpEqu(ExclusiveFullscreenControl);
}

void Pcsx2Config::GSOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/GS");

#ifdef PCSX2_DEVBUILD
	SettingsWrapEntry(SynchronousMTGS);
#endif
	SettingsWrapEntry(VsyncQueueSize);

	wrap.EnumEntry(CURRENT_SETTINGS_SECTION, "VsyncEnable", VsyncEnable, NULL, VsyncEnable);

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

#define GSSettingInt(var) SettingsWrapBitfield(var)
#define GSSettingIntEx(var, name) SettingsWrapBitfieldEx(var, name)
#define GSSettingBool(var) SettingsWrapBitBool(var)
#define GSSettingBoolEx(var, name) SettingsWrapBitBoolEx(var, name)
#define GSSettingFloat(var) SettingsWrapEntry(var)
#define GSSettingFloatEx(var, name) SettingsWrapEntryEx(var, name)
#define GSSettingIntEnumEx(var, name) SettingsWrapIntEnumEx(var, name)
#define GSSettingString(var) SettingsWrapEntry(var)
#define GSSettingStringEx(var, name) SettingsWrapEntryEx(var, name)

	// Unfortunately, because code in the GS still reads the setting by key instead of
	// using these variables, we need to use the old names. Maybe post 2.0 we can change this.
	GSSettingBoolEx(PCRTCAntiBlur, "pcrtc_antiblur");
	GSSettingBoolEx(DisableInterlaceOffset, "disable_interlace_offset");
	GSSettingBoolEx(PCRTCOffsets, "pcrtc_offsets");
	GSSettingBoolEx(PCRTCOverscan, "pcrtc_overscan");
	GSSettingBool(IntegerScaling);
	GSSettingBool(UseDebugDevice);
	GSSettingBool(UseBlitSwapChain);
	GSSettingBool(DisableShaderCache);
	GSSettingBool(DisableDualSourceBlend);
	GSSettingBool(DisableFramebufferFetch);
	GSSettingBool(DisableVertexShaderExpand);
	GSSettingBool(DisableThreadedPresentation);
	GSSettingBool(SkipDuplicateFrames);
	GSSettingBool(OsdShowMessages);
	GSSettingBool(OsdShowSpeed);
	GSSettingBool(OsdShowFPS);
	GSSettingBool(OsdShowCPU);
	GSSettingBool(OsdShowGPU);
	GSSettingBool(OsdShowResolution);
	GSSettingBool(OsdShowGSStats);
	GSSettingBool(OsdShowIndicators);
	GSSettingBool(OsdShowSettings);
	GSSettingBool(OsdShowInputs);
	GSSettingBool(OsdShowFrameTimes);

	GSSettingBool(HWSpinGPUForReadbacks);
	GSSettingBool(HWSpinCPUForReadbacks);
	GSSettingBoolEx(GPUPaletteConversion, "paltex");
	GSSettingBoolEx(AutoFlushSW, "autoflush_sw");
	GSSettingBoolEx(PreloadFrameWithGSData, "preload_frame_with_gs_data");
	GSSettingBoolEx(Mipmap, "mipmap");
	GSSettingBoolEx(ManualUserHacks, "UserHacks");
	GSSettingBoolEx(UserHacks_AlignSpriteX, "UserHacks_align_sprite_X");
	GSSettingIntEnumEx(UserHacks_AutoFlush, "UserHacks_AutoFlushLevel");
	GSSettingBoolEx(UserHacks_CPUFBConversion, "UserHacks_CPU_FB_Conversion");
	GSSettingBoolEx(UserHacks_ReadTCOnClose, "UserHacks_ReadTCOnClose");
	GSSettingBoolEx(UserHacks_DisableDepthSupport, "UserHacks_DisableDepthSupport");
	GSSettingBoolEx(UserHacks_DisablePartialInvalidation, "UserHacks_DisablePartialInvalidation");
	GSSettingBoolEx(UserHacks_DisableSafeFeatures, "UserHacks_Disable_Safe_Features");
	GSSettingBoolEx(UserHacks_DisableRenderFixes, "UserHacks_DisableRenderFixes");
	GSSettingBoolEx(UserHacks_MergePPSprite, "UserHacks_merge_pp_sprite");
	GSSettingBoolEx(UserHacks_WildHack, "UserHacks_WildHack");
	GSSettingIntEnumEx(UserHacks_BilinearHack, "UserHacks_BilinearHack");
	GSSettingBoolEx(UserHacks_NativePaletteDraw, "UserHacks_NativePaletteDraw");
	GSSettingIntEnumEx(UserHacks_TextureInsideRt, "UserHacks_TextureInsideRt");
	GSSettingBoolEx(UserHacks_EstimateTextureRegion, "UserHacks_EstimateTextureRegion");
	GSSettingBoolEx(FXAA, "fxaa");
	GSSettingBool(ShadeBoost);
	GSSettingBoolEx(DumpGSData, "dump");
	GSSettingBoolEx(SaveRT, "save");
	GSSettingBoolEx(SaveFrame, "savef");
	GSSettingBoolEx(SaveTexture, "savet");
	GSSettingBoolEx(SaveDepth, "savez");
	GSSettingBool(DumpReplaceableTextures);
	GSSettingBool(DumpReplaceableMipmaps);
	GSSettingBool(DumpTexturesWithFMVActive);
	GSSettingBool(DumpDirectTextures);
	GSSettingBool(DumpPaletteTextures);
	GSSettingBool(LoadTextureReplacements);
	GSSettingBool(LoadTextureReplacementsAsync);
	GSSettingBool(PrecacheTextureReplacements);
	GSSettingBool(EnableVideoCapture);
	GSSettingBool(EnableVideoCaptureParameters);
	GSSettingBool(VideoCaptureAutoResolution);
	GSSettingBool(EnableAudioCapture);
	GSSettingBool(EnableAudioCaptureParameters);

	GSSettingIntEnumEx(LinearPresent, "linear_present_mode");
	GSSettingIntEnumEx(InterlaceMode, "deinterlace_mode");

	GSSettingFloat(OsdScale);

	GSSettingIntEnumEx(Renderer, "Renderer");
	GSSettingFloatEx(UpscaleMultiplier, "upscale_multiplier");

	// ~51x would the upper bound here for 32768x32768 textures, but you'll run out VRAM long before then.
	UpscaleMultiplier = std::clamp(UpscaleMultiplier, 0.5f, 50.0f);

	GSSettingIntEnumEx(HWMipmap, "mipmap_hw");
	GSSettingIntEnumEx(AccurateBlendingUnit, "accurate_blending_unit");
	GSSettingIntEnumEx(TextureFiltering, "filter");
	GSSettingIntEnumEx(TexturePreloading, "texture_preloading");
	GSSettingIntEnumEx(GSDumpCompression, "GSDumpCompression");
	GSSettingIntEnumEx(HWDownloadMode, "HWDownloadMode");
	GSSettingIntEnumEx(CASMode, "CASMode");
	GSSettingIntEx(CAS_Sharpness, "CASSharpness");
	GSSettingIntEx(Dithering, "dithering_ps2");
	GSSettingIntEx(MaxAnisotropy, "MaxAnisotropy");
	GSSettingIntEx(SWExtraThreads, "extrathreads");
	GSSettingIntEx(SWExtraThreadsHeight, "extrathreads_height");
	GSSettingIntEx(TVShader, "TVShader");
	GSSettingIntEx(SkipDrawStart, "UserHacks_SkipDraw_Start");
	GSSettingIntEx(SkipDrawEnd, "UserHacks_SkipDraw_End");
	SkipDrawEnd = std::max(SkipDrawStart, SkipDrawEnd);

	GSSettingIntEnumEx(UserHacks_HalfPixelOffset, "UserHacks_HalfPixelOffset");
	GSSettingIntEx(UserHacks_RoundSprite, "UserHacks_round_sprite_offset");
	GSSettingIntEx(UserHacks_TCOffsetX, "UserHacks_TCOffsetX");
	GSSettingIntEx(UserHacks_TCOffsetY, "UserHacks_TCOffsetY");
	GSSettingIntEx(UserHacks_CPUSpriteRenderBW, "UserHacks_CPUSpriteRenderBW");
	GSSettingIntEx(UserHacks_CPUSpriteRenderLevel, "UserHacks_CPUSpriteRenderLevel");
	GSSettingIntEx(UserHacks_CPUCLUTRender, "UserHacks_CPUCLUTRender");
	GSSettingIntEnumEx(UserHacks_GPUTargetCLUTMode, "UserHacks_GPUTargetCLUTMode");
	GSSettingIntEnumEx(TriFilter, "TriFilter");
	GSSettingIntEx(OverrideTextureBarriers, "OverrideTextureBarriers");

	GSSettingInt(ShadeBoost_Brightness);
	GSSettingInt(ShadeBoost_Contrast);
	GSSettingInt(ShadeBoost_Saturation);
	GSSettingInt(ExclusiveFullscreenControl);
	GSSettingIntEx(PNGCompressionLevel, "png_compression_level");
	GSSettingIntEx(SaveN, "saven");
	GSSettingIntEx(SaveL, "savel");

	GSSettingStringEx(CaptureContainer, "CaptureContainer");
	GSSettingStringEx(VideoCaptureCodec, "VideoCaptureCodec");
	GSSettingStringEx(VideoCaptureParameters, "VideoCaptureParameters");
	GSSettingStringEx(AudioCaptureCodec, "AudioCaptureCodec");
	GSSettingStringEx(AudioCaptureParameters, "AudioCaptureParameters");
	GSSettingIntEx(VideoCaptureBitrate, "VideoCaptureBitrate");
	GSSettingIntEx(VideoCaptureWidth, "VideoCaptureWidth");
	GSSettingIntEx(VideoCaptureHeight, "VideoCaptureHeight");
	GSSettingIntEx(AudioCaptureBitrate, "AudioCaptureBitrate");

	GSSettingString(Adapter);
	GSSettingString(HWDumpDirectory);
	if (!HWDumpDirectory.empty() && !Path::IsAbsolute(HWDumpDirectory))
		HWDumpDirectory = Path::Combine(EmuFolders::DataRoot, HWDumpDirectory);
	GSSettingString(SWDumpDirectory);
	if (!SWDumpDirectory.empty() && !Path::IsAbsolute(SWDumpDirectory))
		SWDumpDirectory = Path::Combine(EmuFolders::DataRoot, SWDumpDirectory);

#undef GSSettingInt
#undef GSSettingIntEx
#undef GSSettingBool
#undef GSSettingBoolEx
#undef GSSettingFloat
#undef GSSettingEnumEx
#undef GSSettingIntEnumEx
#undef GSSettingString
#undef GSSettingStringEx

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
	UserHacks_WildHack = false;
	UserHacks_NativePaletteDraw = false;
	UserHacks_DisableSafeFeatures = false;
	UserHacks_DisableRenderFixes = false;
	UserHacks_HalfPixelOffset = GSHalfPixelOffset::Off;
	UserHacks_RoundSprite = 0;
	UserHacks_AutoFlush = GSHWAutoFlushLevel::Disabled;
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
	UserHacks_WildHack = false;
	UserHacks_BilinearHack = GSBilinearDirtyMode::Automatic;
	UserHacks_NativePaletteDraw = false;
	UserHacks_HalfPixelOffset = GSHalfPixelOffset::Off;
	UserHacks_RoundSprite = 0;
	UserHacks_TCOffsetX = 0;
	UserHacks_TCOffsetY = 0;
}

bool Pcsx2Config::GSOptions::UseHardwareRenderer() const
{
	return (Renderer != GSRendererType::Null && Renderer != GSRendererType::SW);
}

Pcsx2Config::SPU2Options::SPU2Options()
{
	bitset = 0;
	OutputModule = "cubeb";
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
		SettingsWrapBitBoolEx(MsgOverruns, "Show_Messages_Overruns");
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
			MsgOverruns = false;
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
		SettingsWrapSection("SPU2/Mixing");

		SettingsWrapEntry(FinalVolume);
	}

	{
		SettingsWrapSection("SPU2/Output");

		SettingsWrapEntry(OutputModule);
		SettingsWrapEntry(BackendName);
		SettingsWrapEntry(DeviceName);
		SettingsWrapEntry(Latency);
		SettingsWrapEntry(OutputLatency);
		SettingsWrapBitBool(OutputLatencyMinimal);
		SynchMode = static_cast<SynchronizationMode>(wrap.EntryBitfield(CURRENT_SETTINGS_SECTION, "SynchMode", static_cast<int>(SynchMode), static_cast<int>(SynchMode)));
		SettingsWrapEntry(SpeakerConfiguration);
		SettingsWrapEntry(DplDecodingLevel);
	}

	// clampy clamp
}

bool Pcsx2Config::SPU2Options::operator!=(const SPU2Options& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::SPU2Options::operator==(const SPU2Options& right) const
{
	return OpEqu(bitset) &&

		   OpEqu(SynchMode) &&

		   OpEqu(FinalVolume) &&
		   OpEqu(Latency) &&
		   OpEqu(OutputLatency) &&
		   OpEqu(SpeakerConfiguration) &&
		   OpEqu(DplDecodingLevel) &&

		   OpEqu(SequenceLenMS) &&
		   OpEqu(SeekWindowMS) &&
		   OpEqu(OverlapMS) &&

		   OpEqu(OutputModule) &&
		   OpEqu(BackendName) &&
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


Pcsx2Config::DebugOptions::DebugOptions()
{
	ShowDebuggerOnStart = false;
	AlignMemoryWindowStart = true;
	FontWidth = 8;
	FontHeight = 12;
	WindowWidth = 0;
	WindowHeight = 0;
	MemoryViewBytesPerRow = 16;
}

void Pcsx2Config::DebugOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/Debugger");

	SettingsWrapBitBool(ShowDebuggerOnStart);
	SettingsWrapBitBool(AlignMemoryWindowStart);
	SettingsWrapBitfield(FontWidth);
	SettingsWrapBitfield(FontHeight);
	SettingsWrapBitfield(WindowWidth);
	SettingsWrapBitfield(WindowHeight);
	SettingsWrapBitfield(MemoryViewBytesPerRow);
}

bool Pcsx2Config::DebugOptions::operator!=(const DebugOptions& right) const
{
	return !this->operator==(right);
}

bool Pcsx2Config::DebugOptions::operator==(const DebugOptions& right) const
{
	return OpEqu(bitset) && OpEqu(FontWidth) && OpEqu(FontHeight) && OpEqu(WindowWidth) && OpEqu(WindowHeight) && OpEqu(MemoryViewBytesPerRow);
}

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

	FrameLimitEnable = true;
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
	//SettingsWrapBitBool(FrameLimitEnable);
	//SettingsWrapBitBool(SyncToHostRefreshRate);
	FrameLimitEnable = wrap.EntryBitBool("EmuCore/GS", "FrameLimitEnable", FrameLimitEnable, FrameLimitEnable);
	SyncToHostRefreshRate = wrap.EntryBitBool("EmuCore/GS", "SyncToHostRefreshRate", SyncToHostRefreshRate, SyncToHostRefreshRate);
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
	Overlays = true;
}

void Pcsx2Config::AchievementsOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("Achievements");

	SettingsWrapBitBool(Enabled);
	SettingsWrapBitBoolEx(HardcoreMode, "ChallengeMode");
	SettingsWrapBitBool(EncoreMode);
	SettingsWrapBitBool(SpectatorMode);
	SettingsWrapBitBool(UnofficialTestMode);
	SettingsWrapBitBool(Notifications);
	SettingsWrapBitBool(LeaderboardNotifications);
	SettingsWrapBitBool(SoundEffects);
	SettingsWrapBitBool(Overlays);
	SettingsWrapEntry(NotificationsDuration);
	SettingsWrapEntry(LeaderboardsDuration);

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
	EnablePerGameSettings = true;
	EnableRecordingTools = true;
	EnableGameFixes = true;
	InhibitScreensaver = true;
	BackupSavestate = true;
	SavestateZstdCompression = true;

#ifdef _WIN32
	McdCompressNTFS = true;
#endif

	WarnAboutUnsafeSettings = true;

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
}

void Pcsx2Config::LoadSaveCore(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore");

	SettingsWrapBitBool(CdvdVerboseReads);
	SettingsWrapBitBool(CdvdDumpBlocks);
	SettingsWrapBitBool(CdvdShareWrite);
	SettingsWrapBitBool(EnablePatches);
	SettingsWrapBitBool(EnableCheats);
	SettingsWrapBitBool(EnablePINE);
	SettingsWrapBitBool(EnableWideScreenPatches);
	SettingsWrapBitBool(EnableNoInterlacingPatches);
	SettingsWrapBitBool(EnableFastBoot);
	SettingsWrapBitBool(EnableFastBootFastForward);
	SettingsWrapBitBool(EnablePerGameSettings);
	SettingsWrapBitBool(EnableRecordingTools);
	SettingsWrapBitBool(EnableGameFixes);
	SettingsWrapBitBool(SaveStateOnShutdown);
	SettingsWrapBitBool(EnableDiscordPresence);
	SettingsWrapBitBool(InhibitScreensaver);
	SettingsWrapBitBool(HostFs);

	SettingsWrapBitBool(BackupSavestate);
	SettingsWrapBitBool(SavestateZstdCompression);
	SettingsWrapBitBool(McdFolderAutoManage);

	SettingsWrapBitBool(WarnAboutUnsafeSettings);

	// Process various sub-components:

	Speedhacks.LoadSave(wrap);
	Cpu.LoadSave(wrap);
	GS.LoadSave(wrap);
	SPU2.LoadSave(wrap);
	DEV9.LoadSave(wrap);
	Gamefixes.LoadSave(wrap);
	Profiler.LoadSave(wrap);

	Debugger.LoadSave(wrap);
	Trace.LoadSave(wrap);

	Achievements.LoadSave(wrap);

	SettingsWrapEntry(GzipIsoIndexTemplate);
	SettingsWrapEntry(PINESlot);

	// For now, this in the derived config for backwards ini compatibility.
	SettingsWrapEntryEx(CurrentBlockdump, "BlockDumpSaveDirectory");

	BaseFilenames.LoadSave(wrap);
	EmulationSpeed.LoadSave(wrap);
	LoadSaveMemcards(wrap);

#ifdef _WIN32
	SettingsWrapEntry(McdCompressNTFS);
#endif

	if (wrap.IsLoading())
	{
		CurrentAspectRatio = GS.AspectRatio;
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

bool EmuFolders::InitializeCriticalFolders()
{
	SetAppRoot();
	SetResourcesDirectory();
	SetDataDirectory();

	// logging of directories in case something goes wrong super early
	Console.WriteLn("AppRoot Directory: %s", AppRoot.c_str());
	Console.WriteLn("DataRoot Directory: %s", DataRoot.c_str());
	Console.WriteLn("Resources Directory: %s", Resources.c_str());

	// allow SetDataDirectory() to change settings directory (if we want to split config later on)
	if (Settings.empty())
	{
		Settings = Path::Combine(DataRoot, "inis");

		// Create settings directory if it doesn't exist. If we're not using portable mode, it won't.
		if (!FileSystem::DirectoryExists(Settings.c_str()))
			FileSystem::CreateDirectoryPath(Settings.c_str(), false);
	}

	// the resources directory should exist, bail out if not
	if (!FileSystem::DirectoryExists(Resources.c_str()))
	{
		Console.Error("Resources directory is missing.");
		return false;
	}

	return true;
}

void EmuFolders::SetAppRoot()
{
	const std::string program_path = FileSystem::GetProgramPath();
	Console.WriteLn("Program Path: %s", program_path.c_str());

	AppRoot = Path::Canonicalize(Path::GetDirectory(program_path));
}

void EmuFolders::SetResourcesDirectory()
{
#ifndef __APPLE__
	// On Windows/Linux, these are in the binary directory.
	Resources = Path::Combine(AppRoot, "resources");
#else
	// On macOS, this is in the bundle resources directory.
	Resources = Path::Canonicalize(Path::Combine(AppRoot, "../Resources"));
#endif
}

bool EmuFolders::ShouldUsePortableMode()
{
	// Check whether portable.ini exists in the program directory.
	return FileSystem::FileExists(Path::Combine(AppRoot, "portable.ini").c_str());
}

void EmuFolders::SetDataDirectory()
{
	if (ShouldUsePortableMode())
	{
		DataRoot = AppRoot;
		return;
	}

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

	// make sure it exists
	if (!DataRoot.empty() && !FileSystem::DirectoryExists(DataRoot.c_str()))
	{
		// we're in trouble if we fail to create this directory... but try to hobble on with portable
		if (!FileSystem::CreateDirectoryPath(DataRoot.c_str(), false))
			DataRoot.clear();
	}

	// couldn't determine the data directory? fallback to portable.
	if (DataRoot.empty())
		DataRoot = AppRoot;
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
	result = FileSystem::CreateDirectoryPath(DebuggerSettings.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Cache.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Textures.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(InputProfiles.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Videos.c_str(), false) && result;
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
