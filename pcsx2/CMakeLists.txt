# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

if(XDG_STD)
    add_definitions(-DXDG_STD)
endif(XDG_STD)

# Main pcsx2 source
set(pcsx2Sources
	Cache.cpp
	COP0.cpp
	COP2.cpp
	Counters.cpp
	GameDatabase.cpp
	Dump.cpp
	Elfheader.cpp
	FiFo.cpp
	FPU.cpp
	Gif.cpp
    Gif_Logger.cpp
	Gif_Unit.cpp
    Gte.cpp
	GS.cpp
	GSState.cpp
	Hw.cpp
	HwRead.cpp
	HwWrite.cpp
	Interpreter.cpp
	IopBios.cpp
	IopCounters.cpp
	IopDma.cpp
	IopHw.cpp
	IopIrq.cpp
	IopMem.cpp
	IopSio2.cpp
#	Mdec.cpp
	Memory.cpp
	MMI.cpp
	MTGS.cpp
    MTVU.cpp
    MultipartFileReader.cpp
    OutputIsoFile.cpp
	Patch.cpp
	Patch_Memory.cpp
	Pcsx2Config.cpp
	PluginManager.cpp
	PrecompiledHeader.cpp
	R3000A.cpp
	R3000AInterpreter.cpp
	R3000AOpcodeTables.cpp
	R5900.cpp
	R5900OpcodeImpl.cpp
	R5900OpcodeTables.cpp
	SaveState.cpp
	ShiftJisToUnicode.cpp
	Sif.cpp
	Sif0.cpp
	Sif1.cpp
	sif2.cpp
	Sio.cpp
	SourceLog.cpp
	SPR.cpp
	Stats.cpp
	System.cpp
	Vif0_Dma.cpp
	Vif1_Dma.cpp
	Vif1_MFIFO.cpp
	Vif.cpp
	Vif_Codes.cpp
	Vif_Transfer.cpp
	Vif_Unpack.cpp
	vtlb.cpp
	VU0.cpp
	VUmicro.cpp
	VU0micro.cpp
	VU0microInterp.cpp
	VU1micro.cpp
	VU1microInterp.cpp
	VUflags.cpp
	VUmicroMem.cpp
	VUops.cpp)

# CDVD sources
set(pcsx2CDVDSources
	CDVD/BlockdumpFileReader.cpp
	CDVD/CdRom.cpp
	CDVD/CDVDaccess.cpp
	CDVD/CDVD.cpp
	CDVD/CDVDisoReader.cpp
	CDVD/InputIsoFile.cpp
	CDVD/OutputIsoFile.cpp
	CDVD/CompressedFileReader.cpp
	CDVD/IsoFS/IsoFile.cpp
	CDVD/IsoFS/IsoFSCDVD.cpp
	CDVD/IsoFS/IsoFS.cpp
    )

# DebugTools sources
set(pcsx2DebugToolsSources
	DebugTools/DebugInterface.cpp
	DebugTools/DisassemblyManager.cpp
	DebugTools/ExpressionParser.cpp
	DebugTools/MIPSAnalyst.cpp
	DebugTools/Breakpoints.cpp
	DebugTools/SymbolMap.cpp
	DebugTools/DisR3000A.cpp
	DebugTools/DisR5900asm.cpp
	DebugTools/DisR5900.cpp
	DebugTools/DisVU0Micro.cpp
	DebugTools/DisVU1Micro.cpp)

