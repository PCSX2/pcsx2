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

#include <wx/fileconf.h>

#include "common/IniInterface.h"
#include "Config.h"
#include "GS.h"
#include "CDVD/CDVDaccess.h"
#include "MemoryCardFile.h"

#ifndef PCSX2_CORE
#include "gui/AppConfig.h"
#endif

void TraceLogFilters::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"TraceLog" );

	IniEntry( Enabled );
	
	// Retaining backwards compat of the trace log enablers isn't really important, and
	// doing each one by hand would be murder.  So let's cheat and just save it as an int:

	IniEntry( EE.bitset );
	IniEntry( IOP.bitset );
}

const wxChar* const tbl_SpeedhackNames[] =
	{
		L"mvuFlag",
		L"InstantVU1"};

const __fi wxChar* EnumToString(SpeedhackId id)
{
	return tbl_SpeedhackNames[id];
}

void Pcsx2Config::SpeedhackOptions::Set(SpeedhackId id, bool enabled)
{
	EnumAssert(id);
	switch (id)
	{
		case Speedhack_mvuFlag:
			vuFlagHack = enabled;
			break;
		case Speedhack_InstantVU1:
			vu1Instant = enabled;
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

void Pcsx2Config::SpeedhackOptions::LoadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"Speedhacks");

	IniBitfield(EECycleRate);
	IniBitfield(EECycleSkip);
	IniBitBool(fastCDVD);
	IniBitBool(IntcStat);
	IniBitBool(WaitLoop);
	IniBitBool(vuFlagHack);
	IniBitBool(vuThread);
	IniBitBool(vu1Instant);
}

void Pcsx2Config::ProfilerOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"Profiler" );

	IniBitBool( Enabled );
	IniBitBool( RecBlocks_EE );
	IniBitBool( RecBlocks_IOP );
	IniBitBool( RecBlocks_VU0 );
	IniBitBool( RecBlocks_VU1 );
}

Pcsx2Config::RecompilerOptions::RecompilerOptions()
{
	bitset		= 0;

	//StackFrameChecks	= false;
	//PreBlockCheckEE	= false;

	// All recs are enabled by default.

	EnableEE	= true;
	EnableEECache = false;
	EnableIOP	= true;
	EnableVU0	= true;
	EnableVU1	= true;

	// vu and fpu clamping default to standard overflow.
	vuOverflow	= true;
	//vuExtraOverflow = false;
	//vuSignOverflow = false;
	//vuUnderflow = false;

	fpuOverflow	= true;
	//fpuExtraOverflow = false;
	//fpuFullMode = false;
}

void Pcsx2Config::RecompilerOptions::ApplySanityCheck()
{
	bool fpuIsRight = true;

	if( fpuExtraOverflow )
		fpuIsRight = fpuOverflow;

	if( fpuFullMode )
		fpuIsRight = fpuOverflow && fpuExtraOverflow;

	if( !fpuIsRight )
	{
		// Values are wonky; assume the defaults.
		fpuOverflow		= RecompilerOptions().fpuOverflow;
		fpuExtraOverflow= RecompilerOptions().fpuExtraOverflow;
		fpuFullMode		= RecompilerOptions().fpuFullMode;
	}

	bool vuIsOk = true;

	if( vuExtraOverflow ) vuIsOk = vuIsOk && vuOverflow;
	if( vuSignOverflow ) vuIsOk = vuIsOk && vuExtraOverflow;

	if( !vuIsOk )
	{
		// Values are wonky; assume the defaults.
		vuOverflow		= RecompilerOptions().vuOverflow;
		vuExtraOverflow	= RecompilerOptions().vuExtraOverflow;
		vuSignOverflow	= RecompilerOptions().vuSignOverflow;
		vuUnderflow		= RecompilerOptions().vuUnderflow;
	}
}

