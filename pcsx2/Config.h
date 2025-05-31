// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Host/AudioStreamTypes.h"

#include "common/Pcsx2Defs.h"
#include "common/FPControl.h"

#include <array>
#include <string>
#include <optional>
#include <vector>

// Macro used for removing some of the redtape involved in defining bitfield/union helpers.
//
#define BITFIELD32() \
	union \
	{ \
		u32 bitset; \
		struct \
		{
#define BITFIELD_END \
	} \
	; \
	} \
	;

class Error;
class SettingsInterface;
class SettingsWrapper;

enum class CDVD_SourceType : uint8_t;

namespace Pad
{
	enum class ControllerType : u8;
}

/// Generic setting information which can be reused in multiple components.
struct SettingInfo
{
	using GetOptionsCallback = std::vector<std::pair<std::string, std::string>> (*)();

	enum class Type
	{
		Boolean,
		Integer,
		IntegerList,
		Float,
		String,
		StringList,
		Path,
	};

	Type type;
	const char* name;
	const char* display_name;
	const char* description;
	const char* default_value;
	const char* min_value;
	const char* max_value;
	const char* step_value;
	const char* format;
	const char* const* options; // For integer lists.
	GetOptionsCallback get_options; // For string lists.
	float multiplier;

	const char* StringDefaultValue() const;
	bool BooleanDefaultValue() const;
	s32 IntegerDefaultValue() const;
	s32 IntegerMinValue() const;
	s32 IntegerMaxValue() const;
	s32 IntegerStepValue() const;
	float FloatDefaultValue() const;
	float FloatMinValue() const;
	float FloatMaxValue() const;
	float FloatStepValue() const;

	void SetDefaultValue(SettingsInterface* si, const char* section, const char* key) const;
	void CopyValue(SettingsInterface* dest_si, const SettingsInterface& src_si,
		const char* section, const char* key) const;
};

enum class GenericInputBinding : u8;

// TODO(Stenzek): Move to InputCommon.h or something?
struct InputBindingInfo
{
	enum class Type : u8
	{
		Unknown,
		Button,
		Axis,
		HalfAxis,
		Motor,
		Pointer, // Receive relative mouse movement events, bind_index is offset by the axis.
		Keyboard, // Receive host key events, bind_index is offset by the key code.
		Device, // Used for special-purpose device selection, e.g. force feedback.
		Macro,
	};

	const char* name;
	const char* display_name;
	const char* icon_name;
	Type bind_type;
	u16 bind_index;
	GenericInputBinding generic_mapping;
};

/// Generic input bindings. These roughly match a DualShock 4 or XBox One controller.
/// They are used for automatic binding to PS2 controller types, and for big picture mode navigation.
enum class GenericInputBinding : u8
{
	Unknown,

	DPadUp,
	DPadRight,
	DPadLeft,
	DPadDown,

	LeftStickUp,
	LeftStickRight,
	LeftStickDown,
	LeftStickLeft,
	L3,

	RightStickUp,
	RightStickRight,
	RightStickDown,
	RightStickLeft,
	R3,

	Triangle, // Y on XBox pads.
	Circle, // B on XBox pads.
	Cross, // A on XBox pads.
	Square, // X on XBox pads.

	Select, // Share on DS4, View on XBox pads.
	Start, // Options on DS4, Menu on XBox pads.
	System, // PS button on DS4, Guide button on XBox pads.

	L1, // LB on Xbox pads.
	L2, // Left trigger on XBox pads.
	R1, // RB on XBox pads.
	R2, // Right trigger on Xbox pads.

	SmallMotor, // High frequency vibration.
	LargeMotor, // Low frequency vibration.

	Count,
};

enum GamefixId
{
	GamefixId_FIRST = 0,

	Fix_FpuMultiply = GamefixId_FIRST,
	Fix_GoemonTlbMiss,
	Fix_SoftwareRendererFMV,
	Fix_SkipMpeg,
	Fix_OPHFlag,
	Fix_EETiming,
	Fix_InstantDMA,
	Fix_DMABusy,
	Fix_GIFFIFO,
	Fix_VIFFIFO,
	Fix_VIF1Stall,
	Fix_VuAddSub,
	Fix_Ibit,
	Fix_VUSync,
	Fix_VUOverflow,
	Fix_XGKick,
	Fix_BlitInternalFPS,
	Fix_FullVU0Sync,

	GamefixId_COUNT
};

// TODO - config - not a fan of the excessive use of enums and macros to make them work
// a proper object would likely make more sense (if possible).

enum class SpeedHack
{
	MVUFlag,
	InstantVU1,
	MTVU,
	EECycleRate,
	MaxCount,
};

enum class DebugAnalysisCondition
{
	ALWAYS,
	IF_DEBUGGER_IS_OPEN,
	NEVER
};

struct DebugSymbolSource
{
	std::string Name;
	bool ClearDuringAnalysis = false;

	friend auto operator<=>(const DebugSymbolSource& lhs, const DebugSymbolSource& rhs) = default;
};

struct DebugExtraSymbolFile
{
	std::string Path;
	std::string BaseAddress;
	std::string Condition;

	friend auto operator<=>(const DebugExtraSymbolFile& lhs, const DebugExtraSymbolFile& rhs) = default;
};

enum class DebugFunctionScanMode
{
	SCAN_ELF,
	SCAN_MEMORY,
	SKIP
};

enum class AspectRatioType : u8
{
	Stretch, // Stretches to the whole window/display size
	RAuto4_3_3_2, // Automatically scales to the target aspect ratio if there's a widescreen patch
	R4_3,
	R16_9,
	R10_7,
	MaxCount
};

enum class FMVAspectRatioSwitchType : u8
{
	Off, // Falls back on the selected generic aspect ratio type
	RAuto4_3_3_2,
	R4_3,
	R16_9,
	R10_7,
	MaxCount
};

enum class MemoryCardType
{
	Empty,
	File,
	Folder,
	MaxCount
};

enum class MemoryCardFileType
{
	Unknown,
	PS2_8MB,
	PS2_16MB,
	PS2_32MB,
	PS2_64MB,
	PS1,
	MaxCount
};

enum class LimiterModeType : u8
{
	Nominal,
	Turbo,
	Slomo,
	Unlimited,
};

