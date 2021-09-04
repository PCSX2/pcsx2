add_library(portaudio
	src/common/pa_allocation.c
	src/common/pa_converters.c
	src/common/pa_cpuload.c
	src/common/pa_debugprint.c
	src/common/pa_dither.c
	src/common/pa_front.c
	src/common/pa_process.c
	src/common/pa_ringbuffer.c
	src/common/pa_stream.c
	src/common/pa_trace.c
	src/hostapi/dsound/pa_win_ds.c
	src/hostapi/dsound/pa_win_ds_dynlink.c
	src/hostapi/wdmks/pa_win_wdmks.c
	src/hostapi/wmme/pa_win_wmme.c
	src/hostapi/wasapi/pa_win_wasapi.c
	src/os/win/pa_win_coinitialize.c
	src/os/win/pa_win_hostapis.c
	src/os/win/pa_win_util.c
	src/os/win/pa_win_waveformat.c
	src/os/win/pa_win_wdmks_utils.c
	src/os/win/pa_x86_plain_converters.c
	include/pa_asio.h
	include/pa_jack.h
	include/pa_linux_alsa.h
	include/pa_mac_core.h
	include/pa_win_ds.h
	include/pa_win_wasapi.h
	include/pa_win_waveformat.h
	include/pa_win_wdmks.h
	include/pa_win_wmme.h
	include/portaudio.h
	src/os/win/pa_win_coinitialize.h
)
target_compile_definitions(portaudio PRIVATE
	_CRT_SECURE_NO_WARNINGS
	_CRT_SECURE_NO_DEPRECATE
	PAWIN_USE_WDMKS_DEVICE_INFO
	PA_USE_DS=0
	PA_USE_WASAPI=1
	PA_USE_WDMKS=1
	PA_USE_WMME=0
	PA_USE_ASIO=0
	$<$<CONFIG:Debug>:PA_ENABLE_DEBUG_OUTPUT>
)
target_link_libraries(portaudio PRIVATE ksuser.lib)
target_include_directories(portaudio PUBLIC include PRIVATE src/common src/os/win)
add_library(PkgConfig::PORTAUDIO ALIAS portaudio)
