if (openSUSE)
    # openSUSE don't install wx in a standard library system
    # path. Let's bypass the dynamic linker and hardcode the path.
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
endif()

# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


set(CommonFlags
    -fno-strict-aliasing
    -Wno-parentheses
    -Wstrict-aliasing # Allow to track strict aliasing issue.
    -Wno-char-subscripts # only impact svu which is deprecated
    -Wno-missing-braces
    #-Wno-ignored-attributes # don't remember why I put here but it is pure C option, therefore it complains for nothings on cpp files
    -DWX_PRECOMP
	)

if ("${PGO}" STREQUAL "generate")
    set(pcsx2FinalFlags -fprofile-generate)
elseif("${PGO}" STREQUAL "use")
    set(pcsx2FinalFlags -fprofile-use)
else()
    set(pcsx2FinalFlags "")
endif()

set(pcsx2FinalFlags ${pcsx2FinalFlags} ${CommonFlags})

if(XDG_STD)
    set(pcsx2FinalFlags ${pcsx2FinalFlags} -DXDG_STD)
endif()

set(Output PCSX2)

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
	GS.cpp
	GSState.cpp
	Hw.cpp
	HwRead.cpp
	HwWrite.cpp
	Interpreter.cpp
	IopBios.cpp
	IopCounters.cpp
	IopDma.cpp
	IopGte.cpp
	IopHw.cpp
	IopIrq.cpp
	IopMem.cpp
	IopSio2.cpp
	Mdec.cpp
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

# Main pcsx2 header
set(pcsx2Headers
	AsyncFileReader.h
	Cache.h
	cheatscpp.h
	Common.h
	Config.h
	COP0.h
	Counters.h
	Dmac.h
	Dump.h
	GameDatabase.h
	Elfheader.h
	Gif.h
	Gif_Unit.h
	GS.h
	Hardware.h
	Hw.h
	IopBios.h
	IopCommon.h
	IopCounters.h
	IopDma.h
	IopGte.h
	IopHw.h
	IopMem.h
	IopSio2.h
	Mdec.h
	MTVU.h
	Memory.h
	MemoryTypes.h
	NakedAsm.h
	Patch.h
	PathDefs.h
	Plugins.h
	PrecompiledHeader.h
	R3000A.h
	R5900Exceptions.h
	R5900.h
	R5900OpcodeTables.h
	SaveState.h
	Sifcmd.h
	Sif.h
	Sio.h
	sio_internal.h
	SPR.h
	SysForwardDefs.h
	System.h
	Vif_Dma.h
	Vif.h
	Vif_Unpack.h
	vtlb.h
	VUflags.h
	VUmicro.h
	VUops.h)

# CDVD sources
set(pcsx2CDVDSources
	CDVD/BlockdumpFileReader.cpp
	CDVD/CdRom.cpp
	CDVD/CDVDaccess.cpp
	CDVD/CDVD.cpp
	CDVD/CDVDisoReader.cpp
	CDVD/InputIsoFile.cpp
	CDVD/OutputIsoFile.cpp
	CDVD/ChunksCache.cpp
	CDVD/CompressedFileReader.cpp
	CDVD/CsoFileReader.cpp
	CDVD/GzippedFileReader.cpp
	CDVD/IsoFS/IsoFile.cpp
	CDVD/IsoFS/IsoFSCDVD.cpp
	CDVD/IsoFS/IsoFS.cpp
    )

# CDVD headers
set(pcsx2CDVDHeaders
	CDVD/CdRom.h
	CDVD/CDVDaccess.h
	CDVD/CDVD.h
	CDVD/CDVD_internal.h
	CDVD/CDVDisoReader.h
	CDVD/ChunksCache.h
	CDVD/CompressedFileReader.h
	CDVD/CompressedFileReaderUtils.h
	CDVD/CsoFileReader.h
	CDVD/GzippedFileReader.h
	CDVD/IsoFileFormats.h
	CDVD/IsoFS/IsoDirectory.h
	CDVD/IsoFS/IsoFileDescriptor.h
	CDVD/IsoFS/IsoFile.h
	CDVD/IsoFS/IsoFSCDVD.h
	CDVD/IsoFS/IsoFS.h
	CDVD/IsoFS/SectorSource.h
	CDVD/zlib_indexed.h
	)