void Pcsx2Config::RecompilerOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"Recompiler" );

	IniBitBool( EnableEE );
	IniBitBool( EnableIOP );
	IniBitBool( EnableEECache );
	IniBitBool( EnableVU0 );
	IniBitBool( EnableVU1 );

	IniBitBool( vuOverflow );
	IniBitBool( vuExtraOverflow );
	IniBitBool( vuSignOverflow );
	IniBitBool( vuUnderflow );

	IniBitBool( fpuOverflow );
	IniBitBool( fpuExtraOverflow );
	IniBitBool( fpuFullMode );

	IniBitBool( StackFrameChecks );
	IniBitBool( PreBlockCheckEE );
	IniBitBool( PreBlockCheckIOP );
}

Pcsx2Config::CpuOptions::CpuOptions()
{
	sseMXCSR.bitmask	= DEFAULT_sseMXCSR;
	sseVUMXCSR.bitmask	= DEFAULT_sseVUMXCSR;
}

void Pcsx2Config::CpuOptions::ApplySanityCheck()
{
	sseMXCSR.ClearExceptionFlags().DisableExceptions();
	sseVUMXCSR.ClearExceptionFlags().DisableExceptions();

	Recompiler.ApplySanityCheck();
}

void Pcsx2Config::CpuOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"CPU" );

	IniBitBoolEx( sseMXCSR.DenormalsAreZero,	"FPU.DenormalsAreZero" );
	IniBitBoolEx( sseMXCSR.FlushToZero,			"FPU.FlushToZero" );
	IniBitfieldEx( sseMXCSR.RoundingControl,	"FPU.Roundmode" );

	IniBitBoolEx( sseVUMXCSR.DenormalsAreZero,	"VU.DenormalsAreZero" );
	IniBitBoolEx( sseVUMXCSR.FlushToZero,		"VU.FlushToZero" );
	IniBitfieldEx( sseVUMXCSR.RoundingControl,	"VU.Roundmode" );

	Recompiler.LoadSave( ini );
}

void Pcsx2Config::GSOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"GS" );

#ifdef  PCSX2_DEVBUILD 
	IniEntry( SynchronousMTGS );
#endif
	IniEntry( VsyncQueueSize );

	IniEntry( FrameLimitEnable );
	IniEntry( FrameSkipEnable );
	ini.EnumEntry( L"VsyncEnable", VsyncEnable, NULL, VsyncEnable );

	IniEntry( LimitScalar );
	IniEntry( FramerateNTSC );
	IniEntry( FrameratePAL );

	IniEntry( FramesToDraw );
	IniEntry( FramesToSkip );

	static const wxChar* AspectRatioNames[] =
		{
			L"Stretch",
			L"4:3",
			L"16:9",
			// WARNING: array must be NULL terminated to compute it size
			NULL};

#ifdef PCSX2_CORE
	ini.EnumEntry(L"AspectRatio", AspectRatio, AspectRatioNames, AspectRatio);

	static const wxChar* FMVAspectRatioSwitchNames[] =
		{
			L"Off",
			L"4:3",
			L"16:9",
			// WARNING: array must be NULL terminated to compute it size
			NULL};
	ini.EnumEntry(L"FMVAspectRatioSwitch", FMVAspectRatioSwitch, FMVAspectRatioSwitchNames, FMVAspectRatioSwitch);

	IniEntry(Zoom);
#endif
}

int Pcsx2Config::GSOptions::GetVsync() const
{
	if (EmuConfig.LimiterMode == LimiterModeType::Turbo || !FrameLimitEnable)
		return 0;

	// D3D only support a boolean state. OpenGL waits a number of vsync
	// interrupt (negative value for late vsync).
	switch (VsyncEnable) {
		case VsyncMode::Adaptive: return -1;
		case VsyncMode::Off: return 0;
		case VsyncMode::On: return 1;

		default: return 0;
	}
}

const wxChar *const tbl_GamefixNames[] =
{
	L"FpuMul",
	L"FpuNegDiv",
	L"GoemonTlb",
	L"SkipMPEG",
	L"OPHFlag",
	L"EETiming",
	L"DMABusy",
	L"GIFFIFO",
	L"VIFFIFO",
	L"VIF1Stall",
	L"VuAddSub",
	L"Ibit",
	L"VUKickstart",
	L"VUOverflow",
	L"XGKick"
};

