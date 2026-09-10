/**************************************************************

   groovymister_wrapper.h - GroovyMiSTer C wrapper API header file

   ---------------------------------------------------------

   GroovyMiSTer  noGPU client for Groovy_MiSTer core

 **************************************************************/

#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#include <dlfcn.h>
#define LIBTYPE void*
#define OPENLIB(libname) dlopen((libname), RTLD_LAZY)
#define LIBFUNC(libh, fn) dlsym((libh), (fn))
#define LIBERROR dlerror
#define CLOSELIB(libh) dlclose((libh))

#elif defined _WIN32
#include <windows.h>
#define LIBTYPE HINSTANCE
#define OPENLIB(libname) LoadLibrary(TEXT((libname)))
#define LIBFUNC(lib, fn) GetProcAddress((lib), (fn))

#define CLOSELIB(libp) FreeLibrary((libp))
#endif

#ifdef _WIN32
 /* GROOVYMISTER_WIN32_STATIC */
	#ifndef GROOVYMISTER_WIN32_STATIC
		#define MODULE_API_GMW __declspec(dllexport)
	#else
		#define MODULE_API_GMW
	#endif
#else
	#define MODULE_API_GMW
#endif /* _WIN32 */

#ifdef __linux__
#define LIBGMC "libgroovymister.so"
#elif _WIN32
#define LIBGMC "libgroovymister.dll"
#endif

/* joystick map */
#define GMW_JOY_RIGHT (1 << 0)
#define GMW_JOY_LEFT  (1 << 1)
#define GMW_JOY_DOWN  (1 << 2)
#define GMW_JOY_UP    (1 << 3)
#define GMW_JOY_B1    (1 << 4)
#define GMW_JOY_B2    (1 << 5)
#define GMW_JOY_B3    (1 << 6)
#define GMW_JOY_B4    (1 << 7)
#define GMW_JOY_B5    (1 << 8)
#define GMW_JOY_B6    (1 << 9)
#define GMW_JOY_B7    (1 << 10)
#define GMW_JOY_B8    (1 << 11)
#define GMW_JOY_B9    (1 << 12)
#define GMW_JOY_B10   (1 << 13)
#define GMW_JOY_B11   (1 << 14)
#define GMW_JOY_B12   (1 << 15)

/* The wire is GENERIC: bits 4..15 are Button 1..12 (GMW_JOY_B1..B12). The
   aliases below are the DualShock-type LABELING CONVENTION for those positions
   (the MiSTer OSD shows per-controller-type labels; equivalent positions line
   up across types, e.g. pos 1 = Cross = Xbox A). Per-device .map files on the
   MiSTer translate physical buttons to positions. Bits 12-15 (L2/R2/L3/R3)
   arrive even on the old 16-bit joystick packet. */
#define GMW_JOY_CROSS    GMW_JOY_B1
#define GMW_JOY_CIRCLE   GMW_JOY_B2
#define GMW_JOY_SQUARE   GMW_JOY_B3
#define GMW_JOY_TRIANGLE GMW_JOY_B4
#define GMW_JOY_L1       GMW_JOY_B5
#define GMW_JOY_R1       GMW_JOY_B6
#define GMW_JOY_SELECT   GMW_JOY_B7
#define GMW_JOY_START    GMW_JOY_B8
#define GMW_JOY_L2       GMW_JOY_B9
#define GMW_JOY_R2       GMW_JOY_B10
#define GMW_JOY_L3       GMW_JOY_B11
#define GMW_JOY_R3       GMW_JOY_B12

/* CMD_INIT byte[5] capability flags for gmw_set_input_caps. The client
   negotiates them internally (CMD_GET_VERSION probe before CMD_INIT), so
   requesting caps against a pre-v2 core simply lands on a v1 session.
   Check gmw_get_input_caps() for what was actually granted. */
#define GMW_CAP_INPUTS_V2 0x01 /* joystick packets v2: 32-bit masks + analog triggers */
#define GMW_CAP_RUMBLE    0x02 /* client may send rumble messages on the inputs socket */
/* Promises this client sends keepalives while idle, which is what licenses the core to close
   a silent session. Set it with gmw_set_keepalive(1), NOT gmw_set_input_caps - it is not an
   input capability. Without it the core holds a quiet session indefinitely. */