enum class GSRendererType : s8
{
	Auto = -1,
	DX11 = 3,
	Null = 11,
	OGL = 12,
	SW = 13,
	VK = 14,
	Metal = 17,
	DX12 = 15,
};

enum class GSVSyncMode : u8
{
	Disabled,
	FIFO,
	Mailbox,
	Count
};

enum class GSInterlaceMode : u8
{
	Automatic,
	Off,
	WeaveTFF,
	WeaveBFF,
	BobTFF,
	BobBFF,
	BlendTFF,
	BlendBFF,
	AdaptiveTFF,
	AdaptiveBFF,
	Count
};

enum class GSPostBilinearMode : u8
{
	Off,
	BilinearSmooth,
	BilinearSharp,
};

// Ordering was done to keep compatibility with older ini file.
enum class BiFiltering : u8
{
	Nearest,
	Forced,
	PS2,
	Forced_But_Sprite,
};

enum class TriFiltering : s8
{
	Automatic = -1,
	Off,
	PS2,
	Forced,
};

enum class AccBlendLevel : u8
{
	Minimum,
	Basic,
	Medium,
	High,
	Full,
	Maximum,
};

enum class OsdOverlayPos : u8
{
	None,
	TopLeft,
	TopRight,
};

enum class TexturePreloadingLevel : u8
{
	Off,
	Partial,
	Full,
};

enum class GSScreenshotSize : u8
{
	WindowResolution,
	InternalResolution,
	InternalResolutionUncorrected,
};

enum class GSScreenshotFormat : u8
{
	PNG,
	JPEG,
	WebP,
	Count,
};

enum class GSDumpCompressionMethod : u8
{
	Uncompressed,
	LZMA,
	Zstandard,
};

enum class SavestateCompressionMethod : u8
{
	Uncompressed = 0,
	Deflate64 = 1,
	Zstandard = 2,
	LZMA2 = 3
};

enum class SavestateCompressionLevel : u8
{
	Low = 0,
	Medium = 1,
	High = 2,
	VeryHigh = 3,
};

enum class GSHardwareDownloadMode : u8
{
	Enabled,
	NoReadbacks,
	Unsynchronized,
	Disabled
};

enum class GSCASMode : u8
{
	Disabled,
	SharpenOnly,
	SharpenAndResize,
};

enum class GSHWAutoFlushLevel : u8
{
	Disabled,
	SpritesOnly,
	Enabled,
};

enum class GSGPUTargetCLUTMode : u8
{
	Disabled,
	Enabled,
	InsideTarget,
};

enum class GSTextureInRtMode : u8
{
	Disabled,
	InsideTargets,
	MergeTargets,
};

enum class GSBilinearDirtyMode : u8
{
	Automatic,
	ForceBilinear,
	ForceNearest,
	MaxCount
};

enum class GSHalfPixelOffset : u8
{
	Off,
	Normal,
	Special,
	SpecialAggressive,
	Native,
	NativeWTexOffset,
	MaxCount
};

enum class GSNativeScaling : u8
{
	Off,
	Normal,
	Aggressive,
	MaxCount
};

// --------------------------------------------------------------------------------------
//  TraceLogsEE
// --------------------------------------------------------------------------------------
struct TraceLogsEE
{
	// EE
	BITFIELD32()
	bool
		bios : 1,
		memory : 1,
		giftag : 1,
		vifcode : 1,
		mskpath3 : 1,
		r5900 : 1,
		cop0 : 1,
		cop1 : 1,
		cop2 : 1,
		cache : 1,
		knownhw : 1,
		unknownhw : 1,
		dmahw : 1,
		ipu : 1,
		dmac : 1,
		counters : 1,
		spr : 1,
		vif : 1,
		gif : 1;
	BITFIELD_END

	TraceLogsEE();

	bool operator==(const TraceLogsEE& right) const;
	bool operator!=(const TraceLogsEE& right) const;
};

// --------------------------------------------------------------------------------------
//  TraceLogsIOP
// --------------------------------------------------------------------------------------
struct TraceLogsIOP
{
	BITFIELD32()
	bool
		bios : 1,
		memcards : 1,
		pad : 1,
		r3000a : 1,
		cop2 : 1,
		memory : 1,
		knownhw : 1,
		unknownhw : 1,
		dmahw : 1,
		dmac : 1,
		counters : 1,
		cdvd : 1,
		mdec : 1;
	BITFIELD_END

	TraceLogsIOP();

	bool operator==(const TraceLogsIOP& right) const;
	bool operator!=(const TraceLogsIOP& right) const;
};

// --------------------------------------------------------------------------------------
//  TraceLogsMISC
// --------------------------------------------------------------------------------------
struct TraceLogsMISC
{
	BITFIELD32()
	bool
		sif : 1;
	BITFIELD_END

	TraceLogsMISC();

	bool operator==(const TraceLogsMISC& right) const;
	bool operator!=(const TraceLogsMISC& right) const;
};

// --------------------------------------------------------------------------------------
//  TraceLogFilters
// --------------------------------------------------------------------------------------
struct TraceLogFilters
{
	bool Enabled;

	TraceLogsEE EE;
	TraceLogsIOP IOP;
	TraceLogsMISC MISC;

	TraceLogFilters();

	void LoadSave(SettingsWrapper& ini);
	// When logging, the tracelogpack is checked, not was in the config.
	// Call this to sync the tracelogpack values with the config values.
	void SyncToConfig() const;
	bool operator==(const TraceLogFilters& right) const;
	bool operator!=(const TraceLogFilters& right) const;
};

// --------------------------------------------------------------------------------------
//  Pcsx2Config class
// --------------------------------------------------------------------------------------
// This is intended to be a public class library between the core emulator and GUI only.
//
// When GUI code performs modifications of this class, it must be done with strict thread
// safety, since the emu runs on a separate thread.  Additionally many components of the
// class require special emu-side resets or state save/recovery to be applied.  Please
// use the provided functions to lock the emulation into a safe state and then apply
// chances on the necessary scope (see Core_Pause, Core_ApplySettings, and Core_Resume).
//
struct Pcsx2Config
{
	struct ProfilerOptions
	{
		BITFIELD32()
		bool
			Enabled : 1, // universal toggle for the profiler.
			RecBlocks_EE : 1, // Enables per-block profiling for the EE recompiler [unimplemented]
			RecBlocks_IOP : 1, // Enables per-block profiling for the IOP recompiler [unimplemented]
			RecBlocks_VU0 : 1, // Enables per-block profiling for the VU0 recompiler [unimplemented]
			RecBlocks_VU1 : 1; // Enables per-block profiling for the VU1 recompiler [unimplemented]
		BITFIELD_END