# gui sources
set(pcsx2GuiSources
	gui/AppAssert.cpp
	gui/AppConfig.cpp
	gui/AppCorePlugins.cpp
	gui/AppCoreThread.cpp
	gui/AppEventSources.cpp
	gui/AppGameDatabase.cpp
	gui/AppUserMode.cpp
	gui/AppInit.cpp
	gui/AppMain.cpp
	gui/AppRes.cpp
	gui/ConsoleLogger.cpp
	gui/CpuUsageProvider.cpp
	gui/CpuUsageProviderLnx.cpp
	gui/Dialogs/AboutBoxDialog.cpp
	gui/Dialogs/AppConfigDialog.cpp
	gui/Dialogs/AssertionDialog.cpp
	gui/Panels/BaseApplicableConfigPanel.cpp
	gui/Panels/MemoryCardListView.cpp
	gui/Panels/ThemeSelectorPanel.cpp
	gui/Dialogs/BaseConfigurationDialog.cpp
	gui/Dialogs/BiosSelectorDialog.cpp
	gui/Dialogs/ConfirmationDialogs.cpp
	gui/Dialogs/CreateMemoryCardDialog.cpp
	gui/Dialogs/FirstTimeWizard.cpp
	gui/Dialogs/GameDatabaseDialog.cpp
	gui/Dialogs/ImportSettingsDialog.cpp
	gui/Dialogs/LogOptionsDialog.cpp
	gui/Dialogs/McdConfigDialog.cpp
	gui/Dialogs/PickUserModeDialog.cpp
	gui/Dialogs/StuckThreadDialog.cpp
	gui/Dialogs/SysConfigDialog.cpp
	gui/Debugger/BreakpointWindow.cpp
	gui/Debugger/CtrlDisassemblyView.cpp
	gui/Debugger/CtrlRegisterList.cpp
	gui/Debugger/CtrlMemView.cpp
	gui/Debugger/DebuggerLists.cpp
	gui/Debugger/DisassemblyDialog.cpp
	gui/Debugger/DebugEvents.cpp
	gui/ExecutorThread.cpp
	gui/FrameForGS.cpp
	gui/GlobalCommands.cpp
	gui/i18n.cpp
	gui/IsoDropTarget.cpp
	gui/MainFrame.cpp
	gui/MainMenuClicks.cpp
	gui/MemoryCardFile.cpp
	gui/Panels/BaseApplicableConfigPanel.cpp
	gui/Panels/MemoryCardListPanel.cpp
	gui/MessageBoxes.cpp
	gui/MSWstuff.cpp
	gui/Panels/AudioPanel.cpp
	gui/Panels/BiosSelectorPanel.cpp
	gui/Panels/CpuPanel.cpp
	gui/Panels/DirPickerPanel.cpp
	gui/Panels/GameDatabasePanel.cpp
	gui/Panels/GameFixesPanel.cpp
	gui/Panels/GSWindowPanel.cpp
	gui/Panels/LogOptionsPanels.cpp
	gui/Panels/MiscPanelStuff.cpp
	gui/Panels/PathsPanel.cpp
	gui/Panels/PluginSelectorPanel.cpp
	gui/Panels/SpeedhacksPanel.cpp
	gui/Panels/VideoPanel.cpp
	gui/pxLogTextCtrl.cpp
	gui/RecentIsoList.cpp
	gui/Saveslots.cpp
	gui/SysState.cpp
	gui/UpdateUI.cpp
	)

# Warning: the declaration of the .h are mandatory in case of resources files. It will ensure the creation
# from the bin2cpp tools at the good moment (ie .h must be created before the pcsx2 compilation)
# Gui resources headers
set(res_bin "${CMAKE_BINARY_DIR}/pcsx2/gui/Resources")
set(res_src "${CMAKE_SOURCE_DIR}/pcsx2/gui/Resources")
set(pcsx2GuiResources
    ${res_bin}/AppIcon16.h
    ${res_bin}/AppIcon32.h
    ${res_bin}/AppIcon64.h
    ${res_bin}/BackgroundLogo.h
    ${res_bin}/ConfigIcon_Appearance.h
    ${res_bin}/ButtonIcon_Camera.h
    ${res_bin}/ConfigIcon_Cpu.h
    ${res_bin}/ConfigIcon_Gamefixes.h
    ${res_bin}/ConfigIcon_MemoryCard.h
    ${res_bin}/ConfigIcon_Paths.h
    ${res_bin}/ConfigIcon_Plugins.h
    ${res_bin}/ConfigIcon_Speedhacks.h
    ${res_bin}/ConfigIcon_Video.h
    ${res_bin}/Dualshock.h
    ${res_bin}/Breakpoint_Active.h
    ${res_bin}/Breakpoint_Inactive.h
    )