#define GMW_CAP_KEEPALIVE 0x04 /* client sends CMD_GET_STATUS keepalives while idle */

/* FPGA data received on ACK */
typedef struct MODULE_API_GMW
{
	uint32_t frame;		//frame on gpu
	uint32_t frameEcho;	//frame received
	uint16_t vCount;	//vertical count on gpu
	uint16_t vCountEcho; 	//vertical received

	uint8_t vramEndFrame; 	//1-fpga has all pixels on vram for last CmdBlit
	uint8_t vramReady;	//1-fpga has free space on vram
	uint8_t vramSynced;	//1-fpga is up. NOT an underrun detector: the condition self-clears
			//within about a raster line, so this per-blit sample almost never catches one
	uint8_t vgaFrameskip;	//1-fpga used framebuffer (volatile framebuffer off)
	uint8_t vgaVblank;	//1-fpga is on vblank
	uint8_t vgaF1;		//1-field for interlaced
	uint8_t audio;		//1-fgpa has audio activated
	uint8_t vramQueue; 	//1-fpga has pixels prepared on vram
} gmw_fpgaStatus;

/* Inputs v2: joy1/joy2 widened uint16_t -> uint32_t and the four trigger
   fields appended. The struct layout changed, so consumers must recompile. */
typedef struct MODULE_API_GMW{
	uint32_t joyFrame;	//joystick blit frame
	uint8_t  joyOrder;	//joystick blit order
	uint32_t joy1;	 	//joystick 1 map (v2: 32-bit; v1 cores fill the low 16 bits)
	uint32_t joy2;	 	//joystick 2 map
	char     joy1LXAnalog; 	//joystick 1 L-Analog X
	char     joy1LYAnalog; 	//joystick 1 L-Analog Y
	char     joy1RXAnalog; 	//joystick 1 R-Analog X
	char     joy1RYAnalog; 	//joystick 1 R-Analog Y
	char     joy2LXAnalog; 	//joystick 2 L-Analog X
	char     joy2LYAnalog; 	//joystick 2 L-Analog Y
	char     joy2RXAnalog; 	//joystick 2 R-Analog X
	char     joy2RYAnalog; 	//joystick 2 R-Analog Y
	uint8_t  joy1LTAnalog; 	//joystick 1 L-Trigger 0..255 (v2 analog packet only)
	uint8_t  joy1RTAnalog; 	//joystick 1 R-Trigger
	uint8_t  joy2LTAnalog; 	//joystick 2 L-Trigger
	uint8_t  joy2RTAnalog; 	//joystick 2 R-Trigger
} gmw_fpgaJoyInputs;

typedef struct MODULE_API_GMW{
	uint32_t ps2Frame;	//ps2 blit frame
	uint8_t  ps2Order;	//ps2 blit order
	uint8_t  ps2Keys[32]; 	//bit array with sdl scancodes convention
	uint8_t  ps2Mouse;	//byte 0 ps2 mouse [yo,xo,ys,xs,1,bm,br,bl]
	uint8_t  ps2MouseX; 	//byte 1 ps2 mouse X
	uint8_t  ps2MouseY; 	//byte 2 ps2 mouse Y
	uint8_t  ps2MouseZ; 	//byte 3 ps2 mouse Z
}  gmw_fpgaPS2Inputs;

enum Lz4FramesCode { //gmw_init lz4Frames
    LZ4_OFF = 0, //RAW frames
    LZ4 = 1,
    LZ4_DELTA = 2,
    LZ4_HC = 3,
    LZ4_HC_DELTA = 4,
    LZ4_ADPTATIVE = 5,
    LZ4_ADPTATIVE_DELTA = 6,
    // NLC near-lossless codec. The entropy front-end (TILED / RICE) is a separate
    // knob; see gmw_set_nlc_pack, and tune quality via gmw_set_near_level.
    NLC = 7
};

enum SoundRateCode { //gmw_init soundRate
    RATE_OFF = 0,
    RATE_22050 = 1,
    RATE_44100 = 2,
    RATE_48000 = 3
};

enum SoundChanCode { //gmw_init soundChan
    CHAN_OFF = 0,
    CHAN_MONO = 1,
    CHAN_STEREO = 2
};