		// Default is Disabled, with all recs enabled underneath.
		ProfilerOptions();
		void LoadSave(SettingsWrapper& wrap);

		bool operator==(const ProfilerOptions& right) const;
		bool operator!=(const ProfilerOptions& right) const;
	};

	// ------------------------------------------------------------------------
	struct RecompilerOptions
	{
		BITFIELD32()
		bool
			EnableEE : 1,
			EnableIOP : 1,
			EnableVU0 : 1,
			EnableVU1 : 1;

		bool
			vu0Overflow : 1,
			vu0ExtraOverflow : 1,
			vu0SignOverflow : 1,
			vu0Underflow : 1;

		bool
			vu1Overflow : 1,
			vu1ExtraOverflow : 1,
			vu1SignOverflow : 1,
			vu1Underflow : 1;

		bool
			fpuOverflow : 1,
			fpuExtraOverflow : 1,
			fpuFullMode : 1;

		bool
			EnableEECache : 1;
		bool
			EnableFastmem : 1;
		bool
			PauseOnTLBMiss : 1;
		BITFIELD_END

		RecompilerOptions();
		void ApplySanityCheck();

		void LoadSave(SettingsWrapper& wrap);

		bool operator==(const RecompilerOptions& right) const;
		bool operator!=(const RecompilerOptions& right) const;

		u32 GetEEClampMode() const;
		void SetEEClampMode(u32 value);

		u32 GetVUClampMode() const;
	};

	// ------------------------------------------------------------------------
	struct CpuOptions
	{
		BITFIELD32()
		bool
			ExtraMemory : 1;
		BITFIELD_END

		RecompilerOptions Recompiler;

		FPControlRegister FPUFPCR;
		FPControlRegister FPUDivFPCR;
		FPControlRegister VU0FPCR;
		FPControlRegister VU1FPCR;

		CpuOptions();
		void LoadSave(SettingsWrapper& wrap);
		void ApplySanityCheck();

		bool CpusChanged(const CpuOptions& right) const;

		bool operator==(const CpuOptions& right) const;
		bool operator!=(const CpuOptions& right) const;
	};

	// ------------------------------------------------------------------------
	struct GSOptions
	{
		static const char* AspectRatioNames[];
		static const char* FMVAspectRatioSwitchNames[];
		static const char* BlendingLevelNames[];
		static const char* CaptureContainers[];

		static const char* GetRendererName(GSRendererType type);

		/// Converts a tri-state option to an optional boolean value.
		static std::optional<bool> TriStateToOptionalBoolean(int value);

		static constexpr float DEFAULT_FRAME_RATE_NTSC = 59.94f;
		static constexpr float DEFAULT_FRAME_RATE_PAL = 50.00f;

		static constexpr AspectRatioType DEFAULT_ASPECT_RATIO = AspectRatioType::RAuto4_3_3_2;
		static constexpr GSInterlaceMode DEFAULT_INTERLACE_MODE = GSInterlaceMode::Automatic;

		static constexpr int DEFAULT_VIDEO_CAPTURE_BITRATE = 6000;
		static constexpr int DEFAULT_VIDEO_CAPTURE_WIDTH = 640;
		static constexpr int DEFAULT_VIDEO_CAPTURE_HEIGHT = 480;
		static constexpr int DEFAULT_AUDIO_CAPTURE_BITRATE = 192;
		static const char* DEFAULT_CAPTURE_CONTAINER;

		union
		{
			u64 bitset;

			struct
			{
				bool
					SynchronousMTGS : 1,
					VsyncEnable : 1,
					DisableMailboxPresentation : 1,
					ExtendedUpscalingMultipliers : 1,
					PCRTCAntiBlur : 1,
					DisableInterlaceOffset : 1,
					PCRTCOffsets : 1,
					PCRTCOverscan : 1,
					IntegerScaling : 1,
					UseDebugDevice : 1,
					UseBlitSwapChain : 1,
					DisableShaderCache : 1,
					DisableFramebufferFetch : 1,
					DisableVertexShaderExpand : 1,
					SkipDuplicateFrames : 1,
					OsdShowSpeed : 1,
					OsdShowFPS : 1,
					OsdShowVPS : 1,
					OsdShowCPU : 1,
					OsdShowGPU : 1,
					OsdShowResolution : 1,
					OsdShowGSStats : 1,
					OsdShowIndicators : 1,
					OsdShowSettings : 1,
					OsdShowInputs : 1,
					OsdShowFrameTimes : 1,
					OsdShowVersion : 1,
					OsdShowVideoCapture : 1,
					OsdShowInputRec : 1,
					OsdShowHardwareInfo : 1,
					HWSpinGPUForReadbacks : 1,
					HWSpinCPUForReadbacks : 1,
					GPUPaletteConversion : 1,
					AutoFlushSW : 1,
					PreloadFrameWithGSData : 1,
					Mipmap : 1,
					HWMipmap : 1,
					ManualUserHacks : 1,
					UserHacks_AlignSpriteX : 1,
					UserHacks_CPUFBConversion : 1,
					UserHacks_ReadTCOnClose : 1,
					UserHacks_DisableDepthSupport : 1,
					UserHacks_DisablePartialInvalidation : 1,
					UserHacks_DisableSafeFeatures : 1,
					UserHacks_DisableRenderFixes : 1,
					UserHacks_MergePPSprite : 1,
					UserHacks_ForceEvenSpritePosition : 1,
					UserHacks_NativePaletteDraw : 1,
					UserHacks_EstimateTextureRegion : 1,
					FXAA : 1,
					ShadeBoost : 1,
					DumpGSData : 1,
					SaveRT : 1,
					SaveFrame : 1,
					SaveTexture : 1,
					SaveDepth : 1,
					SaveAlpha : 1,
					SaveInfo : 1,
					DumpReplaceableTextures : 1,
					DumpReplaceableMipmaps : 1,
					DumpTexturesWithFMVActive : 1,
					DumpDirectTextures : 1,
					DumpPaletteTextures : 1,
					LoadTextureReplacements : 1,
					LoadTextureReplacementsAsync : 1,
					PrecacheTextureReplacements : 1,
					EnableVideoCapture : 1,
					EnableVideoCaptureParameters : 1,
					VideoCaptureAutoResolution : 1,
					EnableAudioCapture : 1,
					EnableAudioCaptureParameters : 1;
			};
		};