# IPU sources
set(pcsx2IPUSources
	IPU/IPU.cpp
	IPU/IPU_Fifo.cpp
	IPU/IPUdma.cpp
	IPU/mpeg2lib/Idct.cpp
	IPU/mpeg2lib/Mpeg.cpp
	IPU/yuv2rgb.cpp)

# Linux sources
set(pcsx2LinuxSources
	Linux/LnxKeyCodes.cpp
    Linux/LnxFlatFileReader.cpp
    )

# ps2 sources
set(pcsx2ps2Sources
	ps2/BiosTools.cpp
	ps2/LegacyDmac.cpp
	ps2/Iop/IopHwRead.cpp
	ps2/Iop/IopHwWrite.cpp)

# RDebug sources
set(pcsx2RDebugSources
	RDebug/deci2.cpp
	RDebug/deci2_dbgp.cpp
	RDebug/deci2_dcmp.cpp
	RDebug/deci2_drfp.cpp
	RDebug/deci2_iloadp.cpp
	RDebug/deci2_netmp.cpp
	RDebug/deci2_ttyp.cpp)

# System sources
set(pcsx2SystemSources
	System/SysCoreThread.cpp
	System/SysThreadBase.cpp)

# Utilities sources
set(pcsx2UtilitiesSources
	Utilities/FileUtils.cpp)

# Zip tools utilies sources
set(pcsx2ZipToolsSources
    ZipTools/thread_gzip.cpp
    ZipTools/thread_lzma.cpp)

# Windows sources
set(pcsx2WindowsSources
	windows/cheats/browser.cpp
	windows/cheats/cheats.cpp
	windows/DwmSetup.cpp
	windows/ini.cpp
	windows/PatchBrowser.cpp
	windows/SampleProf.cp
	windows/WinCompressNTFS.cpp
	windows/WinConsolePipe.cpp
	windows/WinSysExec.cpp)

# x86 sources
set(pcsx2x86Sources
	x86/BaseblockEx.cpp
	x86/iCOP0.cpp
	x86/iCore.cpp
	x86/iFPU.cpp
	x86/iFPUd.cpp
	x86/iMisc.cpp
	x86/iMMI.cpp
	x86/iPsxMem.cpp
	x86/iR3000A.cpp
	x86/iR3000Atables.cpp
	x86/iR5900Misc.cpp
	x86/ir5900tables.cpp
	x86/iVU1micro.cpp
	x86/ix86-32/iCore-32.cpp
	x86/ix86-32/iR5900-32.cpp
	x86/ix86-32/iR5900Arit.cpp
	x86/ix86-32/iR5900AritImm.cpp
	x86/ix86-32/iR5900Branch.cpp
	x86/ix86-32/iR5900Jump.cpp
	x86/ix86-32/iR5900LoadStore.cpp
	x86/ix86-32/iR5900Move.cpp
	x86/ix86-32/iR5900MultDiv.cpp
	x86/ix86-32/iR5900Shift.cpp
	x86/ix86-32/iR5900Templates.cpp
	x86/ix86-32/recVTLB.cpp
	x86/newVif_Dynarec.cpp
	x86/newVif_Unpack.cpp
	x86/newVif_UnpackSSE.cpp
	x86/sVU_Lower.cpp
	x86/sVU_Micro.cpp
	x86/sVU_Upper.cpp
	x86/sVU_zerorec.cpp
	)
