include(macros/GlibCompileResourcesSupport)

# All flags, libraries, etc, that are shared between PCSX2 compilation files
add_library(PCSX2_FLAGS INTERFACE)

if (NOT PCSX2_CORE)
	add_executable(PCSX2)
else()
	add_library(PCSX2)
	target_compile_definitions(PCSX2_FLAGS INTERFACE
		"PCSX2_CORE"
	)
endif()
target_link_libraries(PCSX2 PRIVATE PCSX2_FLAGS)

target_compile_features(PCSX2_FLAGS INTERFACE cxx_std_17)
target_compile_definitions(PCSX2_FLAGS INTERFACE "${PCSX2_DEFS}")
target_compile_options(PCSX2_FLAGS INTERFACE "${PCSX2_WARNINGS}")

if (NOT PCSX2_CORE)
	if (PACKAGE_MODE)
		install(TARGETS PCSX2 DESTINATION ${CMAKE_INSTALL_BINDIR})
	else()
		install(TARGETS PCSX2 DESTINATION ${CMAKE_SOURCE_DIR}/bin)
	endif()
endif()

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

if(MSVC)
	if (MSVC_VERSION GREATER_EQUAL 1930)
		target_compile_options(PCSX2_FLAGS INTERFACE /fp:contract)
	endif()
else()
	target_compile_options(PCSX2_FLAGS INTERFACE
		-ffp-contract=fast
		-fno-strict-aliasing
		-Wstrict-aliasing # Allow to track strict aliasing issue.
		-Wno-parentheses
		-Wno-missing-braces
		-Wno-unknown-pragmas
		-DWX_PRECOMP
	)
endif()

if(GCC_VERSION VERSION_EQUAL "8.0" OR GCC_VERSION VERSION_GREATER "8.0")
	# gs is pretty bad at this
	target_compile_options(PCSX2_FLAGS INTERFACE -Wno-packed-not-aligned -Wno-class-memaccess)
endif()

if ("${PGO}" STREQUAL "generate")
	target_compile_options(PCSX2_FLAGS INTERFACE -fprofile-generate)
elseif("${PGO}" STREQUAL "use")
	target_compile_options(PCSX2_FLAGS INTERFACE -fprofile-use)
endif()

if(TARGET PulseAudio::PulseAudio)
	target_compile_definitions(PCSX2_FLAGS INTERFACE SPU2X_PULSEAUDIO)
	target_link_libraries(PCSX2_FLAGS INTERFACE PulseAudio::PulseAudio)
endif()

if(XDG_STD)
	target_compile_definitions(PCSX2_FLAGS INTERFACE XDG_STD)
endif()

if(TARGET SDL2::SDL2 OR TARGET SDL2::SDL2-static)
	target_compile_definitions(PCSX2_FLAGS INTERFACE SDL_BUILD)
	if ("${SDL2_TYPE}" STREQUAL Bundled)
		# Use our in-tree SDL build.
		target_link_libraries(PCSX2_FLAGS INTERFACE SDL2::SDL2-static)
	else()
		target_link_libraries(PCSX2_FLAGS INTERFACE SDL2::SDL2)
	endif()
	if(PCSX2_CORE)
		target_sources(PCSX2 PRIVATE
			Frontend/SDLInputSource.cpp
			Frontend/SDLInputSource.h
		)
	endif()
endif()

if(WIN32)
	# Resources
	target_sources(PCSX2 PRIVATE
		PCSX2.rc
		GS/GS.rc
		PAD/Windows/PAD.rc
		SPU2/Windows/SPU2.rc
		USB/usb-pad/dx/versionproxy.rc
		USB/usb-pad/raw/raw-config.rc
		windows/wxResources.rc
		windows/PCSX2.manifest
	)
	target_compile_definitions(PCSX2_FLAGS INTERFACE
		_M_SSE=0x401 # TODO: Multiple ISA
		DIRECTINPUT_VERSION=0x0800
		WINVER=0x0603
		_WIN32_WINNT=0x0603
		WIN32_LEAN_AND_MEAN
		LZMA_API_STATIC
		WIL_SUPPRESS_EXCEPTIONS
	)
	target_include_directories(PCSX2_FLAGS INTERFACE
		../3rdparty # GL headers
	)
	set_target_properties(PCSX2 PROPERTIES WIN32_EXECUTABLE TRUE)
endif(WIN32)

# Main pcsx2 source
set(pcsx2Sources
	Cache.cpp
	COP0.cpp
	COP2.cpp
	Counters.cpp
	Dmac.cpp
	GameDatabase.cpp
	Dump.cpp
	Elfheader.cpp
	FW.cpp
	FiFo.cpp
	FPU.cpp
	Gif.cpp
	Gif_Logger.cpp
	Gif_Unit.cpp
	GS.cpp
	Host.cpp
	HostDisplay.cpp
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
	PINE.cpp
	Mdec.cpp
	Memory.cpp
	MemoryCardFile.cpp
	MemoryCardFolder.cpp
	MMI.cpp
	MTGS.cpp
	MTVU.cpp
	MultipartFileReader.cpp
	Patch.cpp
	Patch_Memory.cpp
	Pcsx2Config.cpp
	PerformanceMetrics.cpp
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
	Common.h
	Config.h
	COP0.h
	Counters.h
	Dmac.h
	Dump.h
	GameDatabase.h
	Elfheader.h
	FW.h
	Gif.h
	Gif_Unit.h
	GS.h
	Hardware.h
	Host.h
	HostDisplay.h
	Hw.h
	IopBios.h
	IopCounters.h
	IopDma.h
	IopGte.h
	IopHw.h
	IopMem.h
	IopSio2.h
	PINE.h
	Mdec.h
	MTVU.h
	Memory.h
	MemoryCardFile.h
	MemoryCardFolder.h
	MemoryTypes.h
	Patch.h
	PCSX2Base.h
	PerformanceMetrics.h
	PrecompiledHeader.h
	R3000A.h
	R5900Exceptions.h
	R5900.h
	R5900OpcodeTables.h
	SaveState.h
	Sifcmd.h
	Sif.h
	SingleRegisterTypes.h
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
	CDVD/Ps1CD.cpp
	CDVD/CDVDaccess.cpp
	CDVD/CDVD.cpp
	CDVD/CDVDdiscReader.cpp
	CDVD/CDVDisoReader.cpp
	CDVD/CDVDdiscThread.cpp
	CDVD/InputIsoFile.cpp
	CDVD/OutputIsoFile.cpp
	CDVD/ChunksCache.cpp
	CDVD/CompressedFileReader.cpp
	CDVD/ChdFileReader.cpp
	CDVD/CsoFileReader.cpp
	CDVD/GzippedFileReader.cpp
	CDVD/ThreadedFileReader.cpp
	CDVD/IsoFS/IsoFile.cpp
	CDVD/IsoFS/IsoFSCDVD.cpp
	CDVD/IsoFS/IsoFS.cpp
	)

# CDVD headers
set(pcsx2CDVDHeaders
	CDVD/Ps1CD.h
	CDVD/CDVDaccess.h
	CDVD/CDVD.h
	CDVD/CDVD_internal.h
	CDVD/CDVDdiscReader.h
	CDVD/ChunksCache.h
	CDVD/CompressedFileReader.h
	CDVD/ChdFileReader.h
	CDVD/CsoFileReader.h
	CDVD/GzippedFileReader.h
	CDVD/ThreadedFileReader.h
	CDVD/IsoFileFormats.h
	CDVD/IsoFS/IsoDirectory.h
	CDVD/IsoFS/IsoFileDescriptor.h
	CDVD/IsoFS/IsoFile.h
	CDVD/IsoFS/IsoFSCDVD.h
	CDVD/IsoFS/IsoFS.h
	CDVD/IsoFS/SectorSource.h
	CDVD/zlib_indexed.h
	)

# SPU2 sources
set(pcsx2SPU2Sources
	SPU2/ADSR.cpp
	SPU2/Debug.cpp
	SPU2/DplIIdecoder.cpp
	SPU2/Dma.cpp
	SPU2/Mixer.cpp
	SPU2/spu2.cpp
	SPU2/ReadInput.cpp
	SPU2/RegLog.cpp
	SPU2/RegTable.cpp
	SPU2/Reverb.cpp
	SPU2/SndOut.cpp
	SPU2/spu2freeze.cpp
	SPU2/spu2sys.cpp
	SPU2/Timestretcher.cpp
	SPU2/Wavedump_wav.cpp
	)