enum RGBModeCode { //gmw_init rgbMode
    RGB_888 = 0,
    RGB_A888 = 1,
    RGB_565 = 2
};

/* Declaration of the wrapper functions */

// Init streaming with ip, port and mtu size (typical 1500 or 3800 for MiSTer jumbo frames)
// A negative value is returned if connection fails
MODULE_API_GMW int gmw_init(const char* misterHost, uint8_t lz4Frames, uint32_t soundRate, uint8_t soundChan, uint8_t rgbMode, uint16_t mtu);
// Close stream
MODULE_API_GMW void gmw_close(void);
// Send only the CMD_CLOSE datagram (no socket teardown). For shutdown paths on
// threads that no longer drain the RIO queues. Safe to call repeatedly and
// before gmw_close.
MODULE_API_GMW void gmw_send_close(void);
// Send a 1-byte CMD_GET_STATUS keepalive on the video socket to hold an idle
// session against the core's idle timeout. Call while alive but not blitting.
// Only matters if you advertised GMW_CAP_KEEPALIVE via gmw_set_keepalive(1):
// without it the core never closes a silent session in the first place.
MODULE_API_GMW void gmw_send_keepalive(void);
// 1 if the shared connection (from gmw_init) is live. Pad code should check
// this before touching the input socket so it never creates a second client.
MODULE_API_GMW uint8_t gmw_is_connected(void);
// Monotonic auto-reconnect counter (see gmw_set_auto_reconnect). The client
// replays the stashed modeline itself after a reconnect; hosts that keep their
// own modeline state can watch this to know a reconnect happened.
MODULE_API_GMW uint32_t gmw_reconnect_epoch(void);
// Change resolution (check https://github.com/antonioginer/switchres) for modeline generation (interlace=2 for progressive framebuffer)
// Returns 0 once the core has acknowledged the modeline, -1 if it never did. That failure is worth
// handling: the core only sets its expected frame length from a CmdSwitchres it actually processed,
// so a lost one leaves it discarding every frame of the session with nothing else to show for it.
MODULE_API_GMW int gmw_switchres(double pClock, uint16_t hActive, uint16_t hBegin, uint16_t hEnd, uint16_t hTotal, uint16_t vActive, uint16_t vBegin, uint16_t vEnd, uint16_t vTotal, uint8_t interlace);
// This buffer are registered and aligned for sending rgb. Populate it before gmw_blit
MODULE_API_GMW char* gmw_get_pBufferBlit(uint8_t field);
// This buffer are registered and aligned for sending rgb. Populate it before gmw_blit. Here will be difference between actual frame and last with 8-bit overflow
MODULE_API_GMW char* gmw_get_pBufferBlitDelta(void);
// Stream frame, field = 0 for progressive, vCountSync = 0 for auto frame delay or number of vertical line to sync with, margin with nanoseconds for auto frame delay), matchDeltaBytes for delta frames
MODULE_API_GMW void gmw_blit(uint32_t frame, uint8_t field, uint16_t vCountSync, uint32_t margin, uint32_t matchDeltaBytes);
// This buffer are registered and aligned for sending rgb. Populate it before gmw_audio
MODULE_API_GMW char* gmw_get_pBufferAudio(void);
// Stream audio
MODULE_API_GMW void gmw_audio(uint16_t soundSize);
// sleep to sync with crt raster
MODULE_API_GMW void gmw_waitSync(void);
// get nanoseconds (positive or negative) to sync with raster
MODULE_API_GMW int gmw_diffTimeRaster(void);
// getACK is used internal on WaitSync, dwMilliseconds = 0 will time out immediately if no new data
MODULE_API_GMW uint32_t gmw_getACK(uint8_t dwMilliseconds);
// get fpga status from last ACK received
MODULE_API_GMW void gmw_getStatus(gmw_fpgaStatus* status);
// listen inputs from MiSTer
MODULE_API_GMW void gmw_bindInputs(const char* misterHost);
// refresh inputs
MODULE_API_GMW void gmw_pollInputs(void);
// Re-send the 1-byte input subscribe on the existing socket (UDP-loss
// insurance for threaded clients). Only meaningful after gmw_bindInputs.
MODULE_API_GMW void gmw_resubscribe_inputs(void);
// GMW_CAP_* capability flags, requested for the next gmw_init. Call BEFORE
// gmw_init (creates the client object if needed, so ordering vs gmw_bindInputs
// does not matter). 0 = legacy v1, byte-identical 5-byte init. The client
// probes the core version and drops to v1 automatically when the core cannot
// take the caps byte; the value persists across internal auto-reconnects.
MODULE_API_GMW void gmw_set_input_caps(uint8_t caps);
// Caps actually NEGOTIATED for the live session (0 when not connected or when
// the probe landed on v1). Gate rumble/trigger handling on this, not on what
// was requested.
MODULE_API_GMW uint8_t gmw_get_input_caps(void);
// Advertise GMW_CAP_KEEPALIVE, before gmw_init. Enable it ONLY if you actually call
// gmw_send_keepalive() while idle, more often than the core's shortest OSD idle timeout
// (5s today): opting in is what allows the core to close you for going quiet. Off by
// default, so a client that never opts in may pause indefinitely.
MODULE_API_GMW void gmw_set_keepalive(uint8_t on);
// Rumble player 0/1's pad (strong/weak motor 0..255). Requires GMW_CAP_RUMBLE
// negotiated; the MiSTer gates it per pad in OSD -> System -> Controllers ->
// <player> -> Rumble (default On), and force-stops motors on session
// close. Send on state change only: the core repeats the last value until
// replaced (0/0 stops). Internally guarded: no-op when not connected, inputs
// not bound, or rumble not negotiated.
MODULE_API_GMW void gmw_send_rumble(uint8_t player, uint8_t strong, uint8_t weak);
// get joystick inputs
MODULE_API_GMW void gmw_getJoyInputs(gmw_fpgaJoyInputs* joyInputs);
// get ps2 inputs
MODULE_API_GMW void gmw_getPS2Inputs(gmw_fpgaPS2Inputs* ps2Inputs);

