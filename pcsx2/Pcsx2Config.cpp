/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SettingsInterface.h"
#include "common/SettingsWrapper.h"
#include "common/StringUtil.h"
#include "Config.h"
#include "GS.h"
#include "HostDisplay.h"
#include "CDVD/CDVDaccess.h"
#include "MemoryCardFile.h"

#ifndef PCSX2_CORE
#include "gui/AppConfig.h"
#include "GS/GS.h"
#endif

namespace EmuFolders
{
	std::string AppRoot;
	std::string DataRoot;
	std::string Settings;
	std::string Bios;
	std::string Snapshots;
	std::string Savestates;
	std::string MemoryCards;
	std::string Langs;
	std::string Logs;
	std::string Cheats;
	std::string CheatsWS;
	std::string CheatsNI;
	std::string Resources;
	std::string Cache;
	std::string Covers;
	std::string GameSettings;
	std::string Textures;
} // namespace EmuFolders

void TraceLogFilters::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/TraceLog");

	SettingsWrapEntry(Enabled);

	// Retaining backwards compat of the trace log enablers isn't really important, and
	// doing each one by hand would be murder.  So let's cheat and just save it as an int:

	SettingsWrapEntry(EE.bitset);
	SettingsWrapEntry(IOP.bitset);
}

const char* const tbl_SpeedhackNames[] =
{
	"mvuFlag",
	"InstantVU1",
	"MTVU"
};

const char* EnumToString(SpeedhackId id)
{
	return tbl_SpeedhackNames[id];
}