const __fi wxChar* EnumToString( GamefixId id )
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

// Enables a full list of gamefixes.  The list can be either comma or pipe-delimited.
//   Example:  "XGKick,IpuWait"  or  "EEtiming,FpuCompare"
// If an unrecognized tag is encountered, a warning is printed to the console, but no error
// is generated.  This allows the system to function in the event that future versions of
// PCSX2 remove old hacks once they become obsolete.
void Pcsx2Config::GamefixOptions::Set( const wxString& list, bool enabled )
{
	wxStringTokenizer izer( list, L",|", wxTOKEN_STRTOK );
	
	while( izer.HasMoreTokens() )
	{
		wxString token( izer.GetNextToken() );

		GamefixId i;
		for (i=GamefixId_FIRST; i < pxEnumEnd; ++i)
		{
			if( token.CmpNoCase( EnumToString(i) ) == 0 ) break;
		}
		if( i < pxEnumEnd ) Set( i );
	}
}

void Pcsx2Config::GamefixOptions::Set( GamefixId id, bool enabled )
{
	EnumAssert( id );
	switch(id)
	{
		case Fix_VuAddSub:		VuAddSubHack		= enabled;	break;
		case Fix_FpuMultiply:	FpuMulHack			= enabled;	break;
		case Fix_FpuNegDiv:		FpuNegDivHack		= enabled;	break;
		case Fix_XGKick:		XgKickHack			= enabled;	break;
		case Fix_EETiming:		EETimingHack		= enabled;	break;
		case Fix_SkipMpeg:		SkipMPEGHack		= enabled;	break;
		case Fix_OPHFlag:		OPHFlagHack			= enabled;  break;
		case Fix_DMABusy:		DMABusyHack			= enabled;  break;
		case Fix_VIFFIFO:		VIFFIFOHack			= enabled;  break;
		case Fix_VIF1Stall:		VIF1StallHack		= enabled;  break;
		case Fix_GIFFIFO:		GIFFIFOHack			= enabled;  break;
		case Fix_GoemonTlbMiss: GoemonTlbHack		= enabled;  break;
		case Fix_Ibit:			IbitHack			= enabled;  break;
		case Fix_VUKickstart:	VUKickstartHack		= enabled;  break;
		case Fix_VUOverflow:	VUOverflowHack		= enabled;  break;
		jNO_DEFAULT;
	}
}

bool Pcsx2Config::GamefixOptions::Get( GamefixId id ) const
{
	EnumAssert( id );
	switch(id)
	{
		case Fix_VuAddSub:		return VuAddSubHack;
		case Fix_FpuMultiply:	return FpuMulHack;
		case Fix_FpuNegDiv:		return FpuNegDivHack;
		case Fix_XGKick:		return XgKickHack;
		case Fix_EETiming:		return EETimingHack;
		case Fix_SkipMpeg:		return SkipMPEGHack;
		case Fix_OPHFlag:		return OPHFlagHack;
		case Fix_DMABusy:		return DMABusyHack;
		case Fix_VIFFIFO:		return VIFFIFOHack;
		case Fix_VIF1Stall:		return VIF1StallHack;
		case Fix_GIFFIFO:		return GIFFIFOHack;
		case Fix_GoemonTlbMiss: return GoemonTlbHack;
		case Fix_Ibit:			return IbitHack;
		case Fix_VUKickstart:	return VUKickstartHack;
		case Fix_VUOverflow:	return VUOverflowHack;
		jNO_DEFAULT;
	}
	return false;		// unreachable, but we still need to suppress warnings >_<
}

void Pcsx2Config::GamefixOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"Gamefixes" );

	IniBitBool( VuAddSubHack );
	IniBitBool( FpuMulHack );
	IniBitBool( FpuNegDivHack );
	IniBitBool( XgKickHack );
	IniBitBool( EETimingHack );
	IniBitBool( SkipMPEGHack );
	IniBitBool( OPHFlagHack );
	IniBitBool( DMABusyHack );
	IniBitBool( VIFFIFOHack );
	IniBitBool( VIF1StallHack );
	IniBitBool( GIFFIFOHack );
	IniBitBool( GoemonTlbHack );
	IniBitBool( IbitHack );
	IniBitBool( VUKickstartHack );
	IniBitBool( VUOverflowHack );
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