		int VsyncQueueSize = 2;

		float FramerateNTSC = DEFAULT_FRAME_RATE_NTSC;
		float FrameratePAL = DEFAULT_FRAME_RATE_PAL;

		AspectRatioType AspectRatio = DEFAULT_ASPECT_RATIO;
		FMVAspectRatioSwitchType FMVAspectRatioSwitch = FMVAspectRatioSwitchType::Off;
		GSInterlaceMode InterlaceMode = DEFAULT_INTERLACE_MODE;
		GSPostBilinearMode LinearPresent = GSPostBilinearMode::BilinearSmooth;

		float StretchY = 100.0f;
		int Crop[4] = {};

		float OsdScale = 100.0f;
		OsdOverlayPos OsdMessagesPos = OsdOverlayPos::TopLeft;
		OsdOverlayPos OsdPerformancePos = OsdOverlayPos::TopRight;

		GSRendererType Renderer = GSRendererType::Auto;
		float UpscaleMultiplier = 1.0f;

		AccBlendLevel AccurateBlendingUnit = AccBlendLevel::Basic;
		BiFiltering TextureFiltering = BiFiltering::PS2;
		TexturePreloadingLevel TexturePreloading = TexturePreloadingLevel::Full;
		GSDumpCompressionMethod GSDumpCompression = GSDumpCompressionMethod::Zstandard;
		GSHardwareDownloadMode HWDownloadMode = GSHardwareDownloadMode::Enabled;
		GSCASMode CASMode = GSCASMode::Disabled;
		u8 Dithering = 2;
		u8 MaxAnisotropy = 0;
		u8 TVShader = 0;
		s16 GetSkipCountFunctionId = -1;
		s16 BeforeDrawFunctionId = -1;
		s16 MoveHandlerFunctionId = -1;
		int SkipDrawStart = 0;
		int SkipDrawEnd = 0;

		GSHWAutoFlushLevel UserHacks_AutoFlush = GSHWAutoFlushLevel::Disabled;
		GSHalfPixelOffset UserHacks_HalfPixelOffset = GSHalfPixelOffset::Off;
		s8 UserHacks_RoundSprite = 0;
		GSNativeScaling UserHacks_NativeScaling = GSNativeScaling::Off;
		s32 UserHacks_TCOffsetX = 0;
		s32 UserHacks_TCOffsetY = 0;
		u8 UserHacks_CPUSpriteRenderBW = 0;
		u8 UserHacks_CPUSpriteRenderLevel = 0;
		u8 UserHacks_CPUCLUTRender = 0;
		GSGPUTargetCLUTMode UserHacks_GPUTargetCLUTMode = GSGPUTargetCLUTMode::Disabled;
		GSTextureInRtMode UserHacks_TextureInsideRt = GSTextureInRtMode::Disabled;
		GSBilinearDirtyMode UserHacks_BilinearHack = GSBilinearDirtyMode::Automatic;
		TriFiltering TriFilter = TriFiltering::Automatic;
		s8 OverrideTextureBarriers = -1;

		u8 CAS_Sharpness = 50;
		u8 ShadeBoost_Brightness = 50;
		u8 ShadeBoost_Contrast = 50;
		u8 ShadeBoost_Saturation = 50;
		u8 PNGCompressionLevel = 1;

		u16 SWExtraThreads = 2;
		u16 SWExtraThreadsHeight = 4;

		int SaveDrawStart = 0;
		int SaveDrawCount = 5000;
		int SaveDrawBy = 1;
		int SaveFrameStart = 0;
		int SaveFrameCount = -1;
		int SaveFrameBy = 1;

		s8 ExclusiveFullscreenControl = -1;
		GSScreenshotSize ScreenshotSize = GSScreenshotSize::WindowResolution;
		GSScreenshotFormat ScreenshotFormat = GSScreenshotFormat::PNG;
		int ScreenshotQuality = 90;

		std::string CaptureContainer = DEFAULT_CAPTURE_CONTAINER;
		std::string VideoCaptureCodec;
		std::string VideoCaptureFormat;
		std::string VideoCaptureParameters;
		std::string AudioCaptureCodec;
		std::string AudioCaptureParameters;
		int VideoCaptureBitrate = DEFAULT_VIDEO_CAPTURE_BITRATE;
		int VideoCaptureWidth = DEFAULT_VIDEO_CAPTURE_WIDTH;
		int VideoCaptureHeight = DEFAULT_VIDEO_CAPTURE_HEIGHT;
		int AudioCaptureBitrate = DEFAULT_AUDIO_CAPTURE_BITRATE;

		std::string Adapter;
		std::string HWDumpDirectory;
		std::string SWDumpDirectory;

		GSOptions();

		void LoadSave(SettingsWrapper& wrap);

		/// Sets user hack values to defaults when user hacks are not enabled.
		void MaskUserHacks();

		/// Sets user hack values to defaults when upscaling is not enabled.
		void MaskUpscalingHacks();

		/// Returns true if any of the hardware renderers are selected.
		bool UseHardwareRenderer() const;

		/// Returns false if the compared to the old settings, we need to reopen GS.
		/// (i.e. renderer change, swap chain mode change, etc.)
		bool RestartOptionsAreEqual(const GSOptions& right) const;

		/// Returns false if any options need to be applied to the MTGS.
		bool OptionsAreEqual(const GSOptions& right) const;

		bool operator==(const GSOptions& right) const;
		bool operator!=(const GSOptions& right) const;

		// Should we dump this draw/frame?
		bool ShouldDump(int draw, int frame) const;
	};