void Pcsx2Config::SpeedhackOptions::Set(SpeedhackId id, bool enabled)
{
	pxAssert(EnumIsValid(id));
	switch (id)
	{
		case Speedhack_mvuFlag:
			vuFlagHack = enabled;
			break;
		case Speedhack_InstantVU1:
			vu1Instant = enabled;
			break;
		case Speedhack_MTVU:
			vuThread = enabled;
			break;
        jNO_DEFAULT;
	}
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

	// vu and fpu clamping default to standard overflow.
	vuOverflow = true;
	//vuExtraOverflow = false;
	//vuSignOverflow = false;
	//vuUnderflow = false;

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

	if (vuExtraOverflow)
		vuIsOk = vuIsOk && vuOverflow;
	if (vuSignOverflow)
		vuIsOk = vuIsOk && vuExtraOverflow;

	if (!vuIsOk)
	{
		// Values are wonky; assume the defaults.
		vuOverflow = RecompilerOptions().vuOverflow;
		vuExtraOverflow = RecompilerOptions().vuExtraOverflow;
		vuSignOverflow = RecompilerOptions().vuSignOverflow;
		vuUnderflow = RecompilerOptions().vuUnderflow;
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

	SettingsWrapBitBool(vuOverflow);
	SettingsWrapBitBool(vuExtraOverflow);
	SettingsWrapBitBool(vuSignOverflow);
	SettingsWrapBitBool(vuUnderflow);

	SettingsWrapBitBool(fpuOverflow);
	SettingsWrapBitBool(fpuExtraOverflow);
	SettingsWrapBitBool(fpuFullMode);

	SettingsWrapBitBool(StackFrameChecks);
	SettingsWrapBitBool(PreBlockCheckEE);
	SettingsWrapBitBool(PreBlockCheckIOP);
}

bool Pcsx2Config::CpuOptions::CpusChanged(const CpuOptions& right) const
{
	return (Recompiler.EnableEE != right.Recompiler.EnableEE ||
			Recompiler.EnableIOP != right.Recompiler.EnableIOP ||
			Recompiler.EnableVU0 != right.Recompiler.EnableVU0 ||
			Recompiler.EnableVU1 != right.Recompiler.EnableVU1);
}

Pcsx2Config::CpuOptions::CpuOptions()
{
	sseMXCSR.bitmask = DEFAULT_sseMXCSR;
	sseVUMXCSR.bitmask = DEFAULT_sseVUMXCSR;
}

void Pcsx2Config::CpuOptions::ApplySanityCheck()
{
	sseMXCSR.ClearExceptionFlags().DisableExceptions();
	sseVUMXCSR.ClearExceptionFlags().DisableExceptions();

	Recompiler.ApplySanityCheck();
}

void Pcsx2Config::CpuOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/CPU");

	SettingsWrapBitBoolEx(sseMXCSR.DenormalsAreZero, "FPU.DenormalsAreZero");
	SettingsWrapBitBoolEx(sseMXCSR.FlushToZero, "FPU.FlushToZero");
	SettingsWrapBitfieldEx(sseMXCSR.RoundingControl, "FPU.Roundmode");

	SettingsWrapBitBoolEx(sseVUMXCSR.DenormalsAreZero, "VU.DenormalsAreZero");
	SettingsWrapBitBoolEx(sseVUMXCSR.FlushToZero, "VU.FlushToZero");
	SettingsWrapBitfieldEx(sseVUMXCSR.RoundingControl, "VU.Roundmode");

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

const char* Pcsx2Config::GSOptions::GetRendererName(GSRendererType type)
{
	switch (type)
	{
		case GSRendererType::Auto:  return "Auto";
		case GSRendererType::DX11:  return "Direct3D 11";
		case GSRendererType::DX12:  return "Direct3D 12";
		case GSRendererType::Metal: return "Metal";
		case GSRendererType::OGL:   return "OpenGL";
		case GSRendererType::VK:    return "Vulkan";
		case GSRendererType::SW:    return "Software";
		case GSRendererType::Null:  return "Null";
		default:                    return "";
	}
}

Pcsx2Config::GSOptions::GSOptions()
{
	bitset = 0;

	DisableInterlaceOffset = false;
	PCRTCOffsets = false;
	IntegerScaling = false;
	LinearPresent = true;
	UseDebugDevice = false;
	UseBlitSwapChain = false;
	DisableShaderCache = false;
	DisableFramebufferFetch = false;
	ThreadedPresentation = false;
	SkipDuplicateFrames = false;
	OsdShowMessages = true;
	OsdShowSpeed = false;
	OsdShowFPS = false;
	OsdShowCPU = false;
	OsdShowGPU = false;
	OsdShowResolution = false;
	OsdShowGSStats = false;
	OsdShowIndicators = true;

	HWDisableReadbacks = false;
	AccurateDATE = true;
	GPUPaletteConversion = false;
	ConservativeFramebuffer = true;
	AutoFlushSW = true;
	PreloadFrameWithGSData = false;
	WrapGSMem = false;
	Mipmap = true;
	AA1 = true;
	PointListPalette = false;

	ManualUserHacks = false;
	UserHacks_AlignSpriteX = false;
	UserHacks_AutoFlush = false;
	UserHacks_CPUFBConversion = false;
	UserHacks_DisableDepthSupport = false;
	UserHacks_DisablePartialInvalidation = false;
	UserHacks_DisableSafeFeatures = false;
	UserHacks_MergePPSprite = false;
	UserHacks_WildHack = false;

	DumpReplaceableTextures = false;
	DumpReplaceableMipmaps = false;
	DumpTexturesWithFMVActive = false;
	DumpDirectTextures = true;
	DumpPaletteTextures = true;
	LoadTextureReplacements = false;
	LoadTextureReplacementsAsync = true;
	PrecacheTextureReplacements = false;

	ShaderFX_Conf = "shaders/GS_FX_Settings.ini";
	ShaderFX_GLSL = "shaders/GS.fx";
}

bool Pcsx2Config::GSOptions::operator==(const GSOptions& right) const
{
	return (
		OpEqu(SynchronousMTGS) &&
		OpEqu(VsyncQueueSize) &&

		OpEqu(FrameSkipEnable) &&
		OpEqu(FrameLimitEnable) &&

		OpEqu(FramesToDraw) &&
		OpEqu(FramesToSkip) &&

		OpEqu(LimitScalar) &&
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

		OpEqu(Zoom) &&
		OpEqu(StretchY) &&
		OpEqu(OffsetX) &&
		OpEqu(OffsetY) &&
		OpEqu(OsdScale) &&

		OpEqu(Renderer) &&
		OpEqu(UpscaleMultiplier) &&

		OpEqu(HWMipmap) &&
		OpEqu(AccurateBlendingUnit) &&
		OpEqu(CRCHack) &&
		OpEqu(TextureFiltering) &&
		OpEqu(TexturePreloading) &&
		OpEqu(GSDumpCompression) &&
		OpEqu(Dithering) &&
		OpEqu(MaxAnisotropy) &&
		OpEqu(SWExtraThreads) &&
		OpEqu(SWExtraThreadsHeight) &&
		OpEqu(TVShader) &&
		OpEqu(SkipDrawEnd) &&
		OpEqu(SkipDrawStart) &&

		OpEqu(UserHacks_HalfBottomOverride) &&
		OpEqu(UserHacks_HalfPixelOffset) &&
		OpEqu(UserHacks_RoundSprite) &&
		OpEqu(UserHacks_TCOffsetX) &&
		OpEqu(UserHacks_TCOffsetY) &&
		OpEqu(UserHacks_TriFilter) &&
		OpEqu(OverrideTextureBarriers) &&
		OpEqu(OverrideGeometryShaders) &&

		OpEqu(ShadeBoost_Brightness) &&
		OpEqu(ShadeBoost_Contrast) &&
		OpEqu(ShadeBoost_Saturation) &&
		OpEqu(SaveN) &&
		OpEqu(SaveL) &&
		OpEqu(Adapter) &&
		OpEqu(ShaderFX_Conf) &&
		OpEqu(ShaderFX_GLSL));
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
		   OpEqu(ThreadedPresentation) &&
		   OpEqu(OverrideTextureBarriers) &&
		   OpEqu(OverrideGeometryShaders);
}

void Pcsx2Config::GSOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/GS");

#ifdef PCSX2_DEVBUILD
	SettingsWrapEntry(SynchronousMTGS);
#endif
	SettingsWrapEntry(VsyncQueueSize);

	SettingsWrapEntry(FrameLimitEnable);
	SettingsWrapEntry(FrameSkipEnable);
	wrap.EnumEntry(CURRENT_SETTINGS_SECTION, "VsyncEnable", VsyncEnable, NULL, VsyncEnable);

	// LimitScalar is set at runtime.
	SettingsWrapEntry(FramerateNTSC);
	SettingsWrapEntry(FrameratePAL);

	SettingsWrapEntry(FramesToDraw);
	SettingsWrapEntry(FramesToSkip);

#ifdef PCSX2_CORE
	// These are loaded from GSWindow in wx.
	SettingsWrapEnumEx(AspectRatio, "AspectRatio", AspectRatioNames);
	SettingsWrapEnumEx(FMVAspectRatioSwitch, "FMVAspectRatioSwitch", FMVAspectRatioSwitchNames);

	SettingsWrapEntry(Zoom);
	SettingsWrapEntry(StretchY);
	SettingsWrapEntry(OffsetX);
	SettingsWrapEntry(OffsetY);
#endif

#ifndef PCSX2_CORE
	if (wrap.IsLoading())
		ReloadIniSettings();
#else
	LoadSaveIniSettings(wrap);
#endif
}