if(CUBEB_API)
	list(APPEND pcsx2SPU2Sources SPU2/SndOut_Cubeb.cpp)
	target_compile_definitions(PCSX2_FLAGS INTERFACE "SPU2X_CUBEB")
	target_link_libraries(PCSX2_FLAGS INTERFACE cubeb)
endif()

# SPU2 headers
set(pcsx2SPU2Headers
	SPU2/Config.h
	SPU2/Debug.h
	SPU2/defs.h
	SPU2/Dma.h
	SPU2/Global.h
	SPU2/interpolate_table.h
	SPU2/Mixer.h
	SPU2/spu2.h
	SPU2/regs.h
	SPU2/SndOut.h
	SPU2/spdif.h
	SPU2/WavFile.h
)

# DEV9 sources
set(pcsx2DEV9Sources
	DEV9/ATA/Commands/ATA_Command.cpp
	DEV9/ATA/Commands/ATA_CmdDMA.cpp
	DEV9/ATA/Commands/ATA_CmdExecuteDeviceDiag.cpp
	DEV9/ATA/Commands/ATA_CmdNoData.cpp
	DEV9/ATA/Commands/ATA_CmdPIOData.cpp
	DEV9/ATA/Commands/ATA_CmdSMART.cpp
	DEV9/ATA/Commands/ATA_SCE.cpp
	DEV9/ATA/ATA_Info.cpp
	DEV9/ATA/ATA_State.cpp
	DEV9/ATA/ATA_Transfer.cpp
	DEV9/ATA/HddCreate.cpp
	DEV9/InternalServers/DHCP_Server.cpp
	DEV9/InternalServers/DNS_Logger.cpp
	DEV9/InternalServers/DNS_Server.cpp
	DEV9/PacketReader/ARP/ARP_Packet.cpp
	DEV9/PacketReader/IP/ICMP/ICMP_Packet.cpp
	DEV9/PacketReader/IP/TCP/TCP_Options.cpp
	DEV9/PacketReader/IP/TCP/TCP_Packet.cpp
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Options.cpp
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.cpp
	DEV9/PacketReader/IP/UDP/DNS/DNS_Classes.cpp
	DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.cpp
	DEV9/PacketReader/IP/UDP/UDP_Packet.cpp
	DEV9/PacketReader/IP/IP_Options.cpp
	DEV9/PacketReader/IP/IP_Packet.cpp
	DEV9/PacketReader/EthernetFrame.cpp
	DEV9/PacketReader/NetLib.cpp
	DEV9/Sessions/BaseSession.cpp
	DEV9/Sessions/ICMP_Session/ICMP_Session.cpp
	DEV9/Sessions/TCP_Session/TCP_Session.cpp
	DEV9/Sessions/TCP_Session/TCP_Session_In.cpp
	DEV9/Sessions/TCP_Session/TCP_Session_Out.cpp
	DEV9/Sessions/UDP_Session/UDP_FixedPort.cpp
	DEV9/Sessions/UDP_Session/UDP_Session.cpp
	DEV9/smap.cpp
	DEV9/sockets.cpp
	DEV9/DEV9.cpp
	DEV9/flash.cpp
	DEV9/pcap_io.cpp
	DEV9/net.cpp
	)

# DEV9 headers
set(pcsx2DEV9Headers
	DEV9/ATA/ATA.h
	DEV9/ATA/HddCreate.h
	DEV9/DEV9.h
	DEV9/InternalServers/DHCP_Server.cpp
	DEV9/InternalServers/DNS_Logger.h
	DEV9/InternalServers/DNS_Server.h
	DEV9/net.h
	DEV9/PacketReader/ARP/ARP_Packet.h
	DEV9/PacketReader/IP/ICMP/ICMP_Packet.h
	DEV9/PacketReader/IP/TCP/TCP_Options.h
	DEV9/PacketReader/IP/TCP/TCP_Packet.h
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Options.h
	DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.h
	DEV9/PacketReader/IP/UDP/DNS/DNS_Classes.h
	DEV9/PacketReader/IP/UDP/DNS/DNS_Enums.h
	DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.h
	DEV9/PacketReader/IP/UDP/UDP_Packet.h
	DEV9/PacketReader/IP/IP_Address.h
	DEV9/PacketReader/IP/IP_Options.h
	DEV9/PacketReader/IP/IP_Packet.h
	DEV9/PacketReader/IP/IP_Payload.h
	DEV9/PacketReader/EthernetFrame.h
	DEV9/PacketReader/NetLib.h
	DEV9/PacketReader/Payload.h
	DEV9/pcap_io.h
	DEV9/Sessions/BaseSession.h
	DEV9/Sessions/ICMP_Session/ICMP_Session.h
	DEV9/Sessions/TCP_Session/TCP_Session.h
	DEV9/Sessions/UDP_Session/UDP_FixedPort.h
	DEV9/Sessions/UDP_Session/UDP_BaseSession.h
	DEV9/Sessions/UDP_Session/UDP_Session.h
	DEV9/SimpleQueue.h
	DEV9/smap.h
	DEV9/sockets.h
	DEV9/ThreadSafeMap.h
	)

if(NOT PCSX2_CORE)
	list(APPEND pcsx2DEV9Sources
		DEV9/ConfigUI.cpp
		DEV9/DEV9Config.cpp
		DEV9/ATA/HddCreateWx.cpp
	)
	list(APPEND pcsx2DEV9Sources
		DEV9/DEV9Config.h
		DEV9/ATA/HddCreateWx.h
	)
endif()

# USBNull
set(pcsx2USBNullSources USB/USBNull.cpp)
set(pcsx2USBNullHeaders USB/USB.h)

# USB sources 
set(pcsx2USBSources
	USB/USB.cpp
	USB/deviceproxy.cpp
	USB/configuration.cpp
	USB/device_init.cpp
	USB/qemu-usb/glib.cpp
	USB/qemu-usb/vl.cpp
	USB/qemu-usb/iov.cpp
	USB/qemu-usb/desc.cpp
	USB/qemu-usb/core.cpp
	USB/qemu-usb/bus.cpp
	USB/qemu-usb/usb-ohci.cpp
	USB/qemu-usb/hid.cpp
	USB/qemu-usb/input-keymap-qcode-to-qnum.cpp
	USB/usb-msd/usb-msd.cpp
	USB/usb-pad/usb-pad.cpp
	USB/usb-pad/usb-pad-ff.cpp
	USB/usb-pad/lg/lg_ff.cpp
	USB/usb-pad/usb-seamic.cpp
	USB/usb-mic/usb-mic-singstar.cpp
	USB/usb-mic/usb-mic-logitech.cpp
	USB/usb-mic/usb-headset.cpp
	../3rdparty/jpgd/jpgd.cpp
	../3rdparty/jpgd/jpge.cpp
	USB/usb-eyetoy/jo_mpeg.cpp
	USB/usb-eyetoy/usb-eyetoy-webcam.cpp
	USB/usb-hid/usb-hid.cpp
	USB/shared/shared_usb.cpp
	USB/shared/inifile_usb.cpp
	USB/shared/ringbuffer.cpp
	USB/usb-printer/usb-printer.cpp
	)

# USB headers
set(pcsx2USBHeaders
	USB/USB.h
	USB/proxybase.h
	USB/deviceproxy.h
	USB/configuration.h
	USB/platcompat.h
	USB/helpers.h
	USB/readerwriterqueue/readerwriterqueue.h
	USB/readerwriterqueue/atomicops.h
	USB/qemu-usb/glib.h
	USB/qemu-usb/vl.h
	USB/qemu-usb/qusb.h
	USB/qemu-usb/USBinternal.h
	USB/qemu-usb/desc.h
	USB/qemu-usb/iov.h
	USB/qemu-usb/queue.h
	USB/qemu-usb/hid.h
	USB/qemu-usb/input-keymap.h
	USB/usb-msd/usb-msd.h
	USB/usb-pad/usb-pad.h
	USB/usb-pad/padproxy.h
	USB/usb-pad/lg/lg_ff.h
	USB/usb-mic/audio.h
	USB/usb-mic/audiodev.h
	USB/usb-mic/audiodeviceproxy.h
	USB/usb-mic/usb-mic-singstar.h
	USB/usb-mic/usb-headset.h
	USB/usb-mic/audiodev-noop.h
	../3rdparty/jpgd/jpgd.h
	../3rdparty/jpgd/jpge.h
	USB/usb-eyetoy/jo_mpeg.h
	USB/usb-eyetoy/videodeviceproxy.h
	USB/usb-eyetoy/videodev.h
	USB/usb-eyetoy/usb-eyetoy-webcam.h
	USB/usb-eyetoy/ov519.h
	USB/usb-hid/hidproxy.h
	USB/usb-hid/usb-hid.h
	USB/usb-hid/noop.h
	USB/shared/shared_usb.h
	USB/shared/inifile_usb.h
	USB/shared/ringbuffer.h
	USB/usb-printer/usb-printer.h
	)