	struct SPU2Options
	{
		enum class SPU2SyncMode : u8
		{
			Disabled,
			TimeStretch,
			Count
		};

		static constexpr s32 MAX_VOLUME = 200;
		static constexpr AudioBackend DEFAULT_BACKEND = AudioBackend::Cubeb;
		static constexpr SPU2SyncMode DEFAULT_SYNC_MODE = SPU2SyncMode::TimeStretch;

		static std::optional<SPU2SyncMode> ParseSyncMode(const char* str);
		static const char* GetSyncModeName(SPU2SyncMode backend);
		static const char* GetSyncModeDisplayName(SPU2SyncMode backend);

		BITFIELD32()
		bool
			DebugEnabled : 1,
			MsgToConsole : 1,
			MsgKeyOnOff : 1,
			MsgVoiceOff : 1,
			MsgDMA : 1,
			MsgAutoDMA : 1,
			MsgCache : 1,
			AccessLog : 1,
			DMALog : 1,
			WaveLog : 1,
			CoresDump : 1,
			MemDump : 1,
			RegDump : 1,
			VisualDebugEnabled : 1;
		BITFIELD_END

		u32 OutputVolume = 100;
		u32 FastForwardVolume = 100;
		bool OutputMuted = false;

		AudioBackend Backend = DEFAULT_BACKEND;
		SPU2SyncMode SyncMode = DEFAULT_SYNC_MODE;
		AudioStreamParameters StreamParameters;

		std::string DriverName;
		std::string DeviceName;

		SPU2Options();

		void LoadSave(SettingsWrapper& wrap);

		bool IsTimeStretchEnabled() const { return (SyncMode == SPU2SyncMode::TimeStretch); }

		bool operator==(const SPU2Options& right) const;
		bool operator!=(const SPU2Options& right) const;
	};

	struct DEV9Options
	{
		enum struct NetApi : int
		{
			Unset = 0,
			PCAP_Bridged = 1,
			PCAP_Switched = 2,
			TAP = 3,
			Sockets = 4,
		};
		static const char* NetApiNames[];

		enum struct DnsMode : int
		{
			Manual = 0,
			Auto = 1,
			Internal = 2,
		};
		static const char* DnsModeNames[];

		struct HostEntry
		{
			std::string Url;
			std::string Desc;
			u8 Address[4]{};
			bool Enabled;

			bool operator==(const HostEntry& right) const;
			bool operator!=(const HostEntry& right) const;
		};

		bool EthEnable{false};
		NetApi EthApi{NetApi::Unset};
		std::string EthDevice;
		bool EthLogDHCP{false};
		bool EthLogDNS{false};

		bool InterceptDHCP{false};
		u8 PS2IP[4]{};
		u8 Mask[4]{};
		u8 Gateway[4]{};
		u8 DNS1[4]{};
		u8 DNS2[4]{};
		bool AutoMask{true};
		bool AutoGateway{true};
		DnsMode ModeDNS1{DnsMode::Auto};
		DnsMode ModeDNS2{DnsMode::Auto};

		std::vector<HostEntry> EthHosts;

		bool HddEnable{false};
		std::string HddFile;

		DEV9Options();

		void LoadSave(SettingsWrapper& wrap);

		bool operator==(const DEV9Options& right) const;
		bool operator!=(const DEV9Options& right) const;

	protected:
		static void LoadIPHelper(u8* field, const std::string& setting);
		static std::string SaveIPHelper(u8* field);
	};

	// ------------------------------------------------------------------------
	// NOTE: The GUI's GameFixes panel is dependent on the order of bits in this structure.
	struct GamefixOptions
	{
		BITFIELD32()
		bool
			FpuMulHack : 1, // Tales of Destiny hangs.
			GoemonTlbHack : 1, // Gomeon tlb miss hack. The game need to access unmapped virtual address. Instead to handle it as exception, tlb are preloaded at startup
			SoftwareRendererFMVHack : 1, // Switches to software renderer for FMVs
			SkipMPEGHack : 1, // Skips MPEG videos (Katamari and other games need this)
			OPHFlagHack : 1, // Bleach Blade Battlers
			EETimingHack : 1, // General purpose timing hack.
			InstantDMAHack : 1, // Instantly complete DMA's if possible, good for cache emulation problems.
			DMABusyHack : 1, // Denies writes to the DMAC when it's busy. This is correct behaviour but bad timing can cause problems.
			GIFFIFOHack : 1, // Enabled the GIF FIFO (more correct but slower)
			VIFFIFOHack : 1, // Pretends to fill the non-existant VIF FIFO Buffer.
			VIF1StallHack : 1, // Like above, processes FIFO data before the stall is allowed (to make sure data goes over).
			VuAddSubHack : 1, // Tri-ace games, they use an encryption algorithm that requires VU ADDI opcode to be bit-accurate.
			IbitHack : 1, // I bit hack. Needed to stop constant VU recompilation in some games
			VUSyncHack : 1, // Makes microVU run behind the EE to avoid VU register reading/writing sync issues. Useful for M-Bit games
			VUOverflowHack : 1, // Tries to simulate overflow flag checks (not really possible on x86 without soft floats)
			XgKickHack : 1, // Erementar Gerad, adds more delay to VU XGkick instructions. Corrects the color of some graphics, but breaks Tri-ace games and others.
			BlitInternalFPSHack : 1, // Disables privileged register write-based FPS detection.
			FullVU0SyncHack : 1; // Forces tight VU0 sync on every COP2 instruction.
		BITFIELD_END

		GamefixOptions();
		void LoadSave(SettingsWrapper& wrap);
		GamefixOptions& DisableAll();

		static const char* GetGameFixName(GamefixId id);

		bool Get(GamefixId id) const;
		void Set(GamefixId id, bool enabled = true);
		void Clear(GamefixId id) { Set(id, false); }

		bool operator==(const GamefixOptions& right) const;
		bool operator!=(const GamefixOptions& right) const;
	};

	// ------------------------------------------------------------------------
	struct SpeedhackOptions
	{
		static constexpr s8 MIN_EE_CYCLE_RATE = -3;
		static constexpr s8 MAX_EE_CYCLE_RATE = 3;
		static constexpr u8 MAX_EE_CYCLE_SKIP = 3;