#ifdef PCSX2_CORE
void Pcsx2Config::GSOptions::LoadSaveIniSettings(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/GS");

#define GSSettingInt(var) SettingsWrapBitfield(var)
#define GSSettingIntEx(var, name) SettingsWrapBitfieldEx(var, name)
#define GSSettingBool(var) SettingsWrapBitBool(var)
#define GSSettingBoolEx(var, name) SettingsWrapBitBoolEx(var, name)
#define GSSettingFloat(var) SettingsWrapBitfield(var)
#define GSSettingIntEnumEx(var, name) SettingsWrapIntEnumEx(var, name)
#define GSSettingString(var) SettingsWrapEntry(var)
#define GSSettingStringEx(var, name) SettingsWrapEntryEx(var, name)
#else
void Pcsx2Config::GSOptions::ReloadIniSettings()
{
	// ensure theApp is loaded.
	GSinitConfig();

#define GSSettingInt(var) var = theApp.GetConfigI(#var)
#define GSSettingIntEx(var, name) var = theApp.GetConfigI(name)
#define GSSettingBool(var) var = theApp.GetConfigB(#var)
#define GSSettingBoolEx(var, name) var = theApp.GetConfigB(name)
#define GSSettingFloat(var) var = static_cast<double>(theApp.GetConfigI(#var))
#define GSSettingIntEnumEx(var, name) var = static_cast<decltype(var)>(theApp.GetConfigI(name))
#define GSSettingString(var) var = theApp.GetConfigS(#var)
#define GSSettingStringEx(var, name) var = theApp.GetConfigS(name)
#endif

	// Unfortunately, because code in the GS still reads the setting by key instead of
	// using these variables, we need to use the old names. Maybe post 2.0 we can change this.
	GSSettingBoolEx(DisableInterlaceOffset, "disable_interlace_offset");
	GSSettingBoolEx(PCRTCOffsets, "pcrtc_offsets");
	GSSettingBool(IntegerScaling);
	GSSettingBoolEx(LinearPresent, "linear_present");
	GSSettingBool(UseDebugDevice);
	GSSettingBool(UseBlitSwapChain);
	GSSettingBoolEx(DisableShaderCache, "disable_shader_cache");
	GSSettingBool(DisableDualSourceBlend);
	GSSettingBool(DisableFramebufferFetch);
	GSSettingBool(ThreadedPresentation);
	GSSettingBool(SkipDuplicateFrames);
	GSSettingBool(OsdShowMessages);
	GSSettingBool(OsdShowSpeed);
	GSSettingBool(OsdShowFPS);
	GSSettingBool(OsdShowCPU);
	GSSettingBool(OsdShowGPU);
	GSSettingBool(OsdShowResolution);
	GSSettingBool(OsdShowGSStats);
	GSSettingBool(OsdShowIndicators);

	GSSettingBool(HWDisableReadbacks);
	GSSettingBoolEx(AccurateDATE, "accurate_date");
	GSSettingBoolEx(GPUPaletteConversion, "paltex");
	GSSettingBoolEx(ConservativeFramebuffer, "conservative_framebuffer");
	GSSettingBoolEx(AutoFlushSW, "autoflush_sw");
	GSSettingBoolEx(PreloadFrameWithGSData, "preload_frame_with_gs_data");
	GSSettingBoolEx(WrapGSMem, "wrap_gs_mem");
	GSSettingBoolEx(Mipmap, "mipmap");
	GSSettingBoolEx(AA1, "aa1");
	GSSettingBoolEx(ManualUserHacks, "UserHacks");
	GSSettingBoolEx(UserHacks_AlignSpriteX, "UserHacks_align_sprite_X");
	GSSettingBoolEx(UserHacks_AutoFlush, "UserHacks_AutoFlush");
	GSSettingBoolEx(UserHacks_CPUFBConversion, "UserHacks_CPU_FB_Conversion");
	GSSettingBoolEx(UserHacks_DisableDepthSupport, "UserHacks_DisableDepthSupport");
	GSSettingBoolEx(UserHacks_DisablePartialInvalidation, "UserHacks_DisablePartialInvalidation");
	GSSettingBoolEx(UserHacks_DisableSafeFeatures, "UserHacks_Disable_Safe_Features");
	GSSettingBoolEx(UserHacks_MergePPSprite, "UserHacks_merge_pp_sprite");
	GSSettingBoolEx(UserHacks_WildHack, "UserHacks_WildHack");
	GSSettingBoolEx(UserHacks_TextureInsideRt, "UserHacks_TextureInsideRt");
	GSSettingBoolEx(FXAA, "fxaa");
	GSSettingBool(ShadeBoost);
	GSSettingBoolEx(ShaderFX, "shaderfx");
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

	GSSettingIntEnumEx(InterlaceMode, "deinterlace");

	GSSettingFloat(OsdScale);

	GSSettingIntEnumEx(Renderer, "Renderer");
	GSSettingIntEx(UpscaleMultiplier, "upscale_multiplier");
	UpscaleMultiplier = std::clamp(UpscaleMultiplier, 1u, 8u);

	GSSettingIntEnumEx(HWMipmap, "mipmap_hw");
	GSSettingIntEnumEx(AccurateBlendingUnit, "accurate_blending_unit");
	GSSettingIntEnumEx(CRCHack, "crc_hack_level");
	GSSettingIntEnumEx(TextureFiltering, "filter");
	GSSettingIntEnumEx(TexturePreloading, "texture_preloading");
	GSSettingIntEnumEx(GSDumpCompression, "GSDumpCompression");
	GSSettingIntEx(Dithering, "dithering_ps2");
	GSSettingIntEx(MaxAnisotropy, "MaxAnisotropy");
	GSSettingIntEx(SWExtraThreads, "extrathreads");
	GSSettingIntEx(SWExtraThreadsHeight, "extrathreads_height");
	GSSettingIntEx(TVShader, "TVShader");
	GSSettingIntEx(SkipDrawStart, "UserHacks_SkipDraw_Start");
	GSSettingIntEx(SkipDrawEnd, "UserHacks_SkipDraw_End");
	SkipDrawEnd = std::max(SkipDrawStart, SkipDrawEnd);

	GSSettingIntEx(UserHacks_HalfBottomOverride, "UserHacks_Half_Bottom_Override");
	GSSettingIntEx(UserHacks_HalfPixelOffset, "UserHacks_HalfPixelOffset");
	GSSettingIntEx(UserHacks_RoundSprite, "UserHacks_round_sprite_offset");
	GSSettingIntEx(UserHacks_TCOffsetX, "UserHacks_TCOffsetX");
	GSSettingIntEx(UserHacks_TCOffsetY, "UserHacks_TCOffsetY");
	GSSettingIntEnumEx(UserHacks_TriFilter, "UserHacks_TriFilter");
	GSSettingIntEx(OverrideTextureBarriers, "OverrideTextureBarriers");
	GSSettingIntEx(OverrideGeometryShaders, "OverrideGeometryShaders");

	GSSettingInt(ShadeBoost_Brightness);
	GSSettingInt(ShadeBoost_Contrast);
	GSSettingInt(ShadeBoost_Saturation);
	GSSettingIntEx(SaveN, "saven");
	GSSettingIntEx(SaveL, "savel");

	GSSettingString(Adapter);
	GSSettingStringEx(ShaderFX_Conf, "shaderfx_conf");
	GSSettingStringEx(ShaderFX_GLSL, "shaderfx_glsl");

#undef GSSettingInt
#undef GSSettingIntEx
#undef GSSettingBool
#undef GSSettingBoolEx
#undef GSSettingFloat
#undef GSSettingEnumEx
#undef GSSettingIntEnumEx
#undef GSSettingString
#undef GSSettingStringEx
}

void Pcsx2Config::GSOptions::MaskUserHacks()
{
	if (ManualUserHacks)
		return;

	UserHacks_AlignSpriteX = false;
	UserHacks_MergePPSprite = false;
	UserHacks_WildHack = false;
	UserHacks_DisableSafeFeatures = false;
	UserHacks_HalfBottomOverride = -1;
	UserHacks_HalfPixelOffset = 0;
	UserHacks_RoundSprite = 0;
	UserHacks_AutoFlush = false;
	PreloadFrameWithGSData = false;
	WrapGSMem = false;
	UserHacks_DisablePartialInvalidation = false;
	UserHacks_DisableDepthSupport = false;
	UserHacks_CPUFBConversion = false;
	UserHacks_TextureInsideRt = false;
	UserHacks_TCOffsetX = 0;
	UserHacks_TCOffsetY = 0;
	SkipDrawStart = 0;
	SkipDrawEnd = 0;

	// in wx, we put trilinear filtering behind user hacks, but not in qt.
#ifndef PCSX2_CORE
	UserHacks_TriFilter = TriFiltering::Automatic;
#endif
}

void Pcsx2Config::GSOptions::MaskUpscalingHacks()
{
	if (UpscaleMultiplier == 1 || ManualUserHacks)
		return;

	UserHacks_AlignSpriteX = false;
	UserHacks_MergePPSprite = false;
	UserHacks_HalfPixelOffset = 0;
	UserHacks_RoundSprite = 0;
}

bool Pcsx2Config::GSOptions::UseHardwareRenderer() const
{
	return (Renderer != GSRendererType::Null && Renderer != GSRendererType::SW);
}

VsyncMode Pcsx2Config::GetEffectiveVsyncMode() const
{
	if (GS.LimitScalar != 1.0)
	{
		Console.WriteLn("Vsync is OFF");
		return VsyncMode::Off;
	}

	Console.WriteLn("Vsync is %s", GS.VsyncEnable == VsyncMode::Off ? "OFF" : (GS.VsyncEnable == VsyncMode::Adaptive ? "ADAPTIVE" : "ON"));
	return GS.VsyncEnable;
}

Pcsx2Config::SPU2Options::SPU2Options()
{
	OutputModule = "cubeb";
}

void Pcsx2Config::SPU2Options::LoadSave(SettingsWrapper& wrap)
{
	{
		SettingsWrapSection("SPU2/Mixing");

		Interpolation = static_cast<InterpolationMode>(wrap.EntryBitfield(CURRENT_SETTINGS_SECTION, "Interpolation", static_cast<int>(Interpolation), static_cast<int>(Interpolation)));
		SettingsWrapEntry(FinalVolume);

		SettingsWrapEntry(VolumeAdjustC);
		SettingsWrapEntry(VolumeAdjustFL);
		SettingsWrapEntry(VolumeAdjustFR);
		SettingsWrapEntry(VolumeAdjustBL);
		SettingsWrapEntry(VolumeAdjustBR);
		SettingsWrapEntry(VolumeAdjustSL);
		SettingsWrapEntry(VolumeAdjustSR);
		SettingsWrapEntry(VolumeAdjustLFE);
	}

	{
		SettingsWrapSection("SPU2/Output");

		SettingsWrapEntry(OutputModule);
		SettingsWrapEntry(Latency);
		SynchMode = static_cast<SynchronizationMode>(wrap.EntryBitfield(CURRENT_SETTINGS_SECTION, "SynchMode", static_cast<int>(SynchMode), static_cast<int>(SynchMode)));
		SettingsWrapEntry(SpeakerConfiguration);
	}
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

#ifdef PCSX2_CORE
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
#endif

	{
		SettingsWrapSection("DEV9/Hdd");
		SettingsWrapEntry(HddEnable);
		SettingsWrapEntry(HddFile);
		SettingsWrapEntry(HddSizeSectors);
	}
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

static const char* const tbl_GamefixNames[] =
{
	"FpuMul",
	"FpuNegDiv",
	"GoemonTlb",
	"SoftwareRendererFMV",
	"SkipMPEG",
	"OPHFlag",
	"EETiming",
	"DMABusy",
	"GIFFIFO",
	"VIFFIFO",
	"VIF1Stall",
	"VuAddSub",
	"Ibit",
	"VUSync",
	"VUOverflow",
	"XGKick"
};

const char* EnumToString(GamefixId id)
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
	pxAssert(EnumIsValid(id));
	switch (id)
	{
		case Fix_VuAddSub:            VuAddSubHack            = enabled; break;
		case Fix_FpuMultiply:         FpuMulHack              = enabled; break;
		case Fix_FpuNegDiv:           FpuNegDivHack           = enabled; break;
		case Fix_XGKick:              XgKickHack              = enabled; break;
		case Fix_EETiming:            EETimingHack            = enabled; break;
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
		jNO_DEFAULT;
	}
}

bool Pcsx2Config::GamefixOptions::Get(GamefixId id) const
{
	pxAssert(EnumIsValid(id));
	switch (id)
	{
		case Fix_VuAddSub:            return VuAddSubHack;
		case Fix_FpuMultiply:         return FpuMulHack;
		case Fix_FpuNegDiv:           return FpuNegDivHack;
		case Fix_XGKick:              return XgKickHack;
		case Fix_EETiming:            return EETimingHack;
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
		jNO_DEFAULT;
	}
	return false; // unreachable, but we still need to suppress warnings >_<
}

void Pcsx2Config::GamefixOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("EmuCore/Gamefixes");

	SettingsWrapBitBool(VuAddSubHack);
	SettingsWrapBitBool(FpuMulHack);
	SettingsWrapBitBool(FpuNegDivHack);
	SettingsWrapBitBool(XgKickHack);
	SettingsWrapBitBool(EETimingHack);
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

Pcsx2Config::FilenameOptions::FilenameOptions()
{
}

void Pcsx2Config::FilenameOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("Filenames");

	wrap.Entry(CURRENT_SETTINGS_SECTION, "BIOS", Bios, Bios);
}