void Pcsx2Config::DebugOptions::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"Debugger" );

	IniBitBool( ShowDebuggerOnStart );
	IniBitBool( AlignMemoryWindowStart );
	IniBitfield( FontWidth );
	IniBitfield( FontHeight );
	IniBitfield( WindowWidth );
	IniBitfield( WindowHeight );
	IniBitfield( MemoryViewBytesPerRow );
}

void Pcsx2Config::FilenameOptions::LoadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"Filenames");

	static const wxFileName pc(L"Please Configure");

	//when saving in portable mode, we just save the non-full-path filename
	//  --> on load they'll be initialized with default (relative) paths (works for bios)
	//note: this will break if converting from install to portable, and custom folders are used. We can live with that.
#ifndef PCSX2_CORE
	bool needRelativeName = ini.IsSaving() && IsPortable();
#else
	bool needRelativeName = ini.IsSaving();
#endif

	if (needRelativeName)
	{
		wxFileName bios_filename = wxFileName(Bios.GetFullName());
		ini.Entry(L"BIOS", bios_filename, pc);
	}
	else
		ini.Entry(L"BIOS", Bios, pc);
}

Pcsx2Config::FolderOptions::FolderOptions()
{

}

void Pcsx2Config::FramerateOptions::SanityCheck()
{
	// Ensure Conformation of various options...

	NominalScalar = std::clamp(NominalScalar, 0.05, 10.0);
	TurboScalar = std::clamp(TurboScalar, 0.05, 10.0);
	SlomoScalar = std::clamp(SlomoScalar, 0.05, 10.0);
}

void Pcsx2Config::FramerateOptions::LoadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"Framerate");

	IniEntry(NominalScalar);
	IniEntry(TurboScalar);
	IniEntry(SlomoScalar);

	IniEntry(SkipOnLimit);
	IniEntry(SkipOnTurbo);
}

Pcsx2Config::Pcsx2Config()
{
	bitset = 0;
	// Set defaults for fresh installs / reset settings
	McdEnableEjection = true;
	McdFolderAutoManage = true;
	EnablePatches = true;
	BackupSavestate = true;

#ifdef __WXMSW__
	McdCompressNTFS = true;
#endif

	// To be moved to FileMemoryCard pluign (someday)
	for (uint slot = 0; slot < 8; ++slot)
	{
		Mcd[slot].Enabled = !FileMcd_IsMultitapSlot(slot); // enables main 2 slots
		Mcd[slot].Filename = FileMcd_GetDefaultName(slot);

		// Folder memory card is autodetected later.
		Mcd[slot].Type = MemoryCardType::MemoryCard_File;
	}

	GzipIsoIndexTemplate = "$(f).pindex.tmp";

	CdvdSource = CDVD_SourceType::Iso;
}

void Pcsx2Config::LoadSave( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"EmuCore" );

	IniBitBool( CdvdVerboseReads );
	IniBitBool( CdvdDumpBlocks );
	IniBitBool( CdvdShareWrite );
	IniBitBool( EnablePatches );
	IniBitBool( EnableCheats );
	IniBitBool( EnableIPC );
	IniBitBool( EnableWideScreenPatches );
#ifndef DISABLE_RECORDING
	IniBitBool( EnableRecordingTools );
#endif
	IniBitBool( ConsoleToStdio );
	IniBitBool( HostFs );

	IniBitBool( BackupSavestate );
	IniBitBool( McdEnableEjection );
	IniBitBool( McdFolderAutoManage );
	IniBitBool( MultitapPort0_Enabled );
	IniBitBool( MultitapPort1_Enabled );

	// Process various sub-components:

	Speedhacks		.LoadSave( ini );
	Cpu				.LoadSave( ini );
	GS				.LoadSave( ini );
	Gamefixes		.LoadSave( ini );
	Profiler		.LoadSave( ini );

	Debugger		.LoadSave( ini );
	Trace			.LoadSave( ini );

	IniEntry(GzipIsoIndexTemplate);

	// For now, this in the derived config for backwards ini compatibility.