if(TARGET PulseAudio::PulseAudio)
	list(APPEND pcsx2USBSources USB/usb-mic/audiodev-pulse.cpp)
	list(APPEND pcsx2USBHeaders USB/usb-mic/audiodev-pulse.h)
endif()

if(PCSX2_CORE)
	# Host PAD
	set(pcsx2PADSources
		PAD/Host/KeyStatus.cpp
		PAD/Host/PAD.cpp
		PAD/Host/StateManagement.cpp
	)
	set(pcsx2PADHeaders
		PAD/Host/Global.h
		PAD/Host/KeyStatus.h
		PAD/Host/PAD.h
		PAD/Host/StateManagement.h
	)
elseif(WIN32)
	set(pcsx2PADSources
		PAD/Windows/PADConfig.cpp
		PAD/Windows/DeviceEnumerator.cpp
		PAD/Windows/Diagnostics.cpp
		PAD/Windows/DirectInput.cpp
		PAD/Windows/DualShock3.cpp
		PAD/Windows/HidDevice.cpp
		PAD/Windows/InputManager.cpp
		PAD/Windows/KeyboardQueue.cpp
		PAD/Windows/PAD.cpp
		PAD/Windows/PADRawInput.cpp
		PAD/Windows/Tooltips.cpp
		PAD/Windows/VKey.cpp
		PAD/Windows/WindowsKeyboard.cpp
		PAD/Windows/WindowsMessaging.cpp
		PAD/Windows/WindowsMouse.cpp
		PAD/Windows/WndProcEater.cpp
		PAD/Windows/XInputEnum.cpp
	)

	set(pcsx2PADHeaders
		PAD/Gamepad.h
		PAD/Windows/PADConfig.h
		PAD/Windows/DeviceEnumerator.h
		PAD/Windows/Diagnostics.h
		PAD/Windows/DirectInput.h
		PAD/Windows/DualShock3.h
		PAD/Windows/Global.h
		PAD/Windows/HidDevice.h
		PAD/Windows/InputManager.h
		PAD/Windows/KeyboardQueue.h
		PAD/Windows/PADRawInput.h
		PAD/Windows/resource_pad.h
		PAD/Windows/Tooltips.h
		PAD/Windows/usb.h
		PAD/Windows/VKey.h
		PAD/Windows/WindowsKeyboard.h
		PAD/Windows/WindowsMessaging.h
		PAD/Windows/WindowsMouse.h
		PAD/Windows/WndProcEater.h
		PAD/Windows/XInputEnum.h
	)
else()
	# PAD resources pre-compilation
	set(PADImgHeader "${CMAKE_BINARY_DIR}/pcsx2/PAD/Linux/ImgHeader")
	set(PADImg "${CMAKE_SOURCE_DIR}/pcsx2/PAD/Linux/Img")
	target_include_directories(PCSX2_FLAGS INTERFACE "${CMAKE_BINARY_DIR}/pcsx2/PAD/Linux/")

	file(MAKE_DIRECTORY ${PADImgHeader})

	foreach(result_file IN ITEMS
			analog circle cross dp_bottom dp_left dp_right dp_up dualshock2
			joystick_cursor l1 r1 l2 r2 l3 r3 select square start triangle arrow_up
			arrow_bottom arrow_left arrow_right)
		add_custom_command(
			OUTPUT  "${PADImgHeader}/${result_file}.h"
			DEPENDS ${BIN2CPPDEP} "${PADImg}/${result_file}.png"
			COMMAND ${BIN2CPP} "${PADImg}/${result_file}.png" "${PADImgHeader}/${result_file}" )
	endforeach()

	# PAD/Linux sources
	set(pcsx2PADSources
		PAD/Linux/Device.cpp
		PAD/Linux/InputManager.cpp
		PAD/Linux/SDL/joystick.cpp
		PAD/Linux/keyboard.cpp
		PAD/Linux/KeyStatus.cpp
		PAD/Linux/PAD.cpp
		PAD/Linux/state_management.cpp
		PAD/Linux/wx_dialog/dialog.cpp
		PAD/Linux/wx_dialog/opPanel.cpp
		PAD/Linux/wx_dialog/GamepadConfiguration.cpp
		PAD/Linux/wx_dialog/JoystickConfiguration.cpp
		PAD/Linux/Config.cpp
	)

	# PAD/Linux headers
	set(pcsx2PADHeaders
		PAD/Gamepad.h
		PAD/Linux/bitwise.h
		PAD/Linux/Config.h
		PAD/Linux/Device.h
		PAD/Linux/Global.h
		PAD/Linux/InputManager.h
		PAD/Linux/SDL/joystick.h
		PAD/Linux/keyboard.h
		PAD/Linux/KeyStatus.h
		PAD/Linux/state_management.h
		PAD/Linux/wx_dialog/opPanel.h
		PAD/Linux/wx_dialog/GamepadConfiguration.h
		PAD/Linux/wx_dialog/JoystickConfiguration.h
		# images
		${PADImgHeader}/analog.h
		${PADImgHeader}/circle.h
		${PADImgHeader}/cross.h
		${PADImgHeader}/dp_bottom.h
		${PADImgHeader}/dp_left.h
		${PADImgHeader}/dp_right.h
		${PADImgHeader}/dp_up.h
		${PADImgHeader}/dualshock2.h
		${PADImgHeader}/joystick_cursor.h
		${PADImgHeader}/l1.h
		${PADImgHeader}/r1.h
		${PADImgHeader}/l2.h
		${PADImgHeader}/r2.h
		${PADImgHeader}/l3.h
		${PADImgHeader}/r3.h
		${PADImgHeader}/select.h
		${PADImgHeader}/square.h
		${PADImgHeader}/start.h
		${PADImgHeader}/triangle.h
		${PADImgHeader}/arrow_up.h
		${PADImgHeader}/arrow_bottom.h
		${PADImgHeader}/arrow_left.h
		${PADImgHeader}/arrow_right.h
	)
endif()

# GS sources
set(pcsx2GSSources
	GS/GS.cpp
	GS/GSAlignedClass.cpp
	GS/GSBlock.cpp
	GS/GSCapture.cpp
	GS/GSClut.cpp
	GS/GSCodeBuffer.cpp
	GS/GSCrc.cpp
	GS/GSDrawingContext.cpp
	GS/GSDump.cpp
	GS/GSLocalMemory.cpp
	GS/GSLzma.cpp
	GS/GSPerfMon.cpp
	GS/GSPng.cpp
	GS/GSRingHeap.cpp
	GS/GSState.cpp
	GS/GSTables.cpp
	GS/GSUtil.cpp
	GS/GSVector.cpp
	GS/Renderers/Common/GSDevice.cpp
	GS/Renderers/Common/GSDirtyRect.cpp
	GS/Renderers/Common/GSFunctionMap.cpp
	GS/Renderers/Common/GSRenderer.cpp
	GS/Renderers/Common/GSTexture.cpp
	GS/Renderers/Common/GSVertexTrace.cpp
	GS/Renderers/Null/GSDeviceNull.cpp
	GS/Renderers/Null/GSRendererNull.cpp
	GS/Renderers/Null/GSTextureNull.cpp
	GS/Renderers/HW/GSHwHack.cpp
	GS/Renderers/HW/GSRendererHW.cpp
	GS/Renderers/HW/GSTextureCache.cpp
	GS/Renderers/HW/GSTextureReplacementLoaders.cpp
	GS/Renderers/HW/GSTextureReplacements.cpp
	GS/Renderers/SW/GSDrawScanline.cpp
	GS/Renderers/SW/GSDrawScanlineCodeGenerator.cpp
	GS/Renderers/SW/GSDrawScanlineCodeGenerator.all.cpp
	GS/Renderers/SW/GSNewCodeGenerator.cpp
	GS/Renderers/SW/GSRasterizer.cpp
	GS/Renderers/SW/GSRendererSW.cpp
	GS/Renderers/SW/GSSetupPrimCodeGenerator.cpp
	GS/Renderers/SW/GSSetupPrimCodeGenerator.all.cpp
	GS/Renderers/SW/GSTextureCacheSW.cpp
	GS/Renderers/SW/GSTextureSW.cpp
	GS/Window/GSSetting.cpp
	)