# DebugTools sources
set(pcsx2DebugToolsSources
	DebugTools/DebugInterface.cpp
	DebugTools/DisassemblyManager.cpp
	DebugTools/ExpressionParser.cpp
	DebugTools/MIPSAnalyst.cpp
	DebugTools/MipsAssembler.cpp
	DebugTools/MipsAssemblerTables.cpp
	DebugTools/MipsStackWalk.cpp
	DebugTools/Breakpoints.cpp
	DebugTools/SymbolMap.cpp
	DebugTools/DisR3000A.cpp
	DebugTools/DisR5900asm.cpp
	DebugTools/DisVU0Micro.cpp
	DebugTools/DisVU1Micro.cpp
	DebugTools/BiosDebugData.cpp)

# DebugTools headers
set(pcsx2DebugToolsHeaders
	DebugTools/DebugInterface.h
	DebugTools/DisassemblyManager.h
	DebugTools/ExpressionParser.h
	DebugTools/MIPSAnalyst.h
	DebugTools/MipsAssembler.h
	DebugTools/MipsAssemblerTables.h
	DebugTools/MipsStackWalk.h
	DebugTools/Breakpoints.h
	DebugTools/SymbolMap.h
	DebugTools/Debug.h
	DebugTools/DisASM.h
	DebugTools/DisVUmicro.h
	DebugTools/DisVUops.h
	DebugTools/BiosDebugData.h)

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
	gui/Dialogs/AboutBoxDialog.cpp
	gui/Dialogs/AppConfigDialog.cpp
	gui/Dialogs/AssertionDialog.cpp
	gui/Dialogs/BaseConfigurationDialog.cpp
	gui/Dialogs/ConfirmationDialogs.cpp
	gui/Dialogs/ConvertMemoryCardDialog.cpp
	gui/Dialogs/CreateMemoryCardDialog.cpp
	gui/Dialogs/FirstTimeWizard.cpp
	gui/Dialogs/GameDatabaseDialog.cpp
	gui/Dialogs/ImportSettingsDialog.cpp
	gui/Dialogs/LogOptionsDialog.cpp
	gui/Dialogs/McdConfigDialog.cpp
	gui/Dialogs/PickUserModeDialog.cpp
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
	gui/MemoryCardFolder.cpp
	gui/MessageBoxes.cpp
	gui/MSWstuff.cpp
	gui/Panels/BaseApplicableConfigPanel.cpp
	gui/Panels/BiosSelectorPanel.cpp
	gui/Panels/CpuPanel.cpp
	gui/Panels/DirPickerPanel.cpp
	gui/Panels/GameDatabasePanel.cpp
	gui/Panels/GameFixesPanel.cpp
	gui/Panels/GSWindowPanel.cpp
	gui/Panels/LogOptionsPanels.cpp
	gui/Panels/MemoryCardListPanel.cpp
	gui/Panels/MemoryCardListView.cpp
	gui/Panels/MiscPanelStuff.cpp
	gui/Panels/PathsPanel.cpp
	gui/Panels/PluginSelectorPanel.cpp
	gui/Panels/SpeedhacksPanel.cpp
	gui/Panels/VideoPanel.cpp
	gui/RecentIsoList.cpp
	gui/Saveslots.cpp
	gui/SysState.cpp
	gui/UpdateUI.cpp
	)