		BITFIELD32()
		bool
			fastCDVD : 1, // enables fast CDVD access
			IntcStat : 1, // tells Pcsx2 to fast-forward through intc_stat waits.
			WaitLoop : 1, // enables constant loop detection and fast-forwarding
			vuFlagHack : 1, // microVU specific flag hack
			vuThread : 1, // Enable Threaded VU1
			vu1Instant : 1; // Enable Instant VU1 (Without MTVU only)
		BITFIELD_END

		s8 EECycleRate; // EE cycle rate selector (1.0, 1.5, 2.0)
		u8 EECycleSkip; // EE Cycle skip factor (0, 1, 2, or 3)

		SpeedhackOptions();
		void LoadSave(SettingsWrapper& conf);
		SpeedhackOptions& DisableAll();

		void Set(SpeedHack id, int value);

		bool operator==(const SpeedhackOptions& right) const;
		bool operator!=(const SpeedhackOptions& right) const;

		static const char* GetSpeedHackName(SpeedHack id);
		static std::optional<SpeedHack> ParseSpeedHackName(const std::string_view name);
	};

	// ------------------------------------------------------------------------
	struct DebugAnalysisOptions
	{

		static const char* RunConditionNames[];
		static const char* FunctionScanModeNames[];

		DebugAnalysisCondition RunCondition = DebugAnalysisCondition::IF_DEBUGGER_IS_OPEN;
		bool GenerateSymbolsForIRXExports = true;

		bool AutomaticallySelectSymbolsToClear = true;
		std::vector<DebugSymbolSource> SymbolSources;

		bool ImportSymbolsFromELF = true;
		bool ImportSymFileFromDefaultLocation = true;
		bool DemangleSymbols = true;
		bool DemangleParameters = true;
		std::vector<DebugExtraSymbolFile> ExtraSymbolFiles;

		DebugFunctionScanMode FunctionScanMode = DebugFunctionScanMode::SCAN_ELF;
		bool CustomFunctionScanRange = false;
		std::string FunctionScanStartAddress;
		std::string FunctionScanEndAddress;

		bool GenerateFunctionHashes = true;

		void LoadSave(SettingsWrapper& wrap);

		friend auto operator<=>(const DebugAnalysisOptions& lhs, const DebugAnalysisOptions& rhs) = default;
	};

	// ------------------------------------------------------------------------
	struct EmulationSpeedOptions
	{
		BITFIELD32()
		bool SyncToHostRefreshRate : 1;
		bool UseVSyncForTiming : 1;
		BITFIELD_END

		float NominalScalar{1.0f};
		float TurboScalar{2.0f};
		float SlomoScalar{0.5f};

		EmulationSpeedOptions();

		void LoadSave(SettingsWrapper& wrap);
		void SanityCheck();

		bool operator==(const EmulationSpeedOptions& right) const;
		bool operator!=(const EmulationSpeedOptions& right) const;
	};

	// ------------------------------------------------------------------------
	struct FilenameOptions
	{
		std::string Bios;

		FilenameOptions();
		void LoadSave(SettingsWrapper& wrap);

		bool operator==(const FilenameOptions& right) const;
		bool operator!=(const FilenameOptions& right) const;
	};

	// ------------------------------------------------------------------------
	struct USBOptions
	{
		static constexpr u32 NUM_PORTS = 2;

		struct Port
		{
			s32 DeviceType;
			u32 DeviceSubtype;

			bool operator==(const USBOptions::Port& right) const;
			bool operator!=(const USBOptions::Port& right) const;
		};

		std::array<Port, NUM_PORTS> Ports;

		USBOptions();
		void LoadSave(SettingsWrapper& wrap);

		bool operator==(const USBOptions& right) const;
		bool operator!=(const USBOptions& right) const;
	};

	// ------------------------------------------------------------------------
	struct PadOptions
	{
		static constexpr u32 NUM_PORTS = 8;

		struct Port
		{
			Pad::ControllerType Type;

			bool operator==(const PadOptions::Port& right) const;
			bool operator!=(const PadOptions::Port& right) const;
		};

		std::array<Port, NUM_PORTS> Ports;

		BITFIELD32()
		bool
			MultitapPort0_Enabled : 1,
			MultitapPort1_Enabled;
		BITFIELD_END

		PadOptions();
		void LoadSave(SettingsWrapper& wrap);

		bool IsMultitapPortEnabled(u32 port) const
		{
			return (port == 0) ? MultitapPort0_Enabled : MultitapPort1_Enabled;
		}

		bool operator==(const PadOptions& right) const;
		bool operator!=(const PadOptions& right) const;
	};

	// ------------------------------------------------------------------------
	// Options struct for each memory card.
	//
	struct McdOptions
	{
		std::string Filename; // user-configured location of this memory card
		bool Enabled; // memory card enabled (if false, memcard will not show up in-game)
		MemoryCardType Type; // the memory card implementation that should be used
	};

	// ------------------------------------------------------------------------

	struct AchievementsOptions
	{
		static constexpr u32 MINIMUM_NOTIFICATION_DURATION = 3;
		static constexpr u32 MAXIMUM_NOTIFICATION_DURATION = 30;
		static constexpr u32 DEFAULT_NOTIFICATION_DURATION = 5;
		static constexpr u32 DEFAULT_LEADERBOARD_DURATION = 10;
		static constexpr const char* DEFAULT_INFO_SOUND_NAME = "sounds/achievements/message.wav";
		static constexpr const char* DEFAULT_UNLOCK_SOUND_NAME = "sounds/achievements/unlock.wav";
		static constexpr const char* DEFAULT_LBSUBMIT_SOUND_NAME = "sounds/achievements/lbsubmit.wav";

		BITFIELD32()
		bool
			Enabled : 1,
			HardcoreMode : 1,
			EncoreMode : 1,
			SpectatorMode : 1,
			UnofficialTestMode : 1,
			Notifications : 1,
			LeaderboardNotifications : 1,
			SoundEffects : 1,
			InfoSound : 1,
			UnlockSound : 1,
			LBSubmitSound : 1,
			Overlays : 1,
			LBOverlays : 1;
		BITFIELD_END