# GS headers
set(pcsx2GSHeaders
	GS/config.h
	GS/GSAlignedClass.h
	GS/GSBlock.h
	GS/GSCapture.h
	GS/GSClut.h
	GS/GSCodeBuffer.h
	GS/GSCrc.h
	GS/GSDrawingContext.h
	GS/GSDrawingEnvironment.h
	GS/GSDump.h
	GS/GSExtra.h
	GS/GSGL.h
	GS/GSIntrin.h
	GS/GSRegs.h
	GS/GS.h
	GS/GSLocalMemory.h
	GS/GSLzma.h
	GS/GSPerfMon.h
	GS/GSPng.h
	GS/GSRingHeap.h
	GS/GSState.h
	GS/GSTables.h
	GS/GSThread_CXX11.h
	GS/GSThread.h
	GS/GSUtil.h
	GS/GSVector.h
	GS/GSVector4.h
	GS/GSVector4i.h
	GS/GSVector8.h
	GS/GSVector8i.h
	GS/Renderers/Common/GSDevice.h
	GS/Renderers/Common/GSDirtyRect.h
	GS/Renderers/Common/GSFastList.h
	GS/Renderers/Common/GSFunctionMap.h
	GS/Renderers/Common/GSRenderer.h
	GS/Renderers/Common/GSTexture.h
	GS/Renderers/Common/GSVertex.h
	GS/Renderers/Common/GSVertexList.h
	GS/Renderers/Common/GSVertexTrace.h
	GS/Renderers/Null/GSDeviceNull.h
	GS/Renderers/Null/GSRendererNull.h
	GS/Renderers/Null/GSTextureNull.h
	GS/Renderers/HW/GSRendererHW.h
	GS/Renderers/HW/GSTextureCache.h
	GS/Renderers/HW/GSTextureReplacements.h
	GS/Renderers/HW/GSVertexHW.h
	GS/Renderers/SW/GSDrawScanlineCodeGenerator.h
	GS/Renderers/SW/GSDrawScanlineCodeGenerator.all.h
	GS/Renderers/SW/GSDrawScanline.h
	GS/Renderers/SW/GSNewCodeGenerator.h
	GS/Renderers/SW/GSRasterizer.h
	GS/Renderers/SW/GSRendererSW.h
	GS/Renderers/SW/GSScanlineEnvironment.h
	GS/Renderers/SW/GSSetupPrimCodeGenerator.h
	GS/Renderers/SW/GSSetupPrimCodeGenerator.all.h
	GS/Renderers/SW/GSTextureCacheSW.h
	GS/Renderers/SW/GSTextureSW.h
	GS/Renderers/SW/GSVertexSW.h
	GS/Window/GSCaptureDlg.h
	GS/Window/GSDialog.h
	GS/Window/GSSetting.h
	)

if(USE_OPENGL)
	list(APPEND pcsx2GSSources
		GS/Renderers/OpenGL/GLLoader.cpp
		GS/Renderers/OpenGL/GLState.cpp
		GS/Renderers/OpenGL/GSDeviceOGL.cpp
		GS/Renderers/OpenGL/GSTextureOGL.cpp
	)
	list(APPEND pcsx2GSHeaders
		GS/Renderers/OpenGL/GLLoader.h
		GS/Renderers/OpenGL/GLState.h
		GS/Renderers/OpenGL/GSDeviceOGL.h
		GS/Renderers/OpenGL/GSTextureOGL.h
		GS/Renderers/OpenGL/GSUniformBufferOGL.h
	)
	target_link_libraries(PCSX2_FLAGS INTERFACE glad)
endif()

if(USE_VULKAN)
	list(APPEND pcsx2GSSources
		GS/Renderers/Vulkan/GSDeviceVK.cpp
		GS/Renderers/Vulkan/GSTextureVK.cpp
	)
	list(APPEND pcsx2GSHeaders
		GS/Renderers/Vulkan/GSDeviceVK.h
		GS/Renderers/Vulkan/GSTextureVK.h
	)
endif()

set(pcsx2GSMetalShaders
	GS/Renderers/Metal/convert.metal
	GS/Renderers/Metal/merge.metal
	GS/Renderers/Metal/interlace.metal
	GS/Renderers/Metal/tfx.metal
	GS/Renderers/Metal/fxaa.metal
)

if(NOT PCSX2_CORE)
	list(APPEND pcsx2GSSources
		GS/Window/GSwxDialog.cpp
	)
	list(APPEND pcsx2GSHeaders
		GS/Window/GSwxDialog.h
	)
endif()

if(PCSX2_CORE)
	list(APPEND pcsx2SPU2Headers
		SPU2/Host/Config.cpp
		SPU2/Host/ConfigDebug.cpp
		SPU2/Host/ConfigSoundTouch.cpp
		SPU2/Host/Dialogs.cpp
	)
	list(APPEND pcsx2SPU2Headers
		SPU2/Host/Config.h
		SPU2/Host/Dialogs.h
	)
	if(NOT WIN32)
		list(APPEND pcsx2SPU2Headers
			SPU2/WavFile.cpp
		)
	endif()
elseif(WIN32)
	list(APPEND pcsx2SPU2Sources
		SPU2/Windows/CfgHelpers.cpp
		SPU2/Windows/Config.cpp
		SPU2/Windows/ConfigDebug.cpp
		SPU2/Windows/ConfigSoundTouch.cpp
		SPU2/Windows/dsp.cpp
		SPU2/Windows/RealtimeDebugger.cpp
		SPU2/Windows/SndOut_XAudio2.cpp
		SPU2/Windows/UIHelpers.cpp
	)
	list(APPEND pcsx2SPU2Headers
		SPU2/Windows/Dialogs.h
		SPU2/Windows/dsp.h
		SPU2/Windows/resource.h
		SPU2/Windows/WinConfig.h
	)

	list(APPEND pcsx2DEV9Sources
		DEV9/Win32/pcap_io_win32.cpp
		DEV9/Win32/tap-win32.cpp
	)
	list(APPEND pcsx2DEV9Headers
		DEV9/Win32/pcap_io_win32_funcs.h
		DEV9/Win32/tap.h
	)

	list(APPEND pcsx2USBSources
		USB/qemu-usb/input-keymap-win32-to-qcode.cpp
		USB/shared/hidapi.cpp
		USB/shared/rawinput_usb.cpp
		USB/usb-eyetoy/api_init_win32_eyetoy.cpp
		USB/usb-eyetoy/cam-windows.cpp
		USB/usb-hid/api_init_win32_hid.cpp
		USB/usb-hid/raw/rawinput.cpp
		USB/usb-mic/api_init_win32_mic.cpp
		USB/usb-mic/audiodev-wasapi.cpp
		USB/usb-msd/usb-msd-win32.cpp
		USB/usb-pad/api_init_win32_pad.cpp
		USB/usb-pad/dx/dinput-config.cpp
		USB/usb-pad/dx/dinput.cpp
		USB/usb-pad/dx/usb-pad-dx.cpp
		USB/usb-pad/raw/raw-config.cpp
		USB/usb-pad/raw/usb-pad-raw.cpp
		USB/Win32/Config_usb.cpp
	)
	list(APPEND pcsx2USBHeaders
		USB/qemu-usb/input-keymap-win32-to-qcode.h
		USB/shared/rawinput_usb.h
		USB/usb-eyetoy/cam-windows.h
		USB/usb-mic/audiodev-wasapi.h
		USB/usb-pad/dx/dx.h
		USB/usb-pad/dx/usb-pad-dx.h
		USB/usb-pad/dx/versionproxy.h
		USB/usb-pad/raw/raw-config-res.h
		USB/usb-pad/raw/usb-pad-raw.h
		USB/Win32/Config_usb.h
		USB/Win32/resource_usb.h
	)

	list(APPEND pcsx2GSSources
		GS/Renderers/DX11/D3D.cpp
		GS/Renderers/DX11/GSDevice11.cpp
		GS/Renderers/DX11/GSTexture11.cpp
		GS/Renderers/DX11/GSTextureFX11.cpp
		GS/Renderers/DX12/GSDevice12.cpp
		GS/Renderers/DX12/GSTexture12.cpp
		GS/Window/GSCaptureDlg.cpp
		GS/Window/GSDialog.cpp
	)
	list(APPEND pcsx2GSHeaders
		GS/Renderers/DX11/D3D.h
		GS/Renderers/DX11/GSDevice11.h
		GS/Renderers/DX11/GSTexture11.h
		GS/Renderers/DX12/GSDevice12.h
		GS/Renderers/DX12/GSTexture12.h
	)
