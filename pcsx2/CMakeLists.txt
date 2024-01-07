# All flags, libraries, etc, that are shared between PCSX2 compilation files
add_library(PCSX2_FLAGS INTERFACE)

if(DISABLE_ADVANCE_SIMD OR LTO_PCSX2_CORE)
	# Fixes issues with some compiler + linker combinations
	add_library(PCSX2 OBJECT)
else()
	add_library(PCSX2)
endif()
target_link_libraries(PCSX2 PRIVATE PCSX2_FLAGS)

target_compile_features(PCSX2_FLAGS INTERFACE cxx_std_17)
target_compile_definitions(PCSX2_FLAGS INTERFACE "${PCSX2_DEFS}")
target_compile_options(PCSX2_FLAGS INTERFACE "${PCSX2_WARNINGS}")

# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
	message(FATAL_ERROR "
	You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
	It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()

if(MSVC)
	if (NOT USE_CLANG_CL AND MSVC_VERSION GREATER_EQUAL 1930)
		target_compile_options(PCSX2_FLAGS INTERFACE /fp:contract)
	endif()
	target_compile_options(PCSX2_FLAGS INTERFACE /GS-)
else()
	target_compile_options(PCSX2_FLAGS INTERFACE
		-ffp-contract=fast
		-fno-strict-aliasing
		-Wstrict-aliasing # Allow to track strict aliasing issue.
		-Wno-parentheses
		-Wno-missing-braces
		-Wno-unknown-pragmas
	)
endif()

if(USE_LINKED_FFMPEG)
	target_compile_definitions(PCSX2_FLAGS INTERFACE USE_LINKED_FFMPEG)
	target_link_libraries(PCSX2_FLAGS INTERFACE FFMPEG::avcodec FFMPEG::avformat FFMPEG::avutil FFMPEG::swscale FFMPEG::swresample)
endif()

if(WIN32)
	set(MIN_WIN32 0x0A00)
	target_compile_definitions(PCSX2_FLAGS INTERFACE
		WINVER=${MIN_WIN32}
		_WIN32_WINNT=${MIN_WIN32}
		WIN32_LEAN_AND_MEAN
		LZMA_API_STATIC
		WIL_SUPPRESS_EXCEPTIONS
	)
endif(WIN32)

# Main pcsx2 source
set(pcsx2Sources
	Achievements.cpp
	Cache.cpp
	COP0.cpp
	COP2.cpp
	Counters.cpp
	Dmac.cpp
	GameDatabase.cpp
	Elfheader.cpp
	FW.cpp
	FiFo.cpp
	FPU.cpp
	GameList.cpp
	Gif.cpp
	Gif_Logger.cpp
	Gif_Unit.cpp
	GS.cpp
	GSDumpReplayer.cpp
	Host.cpp
	Hotkeys.cpp
	Hw.cpp
	HwRead.cpp
	HwWrite.cpp
	LayeredSettingsInterface.cpp
	INISettingsInterface.cpp
	Interpreter.cpp
	IopBios.cpp
	IopCounters.cpp
	IopDma.cpp
	IopGte.cpp
	IopHw.cpp
	IopIrq.cpp
	IopMem.cpp
	PINE.cpp
	Mdec.cpp
	Memory.cpp
	MMI.cpp
	MTGS.cpp
	MTVU.cpp
	MultipartFileReader.cpp
	Patch.cpp
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
	SIO/Sio.cpp
	SIO/Sio2.cpp
	SIO/Sio0.cpp
	SIO/Multitap/MultitapProtocol.cpp
	SIO/Memcard/MemoryCardFile.cpp
	SIO/Memcard/MemoryCardFolder.cpp
	SIO/Memcard/MemoryCardProtocol.cpp
	SourceLog.cpp
	SPR.cpp
	StateWrapper.cpp
	Vif0_Dma.cpp
	Vif1_Dma.cpp
	Vif1_MFIFO.cpp
	Vif.cpp
	Vif_Codes.cpp
	Vif_Transfer.cpp
	Vif_Unpack.cpp
	VMManager.cpp
	vtlb.cpp
	VU0.cpp
	VUmicro.cpp
	VU0micro.cpp
	VU0microInterp.cpp
	VU1micro.cpp
	VU1microInterp.cpp
	VUflags.cpp
	VUmicroMem.cpp
	VUops.cpp
)

# Main pcsx2 header
set(pcsx2Headers
	Achievements.h
	AsyncFileReader.h
	Cache.h
	Common.h
	Config.h
	COP0.h
	Counters.h
	Dmac.h
	GameDatabase.h
	Elfheader.h
	FW.h
	GameList.h
	Gif.h
	Gif_Unit.h
	GS.h
	GSDumpReplayer.h
	Hardware.h
	Host.h
	Hw.h
	INISettingsInterface.h
	IopBios.h
	IopCounters.h
	IopDma.h
	IopGte.h
	IopHw.h
	IopMem.h
	LayeredSettingsInterface.h
	PINE.h
	Mdec.h
	MTGS.h
	MTVU.h
	Memory.h
	MemoryTypes.h
	Patch.h
	PerformanceMetrics.h
	PrecompiledHeader.h
	R3000A.h
	R5900.h
	R5900OpcodeTables.h
	SaveState.h
	ShaderCacheVersion.h
	Sifcmd.h
	Sif.h
	SIO/Sio.h
	SIO/Sio2.h
	SIO/Sio0.h
	SIO/SioTypes.h
	SIO/Multitap/MultitapProtocol.h
	SIO/Memcard/MemoryCardFile.h
	SIO/Memcard/MemoryCardFolder.h
	SIO/Memcard/MemoryCardProtocol.h
	SPR.h
	SupportURLs.h
	StateWrapper.h
	Vif_Dma.h
	Vif.h
	Vif_Unpack.h
	VMManager.h
	vtlb.h
	VUflags.h
	VUmicro.h
	VUops.h)

# CDVD sources
set(pcsx2CDVDSources
	CDVD/BlockdumpFileReader.cpp
	CDVD/Ps1CD.cpp
	CDVD/CDVDcommon.cpp
	CDVD/CDVD.cpp
	CDVD/CDVDdiscReader.cpp
	CDVD/CDVDisoReader.cpp
	CDVD/CDVDdiscThread.cpp
	CDVD/InputIsoFile.cpp
	CDVD/IsoHasher.cpp
	CDVD/IsoReader.cpp
	CDVD/OutputIsoFile.cpp
	CDVD/ChunksCache.cpp
	CDVD/CompressedFileReader.cpp
	CDVD/ChdFileReader.cpp
	CDVD/CsoFileReader.cpp
	CDVD/GzippedFileReader.cpp
	CDVD/ThreadedFileReader.cpp
	)

# CDVD headers
set(pcsx2CDVDHeaders
	CDVD/Ps1CD.h
	CDVD/CDVDcommon.h
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
	CDVD/IsoHasher.h
	CDVD/IsoReader.h
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
	SPU2/RegTable.cpp
	SPU2/Reverb.cpp
	SPU2/SndOut.cpp
	SPU2/SndOut_Cubeb.cpp
	SPU2/spu2freeze.cpp
	SPU2/spu2sys.cpp
	SPU2/Wavedump_wav.cpp
)

set(pcsx2SPU2SourcesUnshared
	SPU2/ReverbResample.cpp
)

# SPU2 headers
set(pcsx2SPU2Headers
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
)

if(WIN32)
	list(APPEND pcsx2SPU2Sources
		SPU2/SndOut_XAudio2.cpp
	)
endif()

# DEV9 sources
set(pcsx2DEV9Sources
	DEV9/AdapterUtils.cpp
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
	DEV9/PacketReader/ARP/ARP_PacketEditor.cpp
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
	DEV9/PacketReader/EthernetFrameEditor.cpp
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
	DEV9/AdapterUtils.h
	DEV9/ATA/ATA.h
	DEV9/ATA/HddCreate.h
	DEV9/DEV9.h
	DEV9/InternalServers/DHCP_Server.cpp
	DEV9/InternalServers/DNS_Logger.h
	DEV9/InternalServers/DNS_Server.h
	DEV9/net.h
	DEV9/PacketReader/ARP/ARP_Packet.h
	DEV9/PacketReader/ARP/ARP_PacketEditor.h
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
	DEV9/PacketReader/EthernetFrameEditor.h
	DEV9/PacketReader/MAC_Address.h
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

# USB sources
set(pcsx2USBSources
	USB/USB.cpp
	USB/deviceproxy.cpp
	USB/qemu-usb/bus.cpp
	USB/qemu-usb/core.cpp
	USB/qemu-usb/desc.cpp
	USB/qemu-usb/hid.cpp
	USB/qemu-usb/input-keymap-qcode-to-qnum.cpp
	USB/qemu-usb/usb-ohci.cpp
	USB/shared/ringbuffer.cpp
	USB/usb-eyetoy/jo_mpeg.cpp
	USB/usb-eyetoy/usb-eyetoy-webcam.cpp
	USB/usb-hid/usb-hid.cpp
	USB/usb-lightgun/guncon2.cpp
	USB/usb-mic/audiodev-cubeb.cpp
	USB/usb-mic/usb-headset.cpp
	USB/usb-mic/usb-mic-logitech.cpp
	USB/usb-mic/usb-mic-singstar.cpp
	USB/usb-msd/usb-msd.cpp
	USB/usb-pad/lg/lg_ff.cpp
	USB/usb-pad/usb-pad-ff.cpp
	USB/usb-pad/usb-pad-sdl-ff.cpp
	USB/usb-pad/usb-pad.cpp
	USB/usb-pad/usb-seamic.cpp
	USB/usb-pad/usb-turntable.cpp
	USB/usb-printer/usb-printer.cpp
)

# USB headers
set(pcsx2USBHeaders
	USB/USB.h
	USB/deviceproxy.h
	USB/qemu-usb/USBinternal.h
	USB/qemu-usb/desc.h
	USB/qemu-usb/hid.h
	USB/qemu-usb/input-keymap.h
	USB/qemu-usb/queue.h
	USB/qemu-usb/qusb.h
	USB/shared/ringbuffer.h
	USB/usb-eyetoy/jo_mpeg.h
	USB/usb-eyetoy/ov519.h
	USB/usb-eyetoy/usb-eyetoy-webcam.h
	USB/usb-eyetoy/videodev.h
	USB/usb-hid/usb-hid.h
	USB/usb-lightgun/guncon2.h
	USB/usb-mic/audio.h
	USB/usb-mic/audiodev-cubeb.h
	USB/usb-mic/audiodev-noop.h
	USB/usb-mic/audiodev.h
	USB/usb-mic/usb-headset.h
	USB/usb-mic/usb-mic-singstar.h
	USB/usb-msd/usb-msd.h
	USB/usb-pad/lg/lg_ff.h
	USB/usb-pad/usb-pad-sdl-ff.h
	USB/usb-pad/usb-pad.h
	USB/usb-printer/usb-printer.h
)

# Host PAD
set(pcsx2PADSources
	SIO/Pad/Pad.cpp
	SIO/Pad/PadBase.cpp
	SIO/Pad/PadDualshock2.cpp
	SIO/Pad/PadGuitar.cpp
	SIO/Pad/PadPopn.cpp
	SIO/Pad/PadNotConnected.cpp
)
set(pcsx2PADHeaders
	SIO/Pad/Pad.h
	SIO/Pad/PadBase.h
	SIO/Pad/PadDualshock2.h
	SIO/Pad/PadGuitar.h
	SIO/Pad/PadPopn.h
	SIO/Pad/PadNotConnected.h
	SIO/Pad/PadTypes.h
)

# GS sources
set(pcsx2GSSourcesUnshared
	GS/GSBlock.cpp
	GS/GSLocalMemoryMultiISA.cpp
	GS/GSXXH.cpp
	GS/Renderers/Common/GSVertexTraceFMM.cpp
	GS/Renderers/HW/GSRendererHWMultiISA.cpp
	GS/Renderers/SW/GSDrawScanline.cpp
	GS/Renderers/SW/GSDrawScanlineCodeGenerator.cpp
	GS/Renderers/SW/GSDrawScanlineCodeGenerator.all.cpp
	GS/Renderers/SW/GSRasterizer.cpp
	GS/Renderers/SW/GSRendererSW.cpp
	GS/Renderers/SW/GSSetupPrimCodeGenerator.cpp
	GS/Renderers/SW/GSSetupPrimCodeGenerator.all.cpp
)

set(pcsx2GSSources
	GS/GS.cpp
	GS/GSCapture.cpp
	GS/GSClut.cpp
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
	GS/MultiISA.cpp
	GS/Renderers/Common/GSDevice.cpp
	GS/Renderers/Common/GSDirtyRect.cpp
	GS/Renderers/Common/GSFunctionMap.cpp
	GS/Renderers/Common/GSRenderer.cpp
	GS/Renderers/Common/GSTexture.cpp
	GS/Renderers/Common/GSVertexTrace.cpp
	GS/Renderers/Null/GSRendererNull.cpp
	GS/Renderers/HW/GSHwHack.cpp
	GS/Renderers/HW/GSRendererHW.cpp
	GS/Renderers/HW/GSTextureCache.cpp
	GS/Renderers/HW/GSTextureReplacementLoaders.cpp
	GS/Renderers/HW/GSTextureReplacements.cpp
	GS/Renderers/SW/GSTextureCacheSW.cpp
	)

# GS headers
set(pcsx2GSHeaders
	GS/GSAlignedClass.h
	GS/GSBlock.h
	GS/GSCapture.h
	GS/GSClut.h
	GS/GSDrawingContext.h
	GS/GSDrawingEnvironment.h
	GS/GSDump.h
	GS/GSExtra.h
	GS/GSGL.h
	GS/GSRegs.h
	GS/GS.h
	GS/GSJobQueue.h
	GS/GSLocalMemory.h
	GS/GSLzma.h
	GS/GSPerfMon.h
	GS/GSPng.h
	GS/GSRingHeap.h
	GS/GSState.h
	GS/GSTables.h
	GS/GSUtil.h
	GS/GSVector.h
	GS/GSVector4.h
	GS/GSVector4i.h
	GS/GSVector8.h
	GS/GSVector8i.h
	GS/GSXXH.h
	GS/MultiISA.h
	GS/Renderers/Common/GSDevice.h
	GS/Renderers/Common/GSDirtyRect.h
	GS/Renderers/Common/GSFastList.h
	GS/Renderers/Common/GSFunctionMap.h
	GS/Renderers/Common/GSRenderer.h
	GS/Renderers/Common/GSTexture.h
	GS/Renderers/Common/GSVertex.h
	GS/Renderers/Common/GSVertexTrace.h
	GS/Renderers/Null/GSRendererNull.h
	GS/Renderers/HW/GSHwHack.h
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
	GS/Renderers/SW/GSVertexSW.h
	)

if(USE_OPENGL)
	list(APPEND pcsx2GSSources
		GS/Renderers/OpenGL/GLContext.cpp
		GS/Renderers/OpenGL/GLProgram.cpp
		GS/Renderers/OpenGL/GLShaderCache.cpp
		GS/Renderers/OpenGL/GLState.cpp
		GS/Renderers/OpenGL/GLStreamBuffer.cpp
		GS/Renderers/OpenGL/GSDeviceOGL.cpp
		GS/Renderers/OpenGL/GSTextureOGL.cpp
	)
	list(APPEND pcsx2GSHeaders
		GS/Renderers/OpenGL/GLContext.h
		GS/Renderers/OpenGL/GLProgram.h
		GS/Renderers/OpenGL/GLShaderCache.h
		GS/Renderers/OpenGL/GLState.h
		GS/Renderers/OpenGL/GLStreamBuffer.h
		GS/Renderers/OpenGL/GSDeviceOGL.h
		GS/Renderers/OpenGL/GSTextureOGL.h
	)
	target_link_libraries(PCSX2_FLAGS INTERFACE glad)

	if(WIN32)
		list(APPEND pcsx2GSSources GS/Renderers/OpenGL/GLContextWGL.cpp)
		list(APPEND pcsx2GSHeaders GS/Renderers/OpenGL/GLContextWGL.h)
		target_link_libraries(PCSX2_FLAGS INTERFACE opengl32.lib WinPixEventRuntime::WinPixEventRuntime)
	elseif(APPLE)
		list(APPEND pcsx2GSSources GS/Renderers/OpenGL/GLContextAGL.mm)
		list(APPEND pcsx2GSHeaders GS/Renderers/OpenGL/GLContextAGL.h)
	else()
		if(X11_API OR WAYLAND_API)
			list(APPEND pcsx2GSSources GS/Renderers/OpenGL/GLContextEGL.cpp)
			list(APPEND pcsx2GSHeaders GS/Renderers/OpenGL/GLContextEGL.h)
			target_link_libraries(PCSX2_FLAGS INTERFACE PkgConfig::EGL)
		endif()

		if(X11_API)
			list(APPEND pcsx2GSSources GS/Renderers/OpenGL/GLContextEGLX11.cpp)
			list(APPEND pcsx2GSHeaders GS/Renderers/OpenGL/GLContextEGLX11.h)
		endif()

		if(WAYLAND_API)
			list(APPEND pcsx2GSSources GS/Renderers/OpenGL/GLContextEGLWayland.cpp)
			list(APPEND pcsx2GSHeaders GS/Renderers/OpenGL/GLContextEGLWayland.h)

			# We load the Wayland libraries dynamically to avoid a runtime dependency.
			# So, only add the headers, don't link.
			target_include_directories(PCSX2_FLAGS INTERFACE ${Wayland_INCLUDE_DIRS})
		endif()
	endif()
endif()

if(USE_VULKAN)
	list(APPEND pcsx2GSSources
		GS/Renderers/Vulkan/GSDeviceVK.cpp
		GS/Renderers/Vulkan/GSTextureVK.cpp
		GS/Renderers/Vulkan/vk_mem_alloc.cpp
		GS/Renderers/Vulkan/VKBuilders.cpp
		GS/Renderers/Vulkan/VKLoader.cpp
		GS/Renderers/Vulkan/VKShaderCache.cpp
		GS/Renderers/Vulkan/VKStreamBuffer.cpp
		GS/Renderers/Vulkan/VKSwapChain.cpp
	)
	list(APPEND pcsx2GSHeaders
		GS/Renderers/Vulkan/GSDeviceVK.h
		GS/Renderers/Vulkan/GSTextureVK.h
		GS/Renderers/Vulkan/VKBuilders.h
		GS/Renderers/Vulkan/VKEntryPoints.h
		GS/Renderers/Vulkan/VKEntryPoints.inl
		GS/Renderers/Vulkan/VKLoader.h
		GS/Renderers/Vulkan/VKShaderCache.h
		GS/Renderers/Vulkan/VKStreamBuffer.h
		GS/Renderers/Vulkan/VKSwapChain.h
	)
	target_link_libraries(PCSX2_FLAGS INTERFACE Vulkan-Headers glslang)
endif()

set(pcsx2GSMetalShaders
	GS/Renderers/Metal/cas.metal
	GS/Renderers/Metal/convert.metal
	GS/Renderers/Metal/present.metal
	GS/Renderers/Metal/merge.metal
	GS/Renderers/Metal/misc.metal
	GS/Renderers/Metal/interlace.metal
	GS/Renderers/Metal/tfx.metal
	GS/Renderers/Metal/fxaa.metal
)

if(WIN32)
	list(APPEND pcsx2DEV9Sources
		DEV9/Win32/pcap_io_win32.cpp
		DEV9/Win32/tap-win32.cpp
	)
	list(APPEND pcsx2DEV9Headers
		DEV9/Win32/pcap_io_win32_funcs.h
		DEV9/Win32/tap.h
	)

	list(APPEND pcsx2USBSources
		USB/usb-eyetoy/cam-windows.cpp
	)
	list(APPEND pcsx2USBHeaders
		USB/usb-eyetoy/cam-windows.h
	)

	list(APPEND pcsx2GSSources
		GS/Renderers/DX11/D3D.cpp
		GS/Renderers/DX11/D3D11ShaderCache.cpp
		GS/Renderers/DX11/GSDevice11.cpp
		GS/Renderers/DX11/GSTexture11.cpp
		GS/Renderers/DX12/D3D12Builders.cpp
		GS/Renderers/DX12/D3D12DescriptorHeapManager.cpp
		GS/Renderers/DX12/D3D12ShaderCache.cpp
		GS/Renderers/DX12/D3D12StreamBuffer.cpp
		GS/Renderers/DX12/GSDevice12.cpp
		GS/Renderers/DX12/GSTexture12.cpp
	)
	list(APPEND pcsx2GSHeaders
		GS/Renderers/DX11/D3D.h
		GS/Renderers/DX11/D3D11ShaderCache.h
		GS/Renderers/DX11/GSDevice11.h
		GS/Renderers/DX11/GSTexture11.h
		GS/Renderers/DX12/D3D12Builders.h
		GS/Renderers/DX12/D3D12DescriptorHeapManager.h
		GS/Renderers/DX12/D3D12ShaderCache.h
		GS/Renderers/DX12/D3D12StreamBuffer.h
		GS/Renderers/DX12/GSDevice12.h
		GS/Renderers/DX12/GSTexture12.h
	)
elseif(LINUX)
	list(APPEND pcsx2USBSources
		USB/usb-eyetoy/cam-linux.cpp
	)
	list(APPEND pcsx2USBHeaders
		USB/usb-eyetoy/cam-linux.h
	)
else()
	list(APPEND pcsx2USBSources
		USB/usb-eyetoy/cam-noop.cpp
	)
endif()

# IPU sources
set(pcsx2IPUSources
	IPU/IPU.cpp
	IPU/IPU_Fifo.cpp
	IPU/IPUdma.cpp
)

set(pcsx2IPUSourcesUnshared
	IPU/IPU_MultiISA.cpp
	IPU/IPUdither.cpp
	IPU/yuv2rgb.cpp
)

# IPU headers
set(pcsx2IPUHeaders
	IPU/IPU.h
	IPU/IPU_Fifo.h
	IPU/IPU_MultiISA.h
	IPU/IPUdma.h
	IPU/mpeg2_vlc.h
	IPU/yuv2rgb.h
)

if(DISABLE_ADVANCE_SIMD)
	target_compile_definitions(PCSX2 PUBLIC MULTI_ISA_SHARED_COMPILATION)
	if(USE_GCC)
		target_link_options(PCSX2_FLAGS INTERFACE -Wno-odr)
	endif()
	if(WIN32)
		set(compile_options_avx2 /arch:AVX2)
		set(compile_options_avx  /arch:AVX)
	elseif(USE_GCC)
		# GCC can't inline into multi-isa functions if we use march and mtune, but can if we use feature flags
		set(compile_options_avx2 -msse4.1 -mavx -mavx2 -mbmi -mbmi2 -mfma)
		set(compile_options_avx  -msse4.1 -mavx)
		set(compile_options_sse4 -msse4.1)
	else()
		set(compile_options_avx2 -march=haswell -mtune=haswell)
		set(compile_options_avx  -march=sandybridge -mtune=sandybridge)
		set(compile_options_sse4 -msse4.1 -mtune=nehalem)
	endif()
	# ODR violation time!
	# Everything would be fine if we only defined things in cpp files, but C++ tends to like inline functions (STL anyone?)
	# Each ISA will bring with it its own copies of these inline header functions, and the linker gets to choose whichever one it wants!  Not fun if the linker chooses the avx2 version and uses it with everything
	# Thankfully, most linkers don't choose at random.  When presented with a bunch of .o files, most linkers seem to choose the first implementation they see, so make sure you order these from oldest to newest
	# Note: ld64 (macOS's linker) does not act the same way when presented with .a files, unless linked with `-force_load` (cmake WHOLE_ARCHIVE).
	set(is_first_isa "1")
	foreach(isa "sse4" "avx" "avx2")
		add_library(GS-${isa} STATIC ${pcsx2GSSourcesUnshared} ${pcsx2IPUSourcesUnshared} ${pcsx2SPU2SourcesUnshared})
		target_link_libraries(GS-${isa} PRIVATE PCSX2_FLAGS)
		target_compile_definitions(GS-${isa} PRIVATE MULTI_ISA_UNSHARED_COMPILATION=isa_${isa} MULTI_ISA_IS_FIRST=${is_first_isa} ${pcsx2_defs_${isa}})
		target_compile_options(GS-${isa} PRIVATE ${compile_options_${isa}})
		if (${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.24)
			target_link_libraries(PCSX2 PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,GS-${isa}>)
		elseif(APPLE)
			message(FATAL_ERROR "MacOS builds with DISABLE_ADVANCE_SIMD=ON require CMake 3.24")
		else()
			target_link_libraries(PCSX2 PRIVATE GS-${isa})
		endif()
		set(is_first_isa "0")
	endforeach()
else()
	list(APPEND pcsx2GSSources ${pcsx2GSSourcesUnshared})
	list(APPEND pcsx2IPUSources ${pcsx2IPUSourcesUnshared})
	list(APPEND pcsx2SPU2Sources ${pcsx2SPU2SourcesUnshared})
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

set(pcsx2ImGuiSources
	ImGui/FullscreenUI.cpp
	ImGui/ImGuiFullscreen.cpp
	ImGui/ImGuiManager.cpp
	ImGui/ImGuiOverlays.cpp
)

set(pcsx2ImGuiHeaders
	ImGui/FullscreenUI.h
	ImGui/ImGuiAnimated.h
	ImGui/ImGuiFullscreen.h
	ImGui/ImGuiManager.h
	ImGui/ImGuiOverlays.h
)

set(pcsx2InputSources
	Input/InputManager.cpp
	Input/InputSource.cpp
	Input/SDLInputSource.cpp
)
set(pcsx2InputHeaders
	Input/InputManager.h
	Input/InputSource.h
	Input/SDLInputSource.h
)

if(APPLE)
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
endif()

if(WIN32)
	# RAIntegration is only supported on Windows.
	target_compile_definitions(PCSX2_FLAGS INTERFACE ENABLE_RAINTEGRATION)
	target_link_libraries(PCSX2_FLAGS INTERFACE rainterface)
endif()
if(WIN32)
	list(APPEND pcsx2InputSources
		Input/DInputSource.cpp
		Input/XInputSource.cpp
	)
	list(APPEND pcsx2InputHeaders
		Input/DInputSource.h
		Input/XInputSource.h
	)
endif()

# Linux sources
set(pcsx2LinuxSources
	CDVD/Linux/DriveUtility.cpp
	CDVD/Linux/IOCtlSrc.cpp
	Linux/LnxFlatFileReader.cpp
	)

set(pcsx2OSXSources
	CDVD/Darwin/DriveUtility.cpp
	CDVD/Darwin/IOCtlSrc.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

set(pcsx2FreeBSDSources
	CDVD/Darwin/DriveUtility.cpp
	CDVD/Darwin/IOCtlSrc.cpp
	Darwin/DarwinFlatFileReader.cpp
	)

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
	${rec_src}/PadData.cpp
	${rec_src}/Utilities/InputRecordingLogger.cpp
)

# Recording headers
set(pcsx2RecordingHeaders
	${rec_src}/InputRecording.h
	${rec_src}/InputRecordingControls.h
	${rec_src}/InputRecordingFile.h
	${rec_src}/PadData.h
	${rec_src}/Utilities/InputRecordingLogger.h
)

# Windows sources
set(pcsx2WindowsSources
	CDVD/Windows/DriveUtility.cpp
	CDVD/Windows/IOCtlSrc.cpp
	windows/FlatFileReaderWindows.cpp
	windows/Optimus.cpp
)

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
	x86/ix86-32/iCore.cpp
	x86/ix86-32/iR5900.cpp
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
	${pcsx2SPU2Sources}
	${pcsx2SPU2Headers}
	${pcsx2GSSources}
	${pcsx2GSHeaders}
)

if(LTO_PCSX2_CORE)
	add_library(PCSX2_LTO ${pcsx2LTOSources})
	if (WIN32)
		target_compile_definitions(PCSX2_LTO PRIVATE _M_SSE=0x401)
	endif()
	if (DISABLE_ADVANCE_SIMD)
		target_compile_definitions(PCSX2_LTO PRIVATE MULTI_ISA_SHARED_COMPILATION)
	endif()
	target_link_libraries(PCSX2_LTO PRIVATE PCSX2_FLAGS)
	target_link_libraries(PCSX2 PRIVATE PCSX2_LTO)
	set_target_properties(PCSX2_LTO PROPERTIES INTERPROCEDURAL_OPTIMIZATION true)
	fixup_file_properties(PCSX2_LTO)
else()
	target_sources(PCSX2 PRIVATE ${pcsx2LTOSources})
endif()

# common Sources
target_sources(PCSX2 PRIVATE
	${pcsx2CDVDSources}
	${pcsx2CDVDHeaders}
	${pcsx2DEV9Sources}
	${pcsx2DEV9Headers}
	${pcsx2PADHeaders}
	${pcsx2PADSources}
	${pcsx2RecordingSources}
	${pcsx2DebugToolsSources}
	${pcsx2DebugToolsHeaders}
	${pcsx2ImGuiSources}
	${pcsx2ImGuiHeaders}
	${pcsx2InputSources}
	${pcsx2InputHeaders}
	${pcsx2ps2Sources}
	${pcsx2ps2Headers}
)

# platform sources
if(LINUX)
	target_sources(PCSX2 PRIVATE
		${pcsx2LinuxSources}
		${pcsx2LinuxHeaders}
		)

	target_link_libraries(PCSX2_FLAGS INTERFACE
		PkgConfig::AIO
		PkgConfig::LIBUDEV
	)
endif()

if(WIN32)
	target_sources(PCSX2 PRIVATE
		${pcsx2WindowsSources}
	)
endif()

target_sources(PCSX2 PRIVATE ${pcsx2USBSources} ${pcsx2USBHeaders})
target_link_libraries(PCSX2_FLAGS INTERFACE jpgd)

if(APPLE OR BSD)
	if(APPLE)
		target_sources(PCSX2 PRIVATE
			${pcsx2OSXSources})
	else()
		target_sources(PCSX2 PRIVATE
			${pcsx2FreeBSDSources})
	endif()
	target_sources(PCSX2 PRIVATE
		${pcsx2LinuxHeaders})
endif()

target_link_libraries(PCSX2_FLAGS INTERFACE
	common
	imgui
	fmt::fmt
	ryml
	libchdr
	libzip::zip
	cpuinfo
	zydis
	cubeb
	rcheevos
	discord-rpc
	SDL2::SDL2
	ZLIB::ZLIB
	LZ4::LZ4
	SoundTouch::SoundTouch
	PNG::PNG
	LibLZMA::LibLZMA
	Zstd::Zstd
	${LIBC_LIBRARIES}
)

target_link_libraries(PCSX2_FLAGS INTERFACE
	demangler
	simpleini
)

if(WIN32)
	target_link_libraries(PCSX2_FLAGS INTERFACE
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
		d3d12.lib
		dxgi.lib
		strmiids.lib
		opengl32.lib
		comsuppw.lib
		dwmapi.lib
		OneCore.lib
	)
elseif(APPLE)
	target_link_libraries(PCSX2_FLAGS INTERFACE
		PCAP::PCAP
	)
else()
	target_link_libraries(PCSX2_FLAGS INTERFACE
		OpenGL::GL
		PCAP::PCAP
	)
endif()

# additonal include directories
target_include_directories(PCSX2_FLAGS INTERFACE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_BINARY_DIR}/pcsx2"
	"${CMAKE_BINARY_DIR}/common/include"
	"${CMAKE_SOURCE_DIR}/3rdparty/xbyak"
	"${FFMPEG_INCLUDE_DIRS}"
)
target_compile_definitions(PCSX2_FLAGS INTERFACE
	XBYAK_NO_EXCEPTION
)

set_source_files_properties(PrecompiledHeader.cpp PROPERTIES HEADER_FILE_ONLY TRUE)
if(COMMAND target_precompile_headers)
	message("Using precompiled headers.")
	target_precompile_headers(PCSX2_FLAGS INTERFACE PrecompiledHeader.h)
endif()

# Copy resource files if needed
function(pcsx2_resource target path basedir)
	get_filename_component(dir ${path} DIRECTORY)
	file(RELATIVE_PATH subdir ${basedir} ${dir})
	if(APPLE)
		target_sources(${target} PRIVATE ${path})
		set_source_files_properties(${path} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/${subdir})
	else()
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${target}>/resources/${subdir}"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${path}" "$<TARGET_FILE_DIR:${target}>/resources/${subdir}")
	endif()
	source_group(Resources/${subdir} FILES ${path})