void Pcsx2Config::FramerateOptions::SanityCheck()
{
	// Ensure Conformation of various options...

	NominalScalar = std::clamp(NominalScalar, 0.05, 10.0);
	TurboScalar = std::clamp(TurboScalar, 0.05, 10.0);
	SlomoScalar = std::clamp(SlomoScalar, 0.05, 10.0);
}

void Pcsx2Config::FramerateOptions::LoadSave(SettingsWrapper& wrap)
{
	SettingsWrapSection("Framerate");

	SettingsWrapEntry(NominalScalar);
	SettingsWrapEntry(TurboScalar);
	SettingsWrapEntry(SlomoScalar);

	SettingsWrapEntry(SkipOnLimit);
	SettingsWrapEntry(SkipOnTurbo);
}

Pcsx2Config::Pcsx2Config()
{
	bitset = 0;
	// Set defaults for fresh installs / reset settings
	McdEnableEjection = true;
	McdFolderAutoManage = true;
	EnablePatches = true;
	EnableRecordingTools = true;
#ifdef PCSX2_CORE
	EnableGameFixes = true;
#endif
	BackupSavestate = true;
	SavestateZstdCompression = true;

#ifdef __WXMSW__
	McdCompressNTFS = true;
#endif

	// To be moved to FileMemoryCard pluign (someday)
	for (uint slot = 0; slot < 8; ++slot)
	{
		Mcd[slot].Enabled = !FileMcd_IsMultitapSlot(slot); // enables main 2 slots
		Mcd[slot].Filename = FileMcd_GetDefaultName(slot);

		// Folder memory card is autodetected later.
		Mcd[slot].Type = MemoryCardType::File;
	}

	GzipIsoIndexTemplate = "$(f).pindex.tmp";
}