else()
	list(APPEND pcsx2SPU2Sources
		SPU2/Linux/CfgHelpers.cpp
		SPU2/Linux/Config.cpp
		SPU2/Linux/ConfigDebug.cpp
		SPU2/Linux/ConfigSoundTouch.cpp
		SPU2/Linux/Dialogs.cpp
		SPU2/WavFile.cpp
		SPU2/wx/wxConfig.cpp
	)
	list(APPEND pcsx2SPU2Headers
		SPU2/Linux/Config.h
		SPU2/Linux/Dialogs.h
		SPU2/wx/wxConfig.h
	)

	list(APPEND pcsx2USBSources
		USB/icon_buzz_24.cpp
		USB/linux/config-gtk.cpp
		USB/linux/config.cpp
		USB/linux/util.cpp
		USB/qemu-usb/input-keymap-linux-to-qcode.cpp
		USB/usb-eyetoy/api_init_linux.cpp
		USB/usb-eyetoy/cam-linux.cpp
		USB/usb-hid/api_init_linux.cpp
		USB/usb-hid/evdev/evdev-gtk.cpp
		USB/usb-mic/api_init_linux.cpp
		USB/usb-msd/usb-msd-gtk.cpp
		USB/usb-pad/api_init_linux.cpp
		USB/usb-pad/evdev/evdev-ff.cpp
		USB/usb-pad/evdev/evdev-gtk.cpp
		USB/usb-pad/evdev/evdev.cpp
		USB/usb-pad/evdev/shared-gtk.cpp
	)
	list(APPEND pcsx2USBHeaders
		USB/gtk.h
		USB/icon_buzz_24.h
		USB/linux/actualfile.h
		USB/linux/config.h
		USB/linux/ini.h
		USB/linux/util.h
		USB/qemu-usb/input-keymap-linux-to-qcode.h
		USB/usb-eyetoy/cam-linux.h
		USB/usb-hid/evdev/evdev.cpp
		USB/usb-hid/evdev/evdev.h
		USB/usb-pad/evdev/evdev-ff.h
		USB/usb-pad/evdev/evdev.h
		USB/usb-pad/evdev/shared.h
	)
endif()


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

# Frontend sources
set(pcsx2FrontendSources
	Frontend/ImGuiManager.cpp
)

# Frontend headers
set(pcsx2FrontendHeaders
	Frontend/ImGuiManager.h
)

if(USE_OPENGL)
	list(APPEND pcsx2FrontendSources
		Frontend/OpenGLHostDisplay.cpp
		Frontend/imgui_impl_opengl3.cpp
	)
	list(APPEND pcsx2FrontendHeaders
		Frontend/OpenGLHostDisplay.h
		Frontend/imgui_impl_opengl3.h
	)
endif()

if(USE_VULKAN)
	list(APPEND pcsx2FrontendSources
		Frontend/VulkanHostDisplay.cpp
		Frontend/imgui_impl_vulkan.cpp
	)
	list(APPEND pcsx2FrontendHeaders
		Frontend/imgui_impl_vulkan.h
		Frontend/VulkanHostDisplay.h
	)
endif()

if(WIN32)
	list(APPEND pcsx2FrontendSources
		Frontend/D3D11HostDisplay.cpp
		Frontend/D3D12HostDisplay.cpp
		Frontend/imgui_impl_dx11.cpp
		Frontend/imgui_impl_dx12.cpp
	)
	list(APPEND pcsx2FrontendHeaders
		Frontend/D3D11HostDisplay.h
		Frontend/D3D12HostDisplay.h
		Frontend/imgui_impl_dx11.h
		Frontend/imgui_impl_dx12.h
	)
elseif(APPLE)
	list(APPEND pcsx2GSSources
		GS/Renderers/Metal/GSDeviceMTL.mm
		GS/Renderers/Metal/GSMTLDeviceInfo.mm
		GS/Renderers/Metal/GSTextureMTL.mm
	)
	list(APPEND pcsx2GSHeaders
		GS/Renderers/Metal/GSDeviceMTL.h
		GS/Renderers/Metal/GSMetalCPPAccessible.h
		GS/Renderers/Metal/GSMTLDeviceInfo.h
		GS/Renderers/Metal/GSMTLSharedHeader.h
		GS/Renderers/Metal/GSMTLShaderCommon.h
		GS/Renderers/Metal/GSTextureMTL.h
	)
	list(APPEND pcsx2FrontendSources
		Frontend/MetalHostDisplay.mm
	)
	list(APPEND pcsx2FrontendHeaders
		Frontend/MetalHostDisplay.h
	)
endif()

if(PCSX2_CORE)
	list(APPEND pcsx2FrontendSources
		Frontend/GameList.cpp
		Frontend/INISettingsInterface.cpp
		Frontend/InputManager.cpp
		Frontend/InputSource.cpp
		Frontend/LayeredSettingsInterface.cpp
		GSDumpReplayer.cpp
		HostSettings.cpp
		VMManager.cpp
	)
	list(APPEND pcsx2FrontendHeaders
		Frontend/GameList.h
		Frontend/INISettingsInterface.h
		Frontend/InputManager.h
		Frontend/InputSource.h
		Frontend/LayeredSettingsInterface.h
		GSDumpReplayer.h
		HostSettings.h
		VMManager.h)
endif()

# gui sources
set(pcsx2GuiSources
	gui/AppAssert.cpp
	gui/AppConfig.cpp
	gui/AppCoreThread.cpp
	gui/AppEventSources.cpp
	gui/AppHost.cpp
	gui/AppUserMode.cpp
	gui/AppInit.cpp
	gui/AppMain.cpp
	gui/AppRes.cpp
	gui/CheckedStaticBox.cpp
	gui/ConsoleLogger.cpp
	gui/Dialogs/AboutBoxDialog.cpp
	gui/Dialogs/GSDumpDialog.cpp
	gui/Dialogs/AssertionDialog.cpp
	gui/Dialogs/BaseConfigurationDialog.cpp
	gui/Dialogs/ConfirmationDialogs.cpp
	gui/Dialogs/ConvertMemoryCardDialog.cpp
	gui/Dialogs/CreateMemoryCardDialog.cpp
	gui/Dialogs/FirstTimeWizard.cpp
	gui/Dialogs/ImportSettingsDialog.cpp
	gui/Dialogs/LogOptionsDialog.cpp
	gui/Dialogs/McdConfigDialog.cpp
	gui/Dialogs/PickUserModeDialog.cpp
	gui/Dialogs/SysConfigDialog.cpp
	gui/Dialogs/PINEDialog.cpp
	gui/Debugger/BreakpointWindow.cpp
	gui/Debugger/CtrlDisassemblyView.cpp
	gui/Debugger/CtrlRegisterList.cpp
	gui/Debugger/CtrlMemView.cpp
	gui/Debugger/CtrlMemSearch.cpp
	gui/Debugger/DebuggerLists.cpp
	gui/Debugger/DisassemblyDialog.cpp
	gui/Debugger/DebugEvents.cpp
	gui/DriveList.cpp
	gui/ExecutorThread.cpp
	gui/FastFormatString.cpp
	gui/FileUtils.cpp
	gui/FrameForGS.cpp
	gui/GlobalCommands.cpp
	gui/IniInterface.cpp
	gui/i18n.cpp
	gui/IsoDropTarget.cpp
	gui/MainFrame.cpp
	gui/MainMenuClicks.cpp
	gui/MessageBoxes.cpp
	gui/Mutex.cpp
	gui/MSWstuff.cpp
	gui/Panels/BaseApplicableConfigPanel.cpp
	gui/Panels/BiosSelectorPanel.cpp
	gui/Panels/CpuPanel.cpp
	gui/Panels/DirPickerPanel.cpp
	gui/Panels/GameFixesPanel.cpp
	gui/Panels/GSWindowPanel.cpp
	gui/Panels/LogOptionsPanels.cpp
	gui/Panels/MemoryCardListPanel.cpp
	gui/Panels/MemoryCardListView.cpp
	gui/Panels/MiscPanelStuff.cpp
	gui/Panels/PathsPanel.cpp
	gui/Panels/SpeedhacksPanel.cpp
	gui/Panels/VideoPanel.cpp
	gui/PathUtils.cpp
	gui/PersistentThread.cpp
	gui/pxCheckBox.cpp
	gui/pxRadioPanel.cpp
	gui/pxStaticText.cpp
	gui/pxTranslate.cpp
	gui/pxWindowTextWriter.cpp
	gui/RecentIsoList.cpp
	gui/Saveslots.cpp
	gui/StringHelpers.cpp
	gui/SysCoreThread.cpp
	gui/SysState.cpp
	gui/SysThreadBase.cpp
	gui/ThreadingDialogs.cpp
	gui/UpdateUI.cpp
	gui/wxAppWithHelpers.cpp
	gui/wxGuiTools.cpp
	gui/wxHelpers.cpp
	gui/wxSettingsInterface.cpp
	)