# gui headers
set(pcsx2GuiHeaders
	gui/AppAccelerators.h
	gui/AppCommon.h
	gui/AppConfig.h
	gui/AppCorePlugins.h
	gui/AppCoreThread.h
	gui/AppEventListeners.h
	gui/AppForwardDefs.h
	gui/AppGameDatabase.h
	gui/App.h
	gui/ApplyState.h
	gui/AppSaveStates.h
	gui/ConsoleLogger.h
	gui/CpuUsageProvider.h
	gui/Debugger/BreakpointWindow.h
	gui/Debugger/CtrlDisassemblyView.h
	gui/Debugger/CtrlMemView.h
	gui/Debugger/CtrlRegisterList.h
	gui/Debugger/DebugEvents.h
	gui/Debugger/DebuggerLists.h
	gui/Debugger/DisassemblyDialog.h
	gui/Dialogs/BaseConfigurationDialog.inl
	gui/Dialogs/ConfigurationDialog.h
	gui/Dialogs/LogOptionsDialog.h
	gui/Dialogs/ModalPopups.h
	gui/GSFrame.h
	gui/i18n.h
	gui/IsoDropTarget.h
	gui/MainFrame.h
	gui/MemoryCardFile.h
	gui/MemoryCardFolder.h
	gui/MSWstuff.h
	gui/Panels/ConfigurationPanels.h
	gui/Panels/LogOptionsPanels.h
	gui/Panels/MemoryCardPanels.h
	gui/pxEventThread.h
	gui/RecentIsoList.h
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
    ${res_bin}/Logo.h
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

# IPU headers
set(pcsx2IPUHeaders
	IPU/IPUdma.h
	IPU/IPU_Fifo.h
	IPU/IPU.h
	IPU/mpeg2lib/Mpeg.h
	IPU/mpeg2lib/Vlc.h
	IPU/yuv2rgb.h
	)

# Linux sources
set(pcsx2LinuxSources
	gui/CpuUsageProviderLnx.cpp
	Linux/LnxConsolePipe.cpp
	Linux/LnxKeyCodes.cpp
	Linux/LnxFlatFileReader.cpp
    )

set(pcsx2OSXSources
	gui/CpuUsageProviderLnx.cpp
#	Linux/LnxConsolePipe.cpp
#	Linux/LnxKeyCodes.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

set(pcsx2FreeBSDSources
	gui/CpuUsageProviderLnx.cpp
	Linux/LnxConsolePipe.cpp
	Linux/LnxKeyCodes.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

# Linux headers
set(pcsx2LinuxHeaders
	)

# ps2 sources
set(pcsx2ps2Sources
	ps2/BiosTools.cpp
	ps2/pgif.cpp
	ps2/LegacyDmac.cpp
	ps2/Iop/IopHwRead.cpp
	ps2/Iop/IopHwWrite.cpp
	ps2/Iop/PsxBios.cpp)

# ps2 headers
set(pcsx2ps2Headers
	ps2/BiosTools.h
	ps2/eeHwTraceLog.inl
	ps2/pgif.h
	ps2/HwInternal.h
	ps2/Iop/IopHw_Internal.h)

# RDebug sources
set(pcsx2RDebugSources
	RDebug/deci2.cpp
	RDebug/deci2_dbgp.cpp
	RDebug/deci2_dcmp.cpp
	RDebug/deci2_drfp.cpp
	RDebug/deci2_iloadp.cpp
	RDebug/deci2_netmp.cpp
	RDebug/deci2_ttyp.cpp)

# RDebug headers
set(pcsx2RDebugHeaders
	RDebug/deci2_dbgp.h
	RDebug/deci2_dcmp.h
	RDebug/deci2_drfp.h
	RDebug/deci2.h
	RDebug/deci2_iloadp.h
	RDebug/deci2_netmp.h
	RDebug/deci2_ttyp.h)

# System sources
set(pcsx2SystemSources
	System/SysCoreThread.cpp
	System/SysThreadBase.cpp)

# System headers
set(pcsx2SystemHeaders
	System/RecTypes.h
	System/SysThreads.h)

# Utilities sources
set(pcsx2UtilitiesSources
	Utilities/FileUtils.cpp)

# Utilities headers
set(pcsx2UtilitiesHeaders
	Utilities/AsciiFile.h)

# Zip tools utilies sources
set(pcsx2ZipToolsSources
    ZipTools/thread_gzip.cpp
    ZipTools/thread_lzma.cpp)

# Zip tools utilies headers
set(pcsx2ZipToolsHeaders
    ZipTools/ThreadedZipTools.h)


# Windows sources
set(pcsx2WindowsSources
	gui/CpuUsageProviderMSW.cpp
	windows/cheats/browser.cpp
	windows/cheats/cheats.cpp
	windows/DwmSetup.cpp
	windows/FlatFileReaderWindows.cpp
	windows/Optimus.cpp
	windows/PatchBrowser.cpp
	windows/VCprojects/IopSif.cpp
	windows/WinCompressNTFS.cpp
	windows/WinConsolePipe.cpp
	windows/WinKeyCodes.cpp
	)

# Windows headers
set(pcsx2WindowsHeaders
	windows/cheats/cheats.h
	windows/resource.h
	windows/Win32.h)

# x86 sources
set(pcsx2x86Sources
	x86/BaseblockEx.cpp
	x86/iCOP0.cpp
	x86/iCore.cpp
	x86/iFPU.cpp
	x86/iFPUd.cpp
	x86/iMisc.cpp
	x86/iMMI.cpp
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
	)
if (NOT DISABLE_SVU)
    set(pcsx2x86Sources ${pcsx2x86Sources}
	x86/sVU_Lower.cpp
	x86/sVU_Micro.cpp
	x86/sVU_Upper.cpp
	x86/sVU_zerorec.cpp
    )
endif()

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
	x86/microVU_Alloc.inl
	x86/microVU_Analyze.inl
	x86/microVU_Branch.inl
	x86/microVU_Clamp.inl
	x86/microVU_Compile.inl
	x86/microVU.cpp
	x86/microVU_Execute.inl
	x86/microVU_Flags.inl
	x86/microVU.h
	x86/microVU_IR.h
	x86/microVU_Log.inl
	x86/microVU_Lower.inl
	x86/microVU_Macro.inl
	x86/microVU_Misc.h
	x86/microVU_Misc.inl
	x86/microVU_Profiler.h
	x86/microVU_Tables.inl
	x86/microVU_Upper.inl
	x86/newVif.h
	x86/newVif_HashBucket.h
	x86/newVif_UnpackSSE.h
	x86/R5900_Profiler.h
	x86/sVU_Compare.h
	x86/sVU_Debug.h
	x86/sVU_Micro.h
	x86/sVU_zerorec.h
	)

# collect .S files
if (NOT DISABLE_SVU)
    set(pcsx2SSources x86/aVUzerorec.S)
    # change language of .S-files to c++
    set_source_files_properties(${pcsx2SSources} PROPERTIES LANGUAGE CXX)
else()
    set(pcsx2SSources "")
endif()


# common Sources
set(Common
	${pcsx2Sources}
	${pcsx2Headers}
	${pcsx2CDVDSources}
	${pcsx2CDVDHeaders}
	${pcsx2DebugToolsSources}
	${pcsx2GuiSources}
	${pcsx2GuiResources}
	${pcsx2GuiHeaders}
	${pcsx2IPUSources}
	${pcsx2IPUHeaders}
	${pcsx2ps2Sources}
	${pcsx2ps2Headers}
	${pcsx2SystemSources}
	${pcsx2SystemHeaders}
	${pcsx2UtilitiesSources}
	${pcsx2UtilitiesHeaders}
	${pcsx2x86Sources}
	${pcsx2x86Headers}
	${pcsx2ZipToolsSources}
	${pcsx2ZipToolsHeaders}
	${pcsx2SSources})

# platform sources
# Linux
if(Linux)
	set(Platform
		${pcsx2LinuxSources}
		${pcsx2LinuxHeaders})
endif()

# Windows
if(Windows)
	set(Platform
		${pcsx2WindowsSources}
		${pcsx2WindowsHeaders})
endif()

# MacOSX
if(APPLE)
	set(Platform
		${pcsx2OSXSources}
		${pcsx2LinuxHeaders})
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(Platform
		${pcsx2FreeBSDSources}
		${pcsx2LinuxHeaders})
endif()

set(pcsx2FinalSources
	${Common}
	${Platform}
)

set(pcsx2FinalLibs
    Utilities
    x86emitter
    ${wxWidgets_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${AIO_LIBRARIES}
    ${GCOV_LIBRARIES}
)

if(BUILTIN_GS)
    set(pcsx2FinalLibs "${pcsx2FinalLibs} GSdx")
endif()
if(BUILTIN_PAD)
    set(pcsx2FinalLibs "${pcsx2FinalLibs} onepad-1.2.0")
endif()
if(BUILTIN_SPU2)
    set(pcsx2FinalLibs "${pcsx2FinalLibs} spu2x-2.0.0")
endif()
if(BUILTIN_USB)
    set(pcsx2FinalLibs "${pcsx2FinalLibs} USBnull-0.7.0")
endif()
if(BUILTIN_FW)
    set(pcsx2FinalLibs "${pcsx2FinalLibs} FWnull-0.7.0")
endif()
if(BUILTIN_DEV)
    set(pcsx2FinalLibs "${pcsx2FinalLibs} dev9null-0.5.0")
endif()
if(BUILTIN_CDVD)
    set(pcsx2FinalLibs "${pcsx2FinalLibs} CDVDnull")
endif()

# additonal include directories
include_directories(
    gui
    x86
    ${CMAKE_BINARY_DIR}/pcsx2/gui
)

### Generate the resources files
file(MAKE_DIRECTORY ${res_bin})

foreach(res_file IN ITEMS
        AppIcon16 AppIcon32 AppIcon64 BackgroundLogo Logo ButtonIcon_Camera
        ConfigIcon_Appearance ConfigIcon_Cpu ConfigIcon_Gamefixes ConfigIcon_MemoryCard
        ConfigIcon_Paths ConfigIcon_Plugins ConfigIcon_Speedhacks ConfigIcon_Video Breakpoint_Active Breakpoint_Inactive)
    add_custom_command(OUTPUT  "${res_bin}/${res_file}.h" COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl "${res_src}/${res_file}.png" "${res_bin}/${res_file}" )
endforeach()

if(USE_VTUNE)
    set(pcsx2FinalLibs ${pcsx2FinalLibs} ${VTUNE_LIBRARIES})
endif()

add_pcsx2_executable(${Output} "${pcsx2FinalSources}" "${pcsx2FinalLibs}" "${pcsx2FinalFlags}")