endfunction()

if (NOT APPLE)
	set_target_properties(PCSX2 PROPERTIES
		OUTPUT_NAME pcsx2
	)
endif()

fixup_file_properties(PCSX2)

if (APPLE)
	find_library(METAL_LIBRARY Metal)
	find_library(QUARTZCORE_LIBRARY QuartzCore)
	target_link_libraries(PCSX2_FLAGS INTERFACE ${METAL_LIBRARY} ${QUARTZCORE_LIBRARY})
endif()

set_property(GLOBAL PROPERTY PCSX2_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set_property(GLOBAL PROPERTY PCSX2_METAL_SHADERS ${pcsx2GSMetalShaders})

function(setup_main_executable target)
	file(GLOB_RECURSE RESOURCE_FILES ${CMAKE_SOURCE_DIR}/bin/resources/*)
	foreach(path IN LISTS RESOURCE_FILES)
		get_filename_component(file ${path} NAME)
		if("${file}" MATCHES "^\\.") # Don't copy macOS garbage (mainly Finder's .DS_Store files) into application
			continue()
		endif()
		if (NOT WIN32 AND "${path}" MATCHES "/dx11/") # Don't include unneccessary stuff
			continue()
		endif()
		pcsx2_resource(${target} ${path} ${CMAKE_SOURCE_DIR}/bin/resources/)
	endforeach()

	# Add docs to resources for not-Windows, since they don't use the bin directory.
	if(NOT WIN32)
		file(GLOB_RECURSE DOCS_FILES ${CMAKE_SOURCE_DIR}/bin/docs/*)
		foreach(path IN LISTS DOCS_FILES)
			get_filename_component(file ${path} NAME)
			if("${file}" MATCHES "^\\.") # Don't copy macOS garbage (mainly Finder's .DS_Store files) into application
				continue()
			endif()
			pcsx2_resource(${target} ${path} ${CMAKE_SOURCE_DIR}/bin/)
		endforeach()
	endif()

	get_property(PCSX2_SOURCE_DIR GLOBAL PROPERTY PCSX2_SOURCE_DIR)
	get_property(PCSX2_METAL_SHADERS GLOBAL PROPERTY PCSX2_METAL_SHADERS)

	get_target_property(MOC_EXECUTABLE_LOCATION Qt6::moc IMPORTED_LOCATION)
	get_filename_component(QT_BINARY_DIRECTORY "${MOC_EXECUTABLE_LOCATION}" DIRECTORY)

	if(WIN32)
		target_sources(${target} PRIVATE
			${PCSX2_SOURCE_DIR}/windows/PCSX2.manifest
			${PCSX2_SOURCE_DIR}/windows/PCSX2.rc
		)
		set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
		install(TARGETS ${target} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
		if(MSVC)
			install(FILES $<TARGET_PDB_FILE:${target}> DESTINATION ${CMAKE_SOURCE_DIR}/bin)
		endif()
		if(TARGET SDL2::SDL2)
			# Copy SDL2 DLL to binary directory.
			if(CMAKE_BUILD_TYPE STREQUAL "Debug")
				get_property(SDL2_DLL_PATH TARGET SDL2::SDL2 PROPERTY IMPORTED_LOCATION_DEBUG)
			else()
				get_property(SDL2_DLL_PATH TARGET SDL2::SDL2 PROPERTY IMPORTED_LOCATION_RELEASE)
			endif()
			if(SDL2_DLL_PATH)
				install(FILES ${SDL2_DLL_PATH} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
			endif()
		endif()
		find_program(WINDEPLOYQT_EXE windeployqt HINTS "${QT_BINARY_DIRECTORY}")
		install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXE}\" \"${CMAKE_SOURCE_DIR}/bin/$<TARGET_FILE_NAME:${target}>\" --plugindir \"${CMAKE_SOURCE_DIR}/bin/QtPlugins\" --no-compiler-runtime --no-system-d3d-compiler --no-translations COMMAND_ERROR_IS_FATAL ANY)")
		install(CODE "file(WRITE \"${CMAKE_SOURCE_DIR}/bin/qt.conf\" \"[Paths]\\nPlugins = ./QtPlugins\")")
	endif()

	if(APPLE)
		if(CMAKE_GENERATOR MATCHES "Xcode")
			# If we're generating an xcode project, you can just add the shaders to the main pcsx2 target and xcode will deal with them properly
			# This will make sure xcode supplies code completion, etc (if you use a custom command, it won't)
			set_target_properties(${target} PROPERTIES
				XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO INCLUDE_SOURCE
			)
			foreach(shader IN LISTS PCSX2_METAL_SHADERS)
				target_sources(${target} PRIVATE ${PCSX2_SOURCE_DIR}/${shader})
				set_source_files_properties(${PCSX2_SOURCE_DIR}/${shader} PROPERTIES LANGUAGE METAL)
			endforeach()
		else()
			function(generateMetallib std triple outputName)
				set(pcsx2GSMetalShaderOut)
				set(flags
					-ffast-math
					$<$<NOT:$<CONFIG:Release,MinSizeRel>>:-gline-tables-only>
					$<$<NOT:$<CONFIG:Release,MinSizeRel>>:-MO>
				)
				foreach(shader IN LISTS PCSX2_METAL_SHADERS)
					set(shaderOut ${CMAKE_CURRENT_BINARY_DIR}/${outputName}/${shader}.air)
					list(APPEND pcsx2GSMetalShaderOut ${shaderOut})
					get_filename_component(shaderDir ${shaderOut} DIRECTORY)
					add_custom_command(OUTPUT ${shaderOut}
						COMMAND ${CMAKE_COMMAND} -E make_directory ${shaderDir}
						COMMAND xcrun metal ${flags} -std=${std} -target ${triple} -o ${shaderOut} -c ${PCSX2_SOURCE_DIR}/${shader}
						DEPENDS ${PCSX2_SOURCE_DIR}/${shader} ${PCSX2_SOURCE_DIR}/GS/Renderers/Metal/GSMTLSharedHeader.h ${PCSX2_SOURCE_DIR}/GS/Renderers/Metal/GSMTLShaderCommon.h
					)
					set(metallib ${CMAKE_CURRENT_BINARY_DIR}/${outputName}.metallib)
				endforeach()
				add_custom_command(OUTPUT ${metallib}
					COMMAND xcrun metallib -o ${metallib} ${pcsx2GSMetalShaderOut}
					DEPENDS ${pcsx2GSMetalShaderOut}
				)
				pcsx2_resource(${target} ${metallib} ${CMAKE_CURRENT_BINARY_DIR})
			endfunction()
			generateMetallib(macos-metal2.0 air64-apple-macos10.13 default)
			generateMetallib(macos-metal2.2 air64-apple-macos10.15 Metal22)
			generateMetallib(macos-metal2.3 air64-apple-macos11.0  Metal23)
		endif()

		set_target_properties(${target} PROPERTIES
			MACOSX_BUNDLE true
			MACOSX_BUNDLE_INFO_PLIST "${PCSX2_SOURCE_DIR}/Resources/Info.plist.in"
			OUTPUT_NAME PCSX2
		)

		pcsx2_resource(${target} ${PCSX2_SOURCE_DIR}/Resources/PCSX2.icns ${PCSX2_SOURCE_DIR}/Resources)

		# If they say to skip postprocess bundle, leave the target in but make it so they have
		# to manually run it
		if (SKIP_POSTPROCESS_BUNDLE)
			set(postprocessBundleType "")
		else()
			set(postprocessBundleType ALL)
		endif()

		find_program(MACDEPLOYQT_EXE macdeployqt HINTS "${QT_BINARY_DIRECTORY}")
		add_custom_target(pcsx2-postprocess-bundle ${postprocessBundleType}
			COMMAND "${MACDEPLOYQT_EXE}" "$<TARGET_FILE_DIR:${target}>/../.." -no-strip
		)
		add_dependencies(pcsx2-postprocess-bundle ${target})
	endif()

	if(ENABLE_SETCAP AND UNIX AND NOT APPLE)
		message(WARNING "Networking capabilities enabled, building will require temporary root elevation.")
		add_custom_target(
			pcsx2-enable-setcap
			ALL
			DEPENDS ${target}
			COMMAND bash -c "set +x\; echo 'Elevating to enable networking capability on $<TARGET_FILE:${target}>...'\; sudo setcap 'CAP_NET_RAW+eip CAP_NET_ADMIN+eip' '$<TARGET_FILE:${target}>'"
		)
	endif()

	if(PCSX2_EXE_NAME)
		set_target_properties(${target} PROPERTIES OUTPUT_NAME ${PCSX2_EXE_NAME})
	endif()
endfunction()

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