// get version
MODULE_API_GMW const char* gmw_get_version(void);
// Verbose level 0,1,2 (min to max)
MODULE_API_GMW void gmw_set_log_level(int level);
// Install a process-wide log sink BEFORE gmw_init so the full handshake and
// failure trace is captured even when stdout is unavailable (Windows GUI
// apps). verbose: 0=errors only ... 2=full trace (applied via the log level).
// fn may be called from any thread.
MODULE_API_GMW void gmw_set_log_callback(void (*fn)(const char* msg), int verbose);
// Codec-corpus capture: start dumping uncompressed pre-encode frames into
// <dir>/frame_WxH_fmt_NNNNNN.raw, auto-stop after maxFrames (0 = default 120).
// dir=NULL/"" disables. Also auto-enabled by env GM_FRAME_DUMP=<dir>
// (GM_FRAME_DUMP_MAX caps the count). Call from the thread driving gmw_blit.
MODULE_API_GMW void gmw_set_frame_dump(const char* dir, uint32_t maxFrames);
// NLC codec tuning, effective when gmw_init lz4Frames = NLC (7). All ride the
// CMD_INIT byte[1] packing so they MUST be called BEFORE gmw_init.
// near: 0 = lossless (default), 1..3 = +-k near-lossless quantization. Bits [3:2].
// near 1 is the recommended setting for heavy 3D content (it is what brings the
// stream under the core's ~38 MB/s ingest ceiling; ±1 is analog-invisible).
MODULE_API_GMW void gmw_set_near_level(uint8_t k);
// pack: NLC entropy front-end (byte[1] bit 7). 1 = TILED (block-adaptive widths,
// default; better on flat/2D content), 2 = RICE (Golomb-Rice; better on
// photographic/3D content). RICE REQUIRES a core with the Rice decoder (the
// rbf_rice_r3 kit or newer); an older core ignores the bit and would misparse
// Rice bytes as TILED (garbage picture, no wedge). Values other than 2 clamp
// to TILED.
MODULE_API_GMW void gmw_set_nlc_pack(uint8_t pack);
// NLC display path (CMD_INIT byte[1] bits [6:5]): 0 = stream, 2 = autonomous
// decode engine (the default). Call before gmw_init.
MODULE_API_GMW void gmw_set_nlc_disp_mode(uint8_t mode);
// Opt-in ACK watchdog (default OFF): after 10 blits with no frameEcho advance
// the client reconnects transparently inside gmw_blit and replays the stashed
// modeline. The reconnect is video side only, so the inputs socket survives.
// Observe via gmw_reconnect_epoch().
MODULE_API_GMW void gmw_set_auto_reconnect(uint8_t on);