void Pcsx2Config::LoadSave(SettingsWrapper& wrap)
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
	SettingsWrapBitBool(EnableRecordingTools);
#ifdef PCSX2_CORE
	SettingsWrapBitBool(EnableGameFixes);
	SettingsWrapBitBool(SaveStateOnShutdown);
#endif
	SettingsWrapBitBool(ConsoleToStdio);
	SettingsWrapBitBool(HostFs);
	SettingsWrapBitBool(PatchBios);
	SettingsWrapEntry(PatchRegion);

	SettingsWrapBitBool(BackupSavestate);
	SettingsWrapBitBool(SavestateZstdCompression);
	SettingsWrapBitBool(McdEnableEjection);
	SettingsWrapBitBool(McdFolderAutoManage);
	SettingsWrapBitBool(MultitapPort0_Enabled);
	SettingsWrapBitBool(MultitapPort1_Enabled);

	// Process various sub-components:

	Speedhacks.LoadSave(wrap);
	Cpu.LoadSave(wrap);
	GS.LoadSave(wrap);
#ifdef PCSX2_CORE
	// SPU2 is in a separate ini in wx.
	SPU2.LoadSave(wrap);
#endif
	DEV9.LoadSave(wrap);
	Gamefixes.LoadSave(wrap);
	Profiler.LoadSave(wrap);

	Debugger.LoadSave(wrap);
	Trace.LoadSave(wrap);

	SettingsWrapEntry(GzipIsoIndexTemplate);

	// For now, this in the derived config for backwards ini compatibility.