# gui headers
set(pcsx2GuiHeaders
	gui/AppAccelerators.h
	gui/AppCommon.h
	gui/AppConfig.h
	gui/AppCoreThread.h
	gui/AppEventListeners.h
	gui/AppHost.h
	gui/AppForwardDefs.h
	gui/App.h
	gui/ApplyState.h
	gui/AppSaveStates.h
	gui/CheckedStaticBox.h
	gui/ConsoleLogger.h
	gui/Debugger/BreakpointWindow.h
	gui/Debugger/CtrlDisassemblyView.h
	gui/Debugger/CtrlMemView.h
	gui/Debugger/CtrlMemSearch.h
	gui/Debugger/CtrlRegisterList.h
	gui/Debugger/DebugEvents.h
	gui/Debugger/DebuggerLists.h
	gui/Debugger/DisassemblyDialog.h
	gui/Dialogs/BaseConfigurationDialog.inl
	gui/Dialogs/ConfigurationDialog.h
	gui/Dialogs/LogOptionsDialog.h
	gui/Dialogs/ModalPopups.h
	gui/DriveList.h
	gui/GSFrame.h
	gui/i18n.h
	gui/IniInterface.h
	gui/IsoDropTarget.h
	gui/MainFrame.h
	gui/MSWstuff.h
	gui/PathDefs.h
	gui/Panels/ConfigurationPanels.h
	gui/Panels/LogOptionsPanels.h
	gui/Panels/MemoryCardPanels.h
	gui/PersistentThread.h
	gui/pxCheckBox.h
	gui/pxEvents.h
	gui/pxEventThread.h
	gui/pxRadioPanel.h
	gui/pxStaticText.h
	gui/RecentIsoList.h
	gui/Saveslots.h
	gui/StringHelpers.h
	gui/SysThreads.h
	gui/ThreadingDialogs.h
	gui/ThreadingDialogs.cpp
	gui/wxDirName.h
	gui/wxGuiTools.h
	gui/wxSettingsInterface.h
	)

# Warning: the declaration of the .h are mandatory in case of resources files. It will ensure the creation
# from the bin2cpp tools at right good moment (ie .h must be created before the pcsx2 compilation)
# Gui resources headers
set(res_bin "${CMAKE_BINARY_DIR}/pcsx2/gui/Resources")
set(res_src "${CMAKE_SOURCE_DIR}/pcsx2/gui/Resources")
set(pcsx2GuiResources
	${res_bin}/AppIcon16.h
	${res_bin}/AppIcon32.h
	${res_bin}/AppIcon64.h
	${res_bin}/BackgroundLogo.h
	${res_bin}/ButtonIcon_Camera.h
	${res_bin}/ConfigIcon_Cpu.h
	${res_bin}/ConfigIcon_Gamefixes.h
	${res_bin}/ConfigIcon_MemoryCard.h
	${res_bin}/ConfigIcon_Paths.h
	${res_bin}/ConfigIcon_Speedhacks.h
	${res_bin}/ConfigIcon_Video.h
	${res_bin}/Logo.h
	${res_bin}/NoIcon.h
	${res_bin}/Breakpoint_Active.h
	${res_bin}/Breakpoint_Inactive.h
	)

# IPU sources
set(pcsx2IPUSources
	IPU/IPU.cpp
	IPU/IPU_Fifo.cpp
	IPU/IPUdither.cpp
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
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	Linux/LnxFlatFileReader.cpp
	)