#ifdef PCSX2_CORE
	BaseFilenames.LoadSave(ini);
	Framerate.LoadSave(ini);
	LoadSaveMemcards(ini);

	IniEntry(GzipIsoIndexTemplate);

#ifdef __WXMSW__
	IniEntry(McdCompressNTFS);
#endif
#endif

	if (ini.IsLoading())
	{
		CurrentAspectRatio = GS.AspectRatio;
	}

	ini.Flush();
}

void Pcsx2Config::LoadSaveMemcards( IniInterface& ini )
{
	ScopedIniGroup path( ini, L"MemoryCards" );

	for( uint slot=0; slot<2; ++slot )
	{
		ini.Entry( pxsFmt( L"Slot%u_Enable", slot+1 ),
			Mcd[slot].Enabled, Mcd[slot].Enabled );
		ini.Entry( pxsFmt( L"Slot%u_Filename", slot+1 ),
			Mcd[slot].Filename, Mcd[slot].Filename );
	}

	for( uint slot=2; slot<8; ++slot )
	{
		int mtport = FileMcd_GetMtapPort(slot)+1;
		int mtslot = FileMcd_GetMtapSlot(slot)+1;

		ini.Entry( pxsFmt( L"Multitap%u_Slot%u_Enable", mtport, mtslot ),
			Mcd[slot].Enabled, Mcd[slot].Enabled );
		ini.Entry( pxsFmt( L"Multitap%u_Slot%u_Filename", mtport, mtslot ),
			Mcd[slot].Filename, Mcd[slot].Filename );
	}
}

bool Pcsx2Config::MultitapEnabled( uint port ) const
{
	pxAssert( port < 2 );
	return (port==0) ? MultitapPort0_Enabled : MultitapPort1_Enabled;
}

void Pcsx2Config::Load( const wxString& srcfile )
{
	//m_IsLoaded = true;

	wxFileConfig cfg( srcfile );
	IniLoader loader( cfg );
	LoadSave( loader );
}

void Pcsx2Config::Save( const wxString& dstfile )
{
	//if( !m_IsLoaded ) return;

	wxFileConfig cfg( dstfile );
	IniSaver saver( cfg );
	LoadSave( saver );
}

wxString Pcsx2Config::FullpathToBios() const
{
	return Path::Combine(Folders.Bios, BaseFilenames.Bios);
}

wxString Pcsx2Config::FullpathToMcd(uint slot) const
{
	return Path::Combine(Folders.MemoryCards, Mcd[slot].Filename);
}

void Pcsx2Config::CopyConfig(const Pcsx2Config& cfg)
{
	Cpu = cfg.Cpu;
	GS = cfg.GS;
	Speedhacks = cfg.Speedhacks;
	Gamefixes = cfg.Gamefixes;
	Profiler = cfg.Profiler;
	Debugger = cfg.Debugger;
	Trace = cfg.Trace;
	BaseFilenames = cfg.BaseFilenames;
	Framerate = cfg.Framerate;
	for (u32 i = 0; i < sizeof(Mcd) / sizeof(Mcd[0]); i++)
		Mcd[i] = cfg.Mcd[i];

	GzipIsoIndexTemplate = cfg.GzipIsoIndexTemplate;

	CdvdVerboseReads = cfg.CdvdVerboseReads;
	CdvdDumpBlocks = cfg.CdvdDumpBlocks;
	CdvdShareWrite = cfg.CdvdShareWrite;
	EnablePatches = cfg.EnablePatches;
	EnableCheats = cfg.EnableCheats;
	EnableIPC = cfg.EnableIPC;
	EnableWideScreenPatches = cfg.EnableWideScreenPatches;
#ifndef DISABLE_RECORDING
	EnableRecordingTools = cfg.EnableRecordingTools;
#endif
	UseBOOT2Injection = cfg.UseBOOT2Injection;
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
}