		u32 NotificationsDuration = DEFAULT_NOTIFICATION_DURATION;
		u32 LeaderboardsDuration = DEFAULT_LEADERBOARD_DURATION;

		std::string InfoSoundName;
		std::string UnlockSoundName;
		std::string LBSubmitSoundName;

		AchievementsOptions();
		void LoadSave(SettingsWrapper& wrap);

		bool operator==(const AchievementsOptions& right) const;
		bool operator!=(const AchievementsOptions& right) const;
	};

	struct SavestateOptions
	{
		SavestateOptions();
		void LoadSave(SettingsWrapper& wrap);

		SavestateCompressionMethod CompressionType = SavestateCompressionMethod::Zstandard;
		SavestateCompressionLevel CompressionRatio = SavestateCompressionLevel::Medium;

		bool operator==(const SavestateOptions& right) const;
		bool operator!=(const SavestateOptions& right) const;
	};

	// ------------------------------------------------------------------------

	BITFIELD32()
	bool
		CdvdVerboseReads : 1, // enables cdvd read activity verbosely dumped to the console
		CdvdDumpBlocks : 1, // enables cdvd block dumping
		CdvdPrecache : 1, // enables cdvd precaching of compressed images
		EnablePatches : 1, // enables patch detection and application
		EnableCheats : 1, // enables cheat detection and application
		EnablePINE : 1, // enables inter-process communication
		EnableWideScreenPatches : 1,
		EnableNoInterlacingPatches : 1,
		EnableFastBoot : 1,
		EnableFastBootFastForward : 1,
		EnableThreadPinning : 1,
		// TODO - Vaser - where are these settings exposed in the Qt UI?
		EnableRecordingTools : 1,
		EnableGameFixes : 1, // enables automatic game fixes
		SaveStateOnShutdown : 1, // default value for saving state on shutdown
		EnableDiscordPresence : 1, // enables discord rich presence integration
		UseSavestateSelector : 1,
		InhibitScreensaver : 1,
		BackupSavestate : 1,
		McdFolderAutoManage : 1,
		ManuallySetRealTimeClock : 1,

		HostFs : 1,

		WarnAboutUnsafeSettings : 1;
	BITFIELD_END

	CpuOptions Cpu;
	GSOptions GS;
	SpeedhackOptions Speedhacks;
	GamefixOptions Gamefixes;
	ProfilerOptions Profiler;
	DebugAnalysisOptions DebuggerAnalysis;
	EmulationSpeedOptions EmulationSpeed;
	SavestateOptions Savestate;
	SPU2Options SPU2;
	DEV9Options DEV9;
	USBOptions USB;
	PadOptions Pad;

	TraceLogFilters Trace;

	FilenameOptions BaseFilenames;

	AchievementsOptions Achievements;

	// Memorycard options - first 2 are default slots, last 6 are multitap 1 and 2
	// slots (3 each)
	McdOptions Mcd[8];
	std::string GzipIsoIndexTemplate; // for quick-access index with gzipped ISO

	int PINESlot;

	int RtcYear;
	int RtcMonth;
	int RtcDay;
	int RtcHour;
	int RtcMinute;
	int RtcSecond;

	// Set at runtime, not loaded from config.
	std::string CurrentBlockdump;
	std::string CurrentIRX;
	std::string CurrentGameArgs;
	AspectRatioType CurrentAspectRatio = AspectRatioType::RAuto4_3_3_2;
	// Fall back aspect ratio for games that have patches (when AspectRatioType::RAuto4_3_3_2) is active.
	float CurrentCustomAspectRatio = 0.f;
	bool IsPortableMode = false;

	Pcsx2Config();
	void LoadSave(SettingsWrapper& wrap);
	void LoadSaveCore(SettingsWrapper& wrap);
	void LoadSaveMemcards(SettingsWrapper& wrap);

	/// Reloads options affected by patches.
	void ReloadPatchAffectingOptions();

	std::string FullpathToBios() const;
	std::string FullpathToMcd(uint slot) const;

	bool operator==(const Pcsx2Config& right) const = delete;
	bool operator!=(const Pcsx2Config& right) const = delete;

	/// Copies runtime configuration settings (e.g. frame limiter state).
	void CopyRuntimeConfig(Pcsx2Config& cfg);

	/// Copies configuration from one file to another. Does not copy controller settings.
	static void CopyConfiguration(SettingsInterface* dest_si, SettingsInterface& src_si);

	/// Clears all core keys from the specified interface.
	static void ClearConfiguration(SettingsInterface* dest_si);

	/// Removes keys that are not valid for per-game settings.
	static void ClearInvalidPerGameConfiguration(SettingsInterface* si);
};

extern Pcsx2Config EmuConfig;

namespace EmuFolders
{
	extern std::string AppRoot;
	extern std::string DataRoot;
	extern std::string Settings;
	extern std::string Bios;
	extern std::string Snapshots;
	extern std::string Savestates;
	extern std::string MemoryCards;
	extern std::string Logs;
	extern std::string Cheats;
	extern std::string Patches;
	extern std::string Resources;
	extern std::string UserResources;
	extern std::string Cache;
	extern std::string Covers;
	extern std::string GameSettings;
	extern std::string Textures;
	extern std::string InputProfiles;
	extern std::string Videos;
	extern std::string DebuggerLayouts;
	extern std::string DebuggerSettings;

	/// Initializes critical folders (AppRoot, DataRoot, Settings). Call once on startup.
	void SetAppRoot();
	bool SetResourcesDirectory();
	bool SetDataDirectory(Error* error);

	// Assumes that AppRoot and DataRoot have been initialized.
	void SetDefaults(SettingsInterface& si);
	void LoadConfig(SettingsInterface& si);
	bool EnsureFoldersExist();

	/// Opens the specified log file for writing.
	std::FILE* OpenLogFile(std::string_view name, const char* mode);

	/// Returns the path to a resource file, allowing the user to override it.
	std::string GetOverridableResourcePath(std::string_view name);
} // namespace EmuFolders

/////////////////////////////////////////////////////////////////////////////////////////
// Helper Macros for Reading Emu Configurations.
//

// ------------ CPU / Recompiler Options ---------------