# x86 headers
set(pcsx2x86Headers
	x86/BaseblockEx.h
	x86/iCOP0.h
	x86/iCore.h
	x86/iFPU.h
	x86/iMMI.h
	x86/iR3000A.h
	x86/iR5900Arit.h
	x86/iR5900AritImm.h
	x86/iR5900Branch.h
	x86/iR5900.h
	x86/iR5900Jump.h
	x86/iR5900LoadStore.h
	x86/iR5900Move.h
	x86/iR5900MultDiv.h
	x86/iR5900Shift.h
	x86/microVU.h
	x86/microVU_IR.h
	x86/microVU_Misc.h
	x86/microVU_Alloc.inl
	x86/microVU_Analyze.inl
	x86/microVU_Branch.inl
	x86/microVU_Clamp.inl
	x86/microVU_Compile.inl
	x86/microVU.cpp
	x86/microVU_Execute.inl
	x86/microVU_Flags.inl
	x86/microVU_Log.inl
	x86/microVU_Lower.inl
	x86/microVU_Macro.inl
	x86/microVU_Misc.inl
	x86/microVU_Tables.inl
	x86/microVU_Upper.inl
	x86/newVif.h
	x86/newVif_HashBucket.h
	x86/newVif_UnpackSSE.h
	x86/sVU_Compare.h
	x86/sVU_Debug.h
	x86/sVU_Micro.h
	x86/sVU_zerorec.h
	)

# colect .asm files
set(pcsx2AsmFiles
	IPU/yuv2rgb.asm
	x86/ix86-32/aVif_proc-32.asm)

# collect .S files
set(pcsx2SSources
	x86/aVUzerorec.S)

# change language of .S-files to c++
set_source_files_properties(${pcsx2SSources} PROPERTIES LANGUAGE CXX)

# common Sources
set(Common
	${pcsx2Sources}
	${pcsx2CDVDSources}
	${pcsx2DebugToolsSources}
	${pcsx2GuiSources}
    ${pcsx2GuiResources}
	${pcsx2IPUSources}
	${pcsx2ps2Sources}
	${pcsx2SystemSources}
	${pcsx2UtilitiesSources}
	${pcsx2x86Sources}
	${pcsx2ZipToolsSources}
	${pcsx2SSources}
	${pcsx2x86Headers}
	)

# platform sources
# Linux
if(Linux)
	set(Platform
		${pcsx2LinuxSources})
endif(Linux)		

# Windows
if(Windows)
	set(Platform
		${pcsx2WindowsSources})
endif(Windows)

# additonal include directories
include_directories(
					gui
					x86
                    ${CMAKE_BINARY_DIR}/pcsx2/gui
                    )
set(pcsx2FinalSources 
	${Common} 
	${Platform})
set(pcsx2FinalLibs
Utilities 
x86emitter 
aio
${wxWidgets_LIBRARIES} 
${GTK2_LIBRARIES} 
${ZLIB_LIBRARIES} 
${CMAKE_THREAD_LIBS_INIT}
	)

add_pcsx2_executable(pcsx2 "${pcsx2FinalSources}" "${pcsx2FinalLibs}")

### Generate the resources files
file(MAKE_DIRECTORY ${res_bin})

add_custom_command(OUTPUT  "${res_bin}/Dualshock.h"  COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl "${res_src}/Dualshock.jpg" "${res_bin}/Dualshock" )

foreach(res_file IN ITEMS
        AppIcon16 AppIcon32 AppIcon64 BackgroundLogo ButtonIcon_Camera
        ConfigIcon_Appearance ConfigIcon_Cpu ConfigIcon_Gamefixes ConfigIcon_MemoryCard
        ConfigIcon_Paths ConfigIcon_Plugins ConfigIcon_Speedhacks ConfigIcon_Video Breakpoint_Active Breakpoint_Inactive)
    add_custom_command(OUTPUT  "${res_bin}/${res_file}.h" COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl "${res_src}/${res_file}.png" "${res_bin}/${res_file}" )
endforeach()

# Suppress all the system-specific predefined macros outside the reserved namespace.
# Needed when stringifying macros.
# See issue: 1233
if(PACKAGE_MODE)
	SET_SOURCE_FILES_PROPERTIES(gui/AppConfig.cpp PROPERTIES COMPILE_FLAGS "-Wp,-ansi,-U__STRICT_ANSI__")
endif(PACKAGE_MODE)    