set(pcsx2OSXSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

set(pcsx2FreeBSDSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

if(NOT PCSX2_CORE)
	list(APPEND pcsx2LinuxSources
		Linux/LnxConsolePipe.cpp
		Linux/LnxKeyCodes.cpp
	)
	list(APPEND pcsx2OSXSources
		Linux/LnxConsolePipe.cpp
		Darwin/DarwinKeyCodes.cpp
	)
	list(APPEND pcsx2FreeBSDSources
		Linux/LnxConsolePipe.cpp
		Linux/LnxKeyCodes.cpp
	)
endif()

# Linux headers
set(pcsx2LinuxHeaders
	)

# ps2 sources
set(pcsx2ps2Sources
	ps2/BiosTools.cpp
	ps2/pgif.cpp
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

# Recording sources
set(rec_src "Recording")
set(rec_vp_src "${rec_src}/VirtualPad")
set(pcsx2RecordingSources
	${rec_src}/InputRecording.cpp
	${rec_src}/InputRecordingControls.cpp
	${rec_src}/InputRecordingFile.cpp
	${rec_src}/NewRecordingFrame.cpp
	${rec_src}/PadData.cpp
	${rec_src}/Utilities/InputRecordingLogger.cpp
	${rec_vp_src}/VirtualPad.cpp
	${rec_vp_src}/VirtualPadData.cpp
	${rec_vp_src}/VirtualPadResources.cpp
)

# Recording headers
set(pcsx2RecordingHeaders
	${rec_src}/InputRecording.h
	${rec_src}/InputRecordingControls.h
	${rec_src}/InputRecordingFile.h
	${rec_src}/NewRecordingFrame.h
	${rec_src}/PadData.h
	${rec_src}/Utilities/InputRecordingLogger.h
	${rec_vp_src}/VirtualPad.h
	${rec_vp_src}/VirtualPadData.h
	${rec_vp_src}/VirtualPadResources.h
)

# Warning: the declaration of the .h are mandatory in case of resources files. It will ensure the creation
# from the bin2cpp tools at the right moment (ie .h must be created before the pcsx2 compilation)
# Recording - VirtualPad resources headers
set(res_rec_vp_src "${CMAKE_SOURCE_DIR}/pcsx2/Recording/VirtualPad/img")
set(pcsx2RecordingVirtualPadResources
	${res_rec_vp_src}/circlePressed.h
	${res_rec_vp_src}/controllerFull.h
	${res_rec_vp_src}/controllerHalf.h
	${res_rec_vp_src}/controllerThreeQuarters.h
	${res_rec_vp_src}/crossPressed.h
	${res_rec_vp_src}/downPressed.h
	${res_rec_vp_src}/l1Pressed.h
	${res_rec_vp_src}/l2Pressed.h
	${res_rec_vp_src}/l3Pressed.h
	${res_rec_vp_src}/leftPressed.h
	${res_rec_vp_src}/r1Pressed.h
	${res_rec_vp_src}/r2Pressed.h
	${res_rec_vp_src}/r3Pressed.h
	${res_rec_vp_src}/rightPressed.h
	${res_rec_vp_src}/selectPressed.h
	${res_rec_vp_src}/squarePressed.h
	${res_rec_vp_src}/startPressed.h
	${res_rec_vp_src}/trianglePressed.h
	${res_rec_vp_src}/upPressed.h
)

# System headers
set(pcsx2SystemHeaders
	System/RecTypes.h)

# Windows sources
set(pcsx2WindowsSources
	CDVD/Windows/DriveUtility.cpp
	CDVD/Windows/IOCtlSrc.cpp
	windows/FlatFileReaderWindows.cpp
	windows/Optimus.cpp
	windows/VCprojects/IopSif.cpp
	windows/WinConsolePipe.cpp
	windows/WinKeyCodes.cpp
	windows/WinPowerProfile.cpp
	)

# Windows headers
set(pcsx2WindowsHeaders
	windows/resource.h
	windows/Win32.h)

# x86 sources
set(pcsx2x86Sources
	x86/BaseblockEx.cpp
	x86/iCOP0.cpp
	x86/iCore.cpp
	x86/iFPU.cpp
	x86/iFPUd.cpp
	x86/iMMI.cpp
	x86/iR3000A.cpp
	x86/iR3000Atables.cpp
	x86/iR5900Analysis.cpp
	x86/iR5900Misc.cpp
	x86/ir5900tables.cpp
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
	x86/iR5900Analysis.h
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
	)

# These ones benefit a lot from LTO
set(pcsx2LTOSources
	${pcsx2Sources}
	${pcsx2Headers}
	${pcsx2IPUSources}
	${pcsx2IPUHeaders}
	${pcsx2x86Sources}
	${pcsx2x86Headers}
)

if(LTO_PCSX2_CORE)
	add_library(PCSX2_LTO ${pcsx2LTOSources})
	target_link_libraries(PCSX2_LTO PRIVATE PCSX2_FLAGS)
	target_link_libraries(PCSX2 PRIVATE PCSX2_LTO)
	set_target_properties(PCSX2_LTO PROPERTIES INTERPROCEDURAL_OPTIMIZATION true)
else()
	target_sources(PCSX2 PRIVATE ${pcsx2LTOSources})
endif()

# common Sources
target_sources(PCSX2 PRIVATE
	${pcsx2CDVDSources}
	${pcsx2CDVDHeaders}
	${pcsx2SPU2Sources}
	${pcsx2SPU2Headers}
	${pcsx2DEV9Sources}
	${pcsx2DEV9Headers}
	${pcsx2PADHeaders}
	${pcsx2PADSources}
	${pcsx2GSHeaders}
	${pcsx2GSSources}
	${pcsx2RecordingSources}
	${pcsx2DebugToolsSources}
	${pcsx2DebugToolsHeaders}
	${pcsx2FrontendSources}
	${pcsx2FrontendHeaders}
	${pcsx2ps2Sources}
	${pcsx2ps2Headers}
	${pcsx2SystemHeaders}
)

# gui sources when not doing a qt build
if (NOT PCSX2_CORE)
	target_sources(PCSX2 PRIVATE
		${pcsx2GuiSources}
		${pcsx2GuiResources}
		${pcsx2GuiHeaders}
		${pcsx2RecordingVirtualPadResources}
		${pcsx2RecordingHeaders}
	)
endif()

# platform sources
# Linux
if(Linux)
	target_sources(PCSX2 PRIVATE
		${pcsx2LinuxSources}
		${pcsx2LinuxHeaders}
		)

	target_link_libraries(PCSX2_FLAGS INTERFACE
		PkgConfig::AIO
		PkgConfig::LIBUDEV
		ALSA::ALSA
	)
endif()

if(UNIX AND NOT APPLE AND NOT PCSX2_CORE)
	if(X11_API)
		target_link_libraries(PCSX2_FLAGS INTERFACE
			PkgConfig::X11_XCB
			PkgConfig::XCB
			X11::X11
		)
	endif()
	if(WAYLAND_API)
		target_link_libraries(PCSX2_FLAGS INTERFACE
			Wayland::Client
		)
	endif()
endif()

# Windows
if(Windows)
	target_sources(PCSX2 PRIVATE
		${pcsx2WindowsSources}
		${pcsx2WindowsHeaders})
endif()

# MacOSX/BSD
if(UNIX AND NOT Linux)
	target_sources(PCSX2 PRIVATE
		${pcsx2OSXSources}
		${pcsx2LinuxHeaders}
		${pcsx2USBNullSources}
		${pcsx2USBNullHeaders})
else()
	if(NOT PCSX2_CORE)
		target_sources(PCSX2 PRIVATE
			${pcsx2USBSources}
			${pcsx2USBHeaders})
	else()
		target_sources(PCSX2 PRIVATE
			${pcsx2USBNullSources}
			${pcsx2USBNullHeaders})
	endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR ${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	target_sources(PCSX2 PRIVATE
		${pcsx2FreeBSDSources}
		${pcsx2LinuxHeaders})
endif()

target_link_libraries(PCSX2_FLAGS INTERFACE
	common
	imgui
	fmt::fmt
	ryml
	chdr-static
	libzip::zip
	ZLIB::ZLIB
	PkgConfig::SOUNDTOUCH
	PkgConfig::SAMPLERATE
	PNG::PNG
	LibLZMA::LibLZMA
	Zstd::Zstd
	${LIBC_LIBRARIES}
)

if(PCSX2_CORE)
	target_link_libraries(PCSX2_FLAGS INTERFACE
		simpleini
	)
else()
	target_link_libraries(PCSX2_FLAGS INTERFACE
		wxWidgets::all
	)
endif()

if(WIN32)
	target_link_libraries(PCSX2_FLAGS INTERFACE
		baseclasses
		WIL::WIL
		D3D12MemAlloc
		setupapi.lib
		ws2_32.lib
		shlwapi.lib
		iphlpapi.lib
		dsound.lib
		dxguid.lib
		dinput8.lib
		hid.lib
		PowrProf.lib
		d3dcompiler.lib
		d3d11.lib
		dxgi.lib
		strmiids.lib
		opengl32.lib
		comsuppw.lib
	)
	if(NOT PCSX2_CORE)
		target_link_libraries(PCSX2_FLAGS INTERFACE
			pthreads4w
		)
	endif()
elseif(APPLE)
	target_link_libraries(PCSX2_FLAGS INTERFACE
		PCAP::PCAP
	)
else()
	if (NOT PCSX2_CORE)
		target_link_libraries(PCSX2_FLAGS INTERFACE
			GTK::gtk
			HarfBuzz::HarfBuzz
		)
	endif()
	target_link_libraries(PCSX2_FLAGS INTERFACE
		OpenGL::GL
		PCAP::PCAP
	)
endif()

### Generate the resources files
if(NOT PCSX2_CORE)
	file(MAKE_DIRECTORY ${res_bin})

	foreach(res_file IN ITEMS
			AppIcon16 AppIcon32 AppIcon64 BackgroundLogo Logo NoIcon ButtonIcon_Camera
			ConfigIcon_Cpu ConfigIcon_Gamefixes ConfigIcon_MemoryCard
			ConfigIcon_Paths ConfigIcon_Speedhacks ConfigIcon_Video Breakpoint_Active Breakpoint_Inactive)
		add_custom_command(
			OUTPUT "${res_bin}/${res_file}.h"
			DEPENDS ${BIN2CPPDEP} "${res_src}/${res_file}.png"
			COMMAND ${BIN2CPP} "${res_src}/${res_file}.png" "${res_bin}/${res_file}"
		)
	endforeach()

	### Generate Recording resource files
	### Drop them into the folder alongside the png files
	foreach(res_file IN ITEMS
			circlePressed controllerFull controllerHalf controllerThreeQuarters crossPressed downPressed l1Pressed l2Pressed l3Pressed leftPressed
			r1Pressed r2Pressed r3Pressed rightPressed selectPressed squarePressed startPressed trianglePressed upPressed)
		add_custom_command(
			OUTPUT "${res_rec_vp_src}/${res_file}.h"
			DEPENDS ${BIN2CPPDEP} "${res_rec_vp_src}/${res_file}.png"
			COMMAND ${BIN2CPP} "${res_rec_vp_src}/${res_file}.png" "${res_rec_vp_src}/${res_file}"
		)
	endforeach()
endif()

# additonal include directories
target_include_directories(PCSX2_FLAGS INTERFACE
	.
	x86
	${CMAKE_BINARY_DIR}/pcsx2
	${CMAKE_BINARY_DIR}/common/include/
	"${CMAKE_SOURCE_DIR}/3rdparty/jpgd/"
	"${CMAKE_SOURCE_DIR}/3rdparty/xbyak/"
)

if(COMMAND target_precompile_headers)
	message("Using precompiled headers.")
	target_precompile_headers(PCSX2_FLAGS INTERFACE PrecompiledHeader.h)
endif()

# Copy resource files if needed
function(pcsx2_resource path basedir)
	get_filename_component(dir ${path} DIRECTORY)
	file(RELATIVE_PATH subdir ${basedir} ${dir})
	if(APPLE)
		target_sources(PCSX2 PRIVATE ${path})
		set_source_files_properties(${path} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/${subdir})
	elseif(PACKAGE_MODE)
		install(FILES ${path} DESTINATION ${CMAKE_INSTALL_DATADIR}/PCSX2/resources/${subdir})
	else()
		add_custom_command(TARGET PCSX2 POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:PCSX2>/resources/${subdir}"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${path}" "$<TARGET_FILE_DIR:PCSX2>/resources/${subdir}")
	endif()
	source_group(Resources/${subdir} FILES ${path})
endfunction()

file(GLOB_RECURSE RESOURCE_FILES ${CMAKE_SOURCE_DIR}/bin/resources/*)
foreach(path IN LISTS RESOURCE_FILES)
	get_filename_component(file ${path} NAME)
	if("${file}" MATCHES "^\\.") # Don't copy macOS garbage (mainly Finder's .DS_Store files) into application
		continue()
	endif()
	if (NOT WIN32 AND "${path}" MATCHES "/dx11/") # Don't include unneccessary stuff
		continue()
	endif()
	if ((GETTEXT_FOUND OR NO_TRANSLATION OR PCSX2_CORE) AND "${path}" MATCHES "/locale/") # Generate locales with gettext instead of copying them from bin
		continue()
	endif()
	pcsx2_resource(${path} ${CMAKE_SOURCE_DIR}/bin/resources/)
endforeach()

if (NOT APPLE)
	set_target_properties(PCSX2 PROPERTIES
		OUTPUT_NAME pcsx2
	)
endif()

if(APPLE AND NOT GETTEXT_FOUND AND NOT NO_TRANSLATION AND NOT PCSX2_CORE)
	message(FATAL_ERROR "MacOS cannot use the precompiled translation files.  Please install gettext or disable translations with NO_TRANSLATION")
endif()

# Translation files
# CMake is dumb and can't properly associate custom command outputs with targets if they're made in different directories
# So this has to be here (and not in a CMakeLists.txt file in locales/)
if(GETTEXT_FOUND AND NOT NO_TRANSLATION AND NOT PCSX2_CORE)
	include(Translation)

	file(GLOB_RECURSE PO_ICO_FILES  ${CMAKE_SOURCE_DIR}/locales/*/pcsx2_Iconized.po)
	file(GLOB_RECURSE PO_MAIN_FILES ${CMAKE_SOURCE_DIR}/locales/*/pcsx2_Main.po)

	# Macro to compile po file and install them
	GETTEXT_CREATE_TRANSLATIONS_PCSX2(${CMAKE_SOURCE_DIR}/locales/templates/pcsx2_Iconized.pot ${PO_ICO_FILES})
	GETTEXT_CREATE_TRANSLATIONS_PCSX2(${CMAKE_SOURCE_DIR}/locales/templates/pcsx2_Main.pot     ${PO_MAIN_FILES})
endif()

if (APPLE)
	find_library(METAL_LIBRARY Metal)
	target_link_libraries(PCSX2 PRIVATE ${METAL_LIBRARY})

	if(CMAKE_GENERATOR MATCHES "Xcode")
		# If we're generating an xcode project, you can just add the shaders to the main pcsx2 target and xcode will deal with them properly
		# This will make sure xcode supplies code completion, etc (if you use a custom command, it won't)
		set_target_properties(PCSX2 PROPERTIES
			XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO INCLUDE_SOURCE
		)
		foreach(shader IN LISTS pcsx2GSMetalShaders)
			target_sources(PCSX2 PRIVATE ${shader})
			set_source_files_properties(${shader} PROPERTIES LANGUAGE METAL)
		endforeach()
	else()
		function(generateMetallib std target outputName)
			set(pcsx2GSMetalShaderOut)
			set(flags
				-ffast-math
				$<$<NOT:$<CONFIG:Release,MinSizeRel>>:-gline-tables-only>
				$<$<NOT:$<CONFIG:Release,MinSizeRel>>:-MO>
			)
			foreach(shader IN LISTS pcsx2GSMetalShaders)
				set(shaderOut ${CMAKE_CURRENT_BINARY_DIR}/${outputName}/${shader}.air)
				list(APPEND pcsx2GSMetalShaderOut ${shaderOut})
				get_filename_component(shaderDir ${shaderOut} DIRECTORY)
				add_custom_command(OUTPUT ${shaderOut}
					COMMAND ${CMAKE_COMMAND} -E make_directory ${shaderDir}
					COMMAND xcrun metal ${flags} -std=${std} -target ${target} -o ${shaderOut} -c ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
					DEPENDS ${shader} GS/Renderers/Metal/GSMTLSharedHeader.h GS/Renderers/Metal/GSMTLShaderCommon.h
				)
				set(metallib ${CMAKE_CURRENT_BINARY_DIR}/${outputName}.metallib)
			endforeach()
			add_custom_command(OUTPUT ${metallib}
				COMMAND xcrun metallib -o ${metallib} ${pcsx2GSMetalShaderOut}
				DEPENDS ${pcsx2GSMetalShaderOut}
			)
			pcsx2_resource(${metallib} ${CMAKE_CURRENT_BINARY_DIR})
		endfunction()
		generateMetallib(macos-metal2.0 air64-apple-macos10.13 default)
		generateMetallib(macos-metal2.2 air64-apple-macos10.15 Metal22)
		generateMetallib(macos-metal2.3 air64-apple-macos11.0  Metal23)
	endif()

	# MacOS defaults to having a maximum protection of the __DATA segment of rw (non-executable)
	# We have a bunch of page-sized arrays in bss that we use for jit
	# Obviously not being able to make those arrays executable would be a problem
	target_link_options(PCSX2 PRIVATE -Wl,-segprot,__DATA,rwx,rw)

	set_target_properties(PCSX2 PROPERTIES
		MACOSX_BUNDLE true
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/gui/Resources/Info.plist.in"
		OUTPUT_NAME PCSX2
	)

	pcsx2_resource(${CMAKE_CURRENT_SOURCE_DIR}/gui/Resources/PCSX2.icns ${CMAKE_CURRENT_SOURCE_DIR}/gui/Resources)

	# If they say to skip postprocess bundle, leave the target in but make it so they have
	# to manually run it
	if (SKIP_POSTPROCESS_BUNDLE)
		set(postprocessBundleType "")
	else()
		set(postprocessBundleType ALL)
	endif()

	add_custom_target(pcsx2-postprocess-bundle ${postprocessBundleType}
		COMMAND ${CMAKE_COMMAND} "-DPCSX2_BUNDLE_PATH=$<TARGET_FILE_DIR:PCSX2>/../.."
			-P ${CMAKE_SOURCE_DIR}/cmake/Pcsx2PostprocessBundle.cmake
	)
	add_dependencies(pcsx2-postprocess-bundle PCSX2)

	get_target_property(PCSX2_SOURCES PCSX2 SOURCES)
	foreach(source IN LISTS PCSX2_SOURCES)
		# Set the right file types for .inl files in Xcode
		if("${source}" MATCHES "\\.(inl|h)$")
			set_source_files_properties("${source}" PROPERTIES XCODE_EXPLICIT_FILE_TYPE sourcecode.cpp.h)
		endif()
		# CMake makefile and ninja generators will attempt to share one PCH for both cpp and mm files
		# That's not actually OK
		if("${source}" MATCHES "\\.mm$")
			set_source_files_properties("${source}" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
		endif()
	endforeach()
endif()

if(NOT DISABLE_SETCAP AND NOT PCSX2_CORE)
	if(PACKAGE_MODE)
		install(CODE "execute_process(COMMAND /bin/bash -c \"echo 'Enabling networking capability on Linux...';set -x; [ -f '${CMAKE_INSTALL_FULL_BINDIR}/pcsx2' ] && sudo setcap 'CAP_NET_RAW+eip CAP_NET_ADMIN+eip' '${CMAKE_INSTALL_FULL_BINDIR}/pcsx2'; set +x\")")
	else()
		install(CODE "execute_process(COMMAND /bin/bash -c \"echo 'Enabling networking capability on Linux...';set -x; [ -f '${CMAKE_SOURCE_DIR}/bin/pcsx2' ] && sudo setcap 'CAP_NET_RAW+eip CAP_NET_ADMIN+eip' '${CMAKE_SOURCE_DIR}/bin/pcsx2'; set +x\")")
	endif()
endif()

source_groups_from_vcxproj_filters(pcsx2.vcxproj.filters)

# Unix-only files aren't in the vcxproj.filters
source_group(System/Ps2/DEV9 REGULAR_EXPRESSION DEV9/*)
source_group(System/Ps2/PAD FILES ${pcsx2PADSources} ${pcsx2PADHeaders})
source_group(System/Ps2/SPU2 REGULAR_EXPRESSION SPU2/*)
source_group(System/Ps2/USB REGULAR_EXPRESSION USB/*)
source_group(System/Ps2/GS/Renderers/Metal REGULAR_EXPRESSION GS/Renderers/Metal/*)

# Generated resource files
source_group(Resources/GUI FILES ${pcsx2GuiResources})
source_group(Resources/PAD FILES ${pcsx2PADResources})
source_group(Resources/Recording FILES ${pcsx2RecordingVirtualPadResources})
source_group(Resources REGULAR_EXPRESSION ${CMAKE_CURRENT_BINARY_DIR}/*)