#ifdef PCSX2_CORE
	SettingsWrapEntryEx(CurrentBlockdump, "BlockDumpSaveDirectory");

	BaseFilenames.LoadSave(wrap);
	Framerate.LoadSave(wrap);
	LoadSaveMemcards(wrap);

#ifdef __WXMSW__
	SettingsWrapEntry(McdCompressNTFS);
#endif
#endif

	if (wrap.IsLoading())
	{
		CurrentAspectRatio = GS.AspectRatio;
	}
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

bool Pcsx2Config::MultitapEnabled(uint port) const
{
	pxAssert(port < 2);
	return (port == 0) ? MultitapPort0_Enabled : MultitapPort1_Enabled;
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

bool Pcsx2Config::operator==(const Pcsx2Config& right) const
{
	bool equal =
		OpEqu(bitset) &&
		OpEqu(Cpu) &&
		OpEqu(GS) &&
		OpEqu(DEV9) &&
		OpEqu(Speedhacks) &&
		OpEqu(Gamefixes) &&
		OpEqu(Profiler) &&
		OpEqu(Debugger) &&
		OpEqu(Framerate) &&
		OpEqu(Trace) &&
		OpEqu(BaseFilenames) &&
		OpEqu(GzipIsoIndexTemplate);
	for (u32 i = 0; i < sizeof(Mcd) / sizeof(Mcd[0]); i++)
	{
		equal &= OpEqu(Mcd[i].Enabled);
		equal &= OpEqu(Mcd[i].Filename);
	}

	return equal;
}

void Pcsx2Config::CopyConfig(const Pcsx2Config& cfg)
{
	Cpu = cfg.Cpu;
	GS = cfg.GS;
	DEV9 = cfg.DEV9;
	Speedhacks = cfg.Speedhacks;
	Gamefixes = cfg.Gamefixes;
	Profiler = cfg.Profiler;
	Debugger = cfg.Debugger;
	Trace = cfg.Trace;
	BaseFilenames = cfg.BaseFilenames;
	Framerate = cfg.Framerate;

	for (u32 i = 0; i < sizeof(Mcd) / sizeof(Mcd[0]); i++)
	{
		// Type will be File here, even if it's a folder, so we preserve the old value.
		// When the memory card is re-opened, it should redetect anyway.
		Mcd[i].Enabled = cfg.Mcd[i].Enabled;
		Mcd[i].Filename = cfg.Mcd[i].Filename;
	}

	GzipIsoIndexTemplate = cfg.GzipIsoIndexTemplate;

	CdvdVerboseReads = cfg.CdvdVerboseReads;
	CdvdDumpBlocks = cfg.CdvdDumpBlocks;
	CdvdShareWrite = cfg.CdvdShareWrite;
	EnablePatches = cfg.EnablePatches;
	EnableCheats = cfg.EnableCheats;
	EnablePINE = cfg.EnablePINE;
	EnableWideScreenPatches = cfg.EnableWideScreenPatches;
	EnableNoInterlacingPatches = cfg.EnableNoInterlacingPatches;
	EnableRecordingTools = cfg.EnableRecordingTools;
	UseBOOT2Injection = cfg.UseBOOT2Injection;
	PatchBios = cfg.PatchBios;
	PatchRegion = cfg.PatchRegion;
	BackupSavestate = cfg.BackupSavestate;
	McdEnableEjection = cfg.McdEnableEjection;
	McdFolderAutoManage = cfg.McdFolderAutoManage;
	MultitapPort0_Enabled = cfg.MultitapPort0_Enabled;
	MultitapPort1_Enabled = cfg.MultitapPort1_Enabled;
	ConsoleToStdio = cfg.ConsoleToStdio;
	HostFs = cfg.HostFs;
#ifdef __WXMSW__
	McdCompressNTFS = cfg.McdCompressNTFS;
#endif

	LimiterMode = cfg.LimiterMode;
}

void EmuFolders::SetDefaults()
{
	Bios = Path::Combine(DataRoot, "bios");
	Snapshots = Path::Combine(DataRoot, "snaps");
	Savestates = Path::Combine(DataRoot, "sstates");
	MemoryCards = Path::Combine(DataRoot, "memcards");
	Logs = Path::Combine(DataRoot, "logs");
	Cheats = Path::Combine(DataRoot, "cheats");
	CheatsWS = Path::Combine(DataRoot, "cheats_ws");
	CheatsNI = Path::Combine(DataRoot, "cheats_ni");
	Covers = Path::Combine(DataRoot, "covers");
	GameSettings = Path::Combine(DataRoot, "gamesettings");
	Cache = Path::Combine(DataRoot, "cache");
	Textures = Path::Combine(DataRoot, "textures");
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
	CheatsWS = LoadPathFromSettings(si, DataRoot, "CheatsWS", "cheats_ws");
	CheatsNI = LoadPathFromSettings(si, DataRoot, "CheatsNI", "cheats_ni");
	Covers = LoadPathFromSettings(si, DataRoot, "Covers", "covers");
	GameSettings = LoadPathFromSettings(si, DataRoot, "GameSettings", "gamesettings");
	Cache = LoadPathFromSettings(si, DataRoot, "Cache", "cache");
	Textures = LoadPathFromSettings(si, DataRoot, "Textures", "textures");

	Console.WriteLn("BIOS Directory: %s", Bios.c_str());
	Console.WriteLn("Snapshots Directory: %s", Snapshots.c_str());
	Console.WriteLn("Savestates Directory: %s", Savestates.c_str());
	Console.WriteLn("MemoryCards Directory: %s", MemoryCards.c_str());
	Console.WriteLn("Logs Directory: %s", Logs.c_str());
	Console.WriteLn("Cheats Directory: %s", Cheats.c_str());
	Console.WriteLn("CheatsWS Directory: %s", CheatsWS.c_str());
	Console.WriteLn("CheatsNI Directory: %s", CheatsNI.c_str());
	Console.WriteLn("Covers Directory: %s", Covers.c_str());
	Console.WriteLn("Game Settings Directory: %s", GameSettings.c_str());
	Console.WriteLn("Cache Directory: %s", Cache.c_str());
	Console.WriteLn("Textures Directory: %s", Textures.c_str());
}

void EmuFolders::Save(SettingsInterface& si)
{
	// convert back to relative
	si.SetStringValue("Folders", "Bios", Path::MakeRelative(Bios, DataRoot).c_str());
	si.SetStringValue("Folders", "Snapshots", Path::MakeRelative(Snapshots, DataRoot).c_str());
	si.SetStringValue("Folders", "Savestates", Path::MakeRelative(Savestates, DataRoot).c_str());
	si.SetStringValue("Folders", "MemoryCards", Path::MakeRelative(MemoryCards, DataRoot).c_str());
	si.SetStringValue("Folders", "Logs", Path::MakeRelative(Logs, DataRoot).c_str());
	si.SetStringValue("Folders", "Cheats", Path::MakeRelative(Cheats, DataRoot).c_str());
	si.SetStringValue("Folders", "CheatsWS", Path::MakeRelative(CheatsWS, DataRoot).c_str());
	si.SetStringValue("Folders", "CheatsNI", Path::MakeRelative(CheatsNI, DataRoot).c_str());
	si.SetStringValue("Folders", "Cache", Path::MakeRelative(Cache, DataRoot).c_str());
	si.SetStringValue("Folders", "Textures", Path::MakeRelative(Textures, DataRoot).c_str());
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
	result = FileSystem::CreateDirectoryPath(CheatsWS.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(CheatsNI.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Covers.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(GameSettings.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Cache.c_str(), false) && result;
	result = FileSystem::CreateDirectoryPath(Textures.c_str(), false) && result;
	return result;
}