// NLC pre-encode fast path. The per-blit software encode can dominate the frame period on a slow
// CPU, so a host may encode a unique frame once, off the blit thread, and hand the result straight
// to the next blit. Order matters: gmw_switchres first (the encoder takes its width from the live
// modeline), then write an gmw_encode_nlc result into gmw_get_pBufferPreEncoded(), then
// gmw_set_pre_encoded_size(cSize) and gmw_blit. The size is one-shot - it applies to the next blit
// only, and that blit skips the software encoder. NLC codec (lz4Frames 7) only.
MODULE_API_GMW char* gmw_get_pBufferPreEncoded(void);
MODULE_API_GMW void gmw_set_pre_encoded_size(uint32_t cSize);
// Encode one frame with the exact parameters gmw_blit would use. Returns the encoded size, 0 on
// failure. Call after gmw_switchres.
MODULE_API_GMW uint32_t gmw_encode_nlc(const char* rgbFrame, char* out);


/* Inspired by https://stackoverflow.com/a/1067684 */
typedef struct MODULE_API_GMW
{
	int  (*init)(const char* misterHost, uint8_t lz4Frames, uint32_t soundRate, uint8_t soundChan, uint8_t rgbMode, uint16_t mtu);
	void (*close)(void);
	int  (*switchres)(double pClock, uint16_t hActive, uint16_t hBegin, uint16_t hEnd, uint16_t hTotal, uint16_t vActive, uint16_t vBegin, uint16_t vEnd, uint16_t vTotal, uint8_t interlace);
	char*(*get_pBufferBlit)(uint8_t field);
	char*(*get_pBufferBlitDelta)(void);
	void (*blit)(uint32_t frame, uint8_t field, uint16_t vCountSync, uint32_t margin, uint32_t matchDeltaBytes);
	char*(*get_pBufferAudio)(void);
	void (*audio)(uint16_t soundSize);
	void (*waitSync)(void);
	int  (*diffTimeRaster)(void);   /* was declared void: the symbol has always returned int */
	uint32_t (*getACK)(uint8_t dwMilliseconds);
	void (*getStatus)(gmw_fpgaStatus* status);
	void (*bindInputs)(const char* misterHost);
	void (*pollInputs)(void);
	void (*getJoyInputs)(gmw_fpgaJoyInputs* joyInputs);
	void (*getPS2Inputs)(gmw_fpgaPS2Inputs* ps2Inputs);
	const char* (*get_version)(void);
	void (*set_log_level) (int level);
	/* v2 additions, appended only; existing offsets unchanged */
	void (*send_close)(void);
	uint8_t (*is_connected)(void);
	uint32_t (*reconnect_epoch)(void);
	void (*resubscribe_inputs)(void);
	void (*set_input_caps)(uint8_t caps);
	uint8_t (*get_input_caps)(void);
	void (*send_rumble)(uint8_t player, uint8_t strong, uint8_t weak);
	void (*set_log_callback)(void (*fn)(const char* msg), int verbose);
	void (*set_frame_dump)(const char* dir, uint32_t maxFrames);
	void (*set_near_level)(uint8_t k);
	void (*set_nlc_pack)(uint8_t pack);
	void (*set_nlc_disp_mode)(uint8_t mode);
	void (*set_auto_reconnect)(uint8_t on);
	void (*send_keepalive)(void);   /* appended only; existing offsets unchanged */
	char*(*get_pBufferPreEncoded)(void);
	void (*set_pre_encoded_size)(uint32_t cSize);
	uint32_t (*encode_nlc)(const char* rgbFrame, char* out);
	void (*set_keepalive)(uint8_t on);   /* appended only; existing offsets unchanged */
} gmwAPI;


#ifdef __cplusplus
}
#endif