#ifdef _M_X86 // TODO(Stenzek): Remove me once EE/VU/IOP recs are added.
#define THREAD_VU1 (EmuConfig.Cpu.Recompiler.EnableVU1 && EmuConfig.Speedhacks.vuThread)
#else
#define THREAD_VU1 false
#endif
#define INSTANT_VU1 (EmuConfig.Speedhacks.vu1Instant)
#define CHECK_EEREC (EmuConfig.Cpu.Recompiler.EnableEE)
#define CHECK_CACHE (EmuConfig.Cpu.Recompiler.EnableEECache)
#define CHECK_IOPREC (EmuConfig.Cpu.Recompiler.EnableIOP)
#define CHECK_FASTMEM (EmuConfig.Cpu.Recompiler.EnableEE && EmuConfig.Cpu.Recompiler.EnableFastmem)
#define CHECK_EXTRAMEM (memGetExtraMemMode())

//------------ SPECIAL GAME FIXES!!! ---------------
#define CHECK_VUADDSUBHACK (EmuConfig.Gamefixes.VuAddSubHack) // Special Fix for Tri-ace games, they use an encryption algorithm that requires VU addi opcode to be bit-accurate.
#define CHECK_FPUMULHACK (EmuConfig.Gamefixes.FpuMulHack) // Special Fix for Tales of Destiny hangs.
#define CHECK_XGKICKHACK (EmuConfig.Gamefixes.XgKickHack) // Special Fix for Erementar Gerad, adds more delay to VU XGkick instructions. Corrects the color of some graphics.
#define CHECK_EETIMINGHACK (EmuConfig.Gamefixes.EETimingHack) // Fix all scheduled events to happen in 1 cycle.
#define CHECK_INSTANTDMAHACK (EmuConfig.Gamefixes.InstantDMAHack) // Attempt to finish DMA's instantly, useful for games which rely on cache emulation.
#define CHECK_SKIPMPEGHACK (EmuConfig.Gamefixes.SkipMPEGHack) // Finds sceMpegIsEnd pattern to tell the game the mpeg is finished (Katamari and a lot of games need this)
#define CHECK_OPHFLAGHACK (EmuConfig.Gamefixes.OPHFlagHack) // Bleach Blade Battlers
#define CHECK_DMABUSYHACK (EmuConfig.Gamefixes.DMABusyHack) // Denies writes to the DMAC when it's busy. This is correct behaviour but bad timing can cause problems.
#define CHECK_VIFFIFOHACK (EmuConfig.Gamefixes.VIFFIFOHack) // Pretends to fill the non-existant VIF FIFO Buffer.
#define CHECK_VIF1STALLHACK (EmuConfig.Gamefixes.VIF1StallHack) // Like above, processes FIFO data before the stall is allowed (to make sure data goes over).
#define CHECK_GIFFIFOHACK (EmuConfig.Gamefixes.GIFFIFOHack) // Enabled the GIF FIFO (more correct but slower)
#define CHECK_VUOVERFLOWHACK (EmuConfig.Gamefixes.VUOverflowHack) // Special Fix for Superman Returns, they check for overflows on PS2 floats which we can't do without soft floats.
#define CHECK_FULLVU0SYNCHACK (EmuConfig.Gamefixes.FullVU0SyncHack)

//------------ Advanced Options!!! ---------------
#define CHECK_VU_OVERFLOW(vunum) (((vunum) == 0) ? EmuConfig.Cpu.Recompiler.vu0Overflow : EmuConfig.Cpu.Recompiler.vu1Overflow)
#define CHECK_VU_EXTRA_OVERFLOW(vunum) (((vunum) == 0) ? EmuConfig.Cpu.Recompiler.vu0ExtraOverflow : EmuConfig.Cpu.Recompiler.vu1ExtraOverflow) // If enabled, Operands are clamped before being used in the VU recs
#define CHECK_VU_SIGN_OVERFLOW(vunum) (((vunum) == 0) ? EmuConfig.Cpu.Recompiler.vu0SignOverflow : EmuConfig.Cpu.Recompiler.vu1SignOverflow)
#define CHECK_VU_UNDERFLOW(vunum) (((vunum) == 0) ? EmuConfig.Cpu.Recompiler.vu0Underflow : EmuConfig.Cpu.Recompiler.vu1Underflow)

#define CHECK_FPU_OVERFLOW (EmuConfig.Cpu.Recompiler.fpuOverflow)
#define CHECK_FPU_EXTRA_OVERFLOW (EmuConfig.Cpu.Recompiler.fpuExtraOverflow) // If enabled, Operands are checked for infinities before being used in the FPU recs
#define CHECK_FPU_EXTRA_FLAGS 1 // Always enabled now // Sets D/I flags on FPU instructions
#define CHECK_FPU_FULL (EmuConfig.Cpu.Recompiler.fpuFullMode)

//------------ EE Recompiler defines - Comment to disable a recompiler ---------------

#define SHIFT_RECOMPILE // Speed majorly reduced if disabled
#define BRANCH_RECOMPILE // Speed extremely reduced if disabled - more then shift

// Disabling all the recompilers in this block is interesting, as it still runs at a reasonable rate.
// It also adds a few glitches. Really reminds me of the old Linux 64-bit version. --arcum42
#define ARITHMETICIMM_RECOMPILE
#define ARITHMETIC_RECOMPILE
#define MULTDIV_RECOMPILE
#define JUMP_RECOMPILE
#define LOADSTORE_RECOMPILE
#define MOVE_RECOMPILE
#define MMI_RECOMPILE
#define MMI0_RECOMPILE
#define MMI1_RECOMPILE
#define MMI2_RECOMPILE
#define MMI3_RECOMPILE
#define FPU_RECOMPILE
#define CP0_RECOMPILE
#define CP2_RECOMPILE

// You can't recompile ARITHMETICIMM without ARITHMETIC.
#ifndef ARITHMETIC_RECOMPILE
#undef ARITHMETICIMM_RECOMPILE
#endif

#define EE_CONST_PROP 1 // rec2 - enables constant propagation (faster)

// Change to 1 for console logs of SIF, GPU (PS1 mode) and MDEC (PS1 mode).
// These do spam a lot though!
#define PSX_EXTRALOGS 0

#undef BITFIELD32
#undef BITFIELD_END
