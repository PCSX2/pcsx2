#ifndef __GROOVYMISTER_H__
#define __GROOVYMISTER_H__

#include <inttypes.h>

#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX // keep windows.h from defining min/max macros that break <algorithm>-style client code
 #endif
 #include <winsock2.h>
 #include <ws2tcpip.h>
 #include <mswsock.h>
 #include "rio.h"
#else
 #include <cstring>
 #include <cstdio>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <time.h>
#endif

#ifndef GROOVYMISTER_VERSION
#define GROOVYMISTER_VERSION "1.0.0"
#endif

#define BUFFER_SIZE 1245312 // 720x576x3
#define BUFFER_SLICES 846
#define MTU_HEADER 28
#define BUFFER_MTU 1500 - MTU_HEADER

//joystick map
#define GM_JOY_RIGHT (1 << 0)
#define GM_JOY_LEFT  (1 << 1)
#define GM_JOY_DOWN  (1 << 2)
#define GM_JOY_UP    (1 << 3)
#define GM_JOY_B1    (1 << 4)
#define GM_JOY_B2    (1 << 5)
#define GM_JOY_B3    (1 << 6)
#define GM_JOY_B4    (1 << 7)
#define GM_JOY_B5    (1 << 8)
#define GM_JOY_B6    (1 << 9)
#define GM_JOY_B7    (1 << 10)
#define GM_JOY_B8    (1 << 11)
#define GM_JOY_B9    (1 << 12)
#define GM_JOY_B10   (1 << 13)
#define GM_JOY_B11   (1 << 14)
#define GM_JOY_B12   (1 << 15)

// The wire format is generic: bits 4..15 are Button 1..12 (GM_JOY_B1..B12). Groovy fronts
// many platforms, not just PlayStation, so the defines below are only the DualShock-type
// labelling convention for those positions. The MiSTer OSD shows per-controller-type
// labels, and equivalent positions line up across types, e.g. pos 1 = Cross = Xbox A.
// Per-device .map files on the MiSTer translate physical buttons to positions.
#define GM_JOY_CROSS    GM_JOY_B1
#define GM_JOY_CIRCLE   GM_JOY_B2
#define GM_JOY_SQUARE   GM_JOY_B3
#define GM_JOY_TRIANGLE GM_JOY_B4
#define GM_JOY_L1       GM_JOY_B5
#define GM_JOY_R1       GM_JOY_B6
#define GM_JOY_SELECT   GM_JOY_B7
#define GM_JOY_START    GM_JOY_B8
#define GM_JOY_L2       GM_JOY_B9
#define GM_JOY_R2       GM_JOY_B10
#define GM_JOY_L3       GM_JOY_B11
#define GM_JOY_R3       GM_JOY_B12

// CMD_INIT byte[5] capability flags (setInputCaps; sent as a len-6 init).
// Only cores with GROOVY_VERSION >= 2 accept a len-6 CMD_INIT. Older
// cores validate the datagram length and silently discard it (no ACK), so a
// caps client could never connect to them. CmdInit therefore probes the core
// with CMD_GET_VERSION first and falls back to a len-5 init (caps dropped) on
// version < 2; getInputCaps() reports what was actually negotiated.
#define GM_CAP_INPUTS_V2 0x01 // joystick packet v2: 32-bit masks + analog triggers
#define GM_CAP_RUMBLE    0x02 // client may send rumble messages on the inputs socket
// Promises that this client sends keepalives while idle, which is what licenses the core to
// close a silent session. Without it the core leaves a quiet session alone, so a client that
// does not opt in may pause indefinitely. Opting in is a contract: call CmdSendKeepAlive()
// more often than the core's shortest OSD idle timeout (5s today) for as long as you are
// connected but not blitting, pauses and long loads included.
#define GM_CAP_KEEPALIVE 0x04 // client sends CMD_GET_STATUS keepalives while idle

/*! fpgaStatus :
 *  Data received after CmdInit and CmdBlit calls
 */
typedef struct fpgaStatus{
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
} fpgaStatus;

typedef struct fpgaJoyInputs{
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
} fpgaJoyInputs;

typedef struct fpgaPS2Inputs{
	uint32_t ps2Frame;	//ps2 blit frame
	uint8_t  ps2Order;	//ps2 blit order
	uint8_t  ps2Keys[32]; 	//bit array with sdl scancodes convention
	uint8_t  ps2Mouse;	//byte 0 ps2 mouse [yo,xo,ys,xs,1,bm,br,bl]
	uint8_t  ps2MouseX; 	//byte 1 ps2 mouse X
	uint8_t  ps2MouseY; 	//byte 2 ps2 mouse Y
	uint8_t  ps2MouseZ; 	//byte 3 ps2 mouse Z
} fpgaPS2Inputs;

#ifndef _WIN32
typedef unsigned long DWORD;
#endif

// Optional process-wide log sink: route the client's LOG() output through a
// callback instead of stdout (stdout is invisible in Windows GUI apps, hiding
// CmdInit failure reasons). Verbosity still comes from setVerbose(); the sink
// only replaces the destination. Zero cost when unset.
typedef void (*gm_log_sink_fn)(const char* msg);
void gm_set_log_sink(gm_log_sink_fn fn);

class GroovyMister
{
 public:
	 
	fpgaStatus fpga; 	 // Data with last received ACK
	fpgaJoyInputs joyInputs; // Data with last joystick inputs received
	fpgaPS2Inputs ps2Inputs; // Data with last ps2 inputs received

	GroovyMister();
	~GroovyMister();
	
	char* getPBufferBlit(uint8_t field); // This buffer are registered and aligned for sending rgb. Populate it before CmdBlit
	char* getPBufferPreEncoded(void); // Registered compressed-send buffer. For the NLC pre-encode fast path: write an EncodeNLC frame here, then setPreEncodedSize + CmdBlit
	void setPreEncodedSize(uint32_t cSize); // One-shot: the next CmdBlit sends cSize pre-encoded bytes from getPBufferPreEncoded() and SKIPS the software encoder (NLC codec only)
	uint32_t EncodeNLC(const char* rgbFrame, char* out); // Encode one frame with CmdBlit's exact NLC params (call after CmdSwitchres); returns encoded size (0 = failed)
	char* getPBufferBlitDelta(void); // This buffer are registered and aligned for sending rgb. Populate it before CmdBlit with delta difference between actual frame and last
	char* getPBufferAudio(void); // This buffer are registered and aligned for sending audio. Populate it before CmdAudio
	
	// Close connection
	void CmdClose(void);
	// Init streaming with ip, port
	int CmdInit(const char* misterHost, uint16_t misterPort, int lz4Frames, uint32_t soundRate, uint8_t soundChan, uint8_t rgbMode, uint16_t mtu);
	void setNlcDispMode(uint8_t mode);   // NLC display path: 0=streaming, 2=autonomous decode engine
	void setNlcPack(uint8_t pack);       // NLC entropy front-end: 1=TILED (default), 2=RICE (CMD_INIT byte[1] bit 7)
	void setNearLevel(uint8_t lvl);      // NLC near-lossless level 0-3 (0=lossless default; CMD_INIT byte[1] bits [3:2])
	void setInputCaps(uint8_t caps);     // GM_CAP_* input capabilities, set before CmdInit (0 = legacy v1 inputs)
	// Advertise GM_CAP_KEEPALIVE, set before CmdInit. Only enable it if you actually call
	// CmdSendKeepAlive() while idle - see the contract at GM_CAP_KEEPALIVE. Off by default.
	void setKeepAlive(uint8_t on);
	// Change resolution (check https://github.com/antonioginer/switchres) with modeline.
	// Returns 0 on success (ACK'd), -1 if not connected or the ACK never arrived after retrying.
	int CmdSwitchres(double pClock, uint16_t hActive, uint16_t hBegin, uint16_t hEnd, uint16_t hTotal, uint16_t vActive, uint16_t vBegin, uint16_t vEnd, uint16_t vTotal, uint8_t interlace);
	// Stream frame, field = 0 for progressive, vCountSync = 0 for auto frame delay or number of vertical line to sync with, margin with nanoseconds for auto frame delay)
	void CmdBlit(uint32_t frame, uint8_t field, uint16_t vCountSync, uint32_t margin, uint32_t matchDeltaBytes);
	// Stream audio
	void CmdAudio(uint16_t soundSize);
	// getACK is used internal on WaitSync, dwMilliseconds = 0 will time out immediately if no new data
	uint32_t getACK(DWORD dwMilliseconds);
	// sleep to sync with crt raster
	void WaitSync(void);
	// get nanoseconds (positive or negative) to sync with raster
	int DiffTimeRaster(void);

	void BindInputs(const char* misterHost, uint16_t misterPort);
	void PollInputs(void);
	// Rumble the pad assigned to player 0/1 (strong/weak motor 0..255). Requires
	// setInputCaps(GM_CAP_RUMBLE) before CmdInit; the MiSTer gates it per pad in
	// OSD -> System -> Controllers -> <player> -> Rumble (default On). Send on
	// state change only; a value repeats until replaced (or 0/0 to stop).
	// No-op unless inputs are bound AND the live session negotiated GM_CAP_RUMBLE.
	void SendRumble(uint8_t player, uint8_t strong, uint8_t weak);

	// Send only the CMD_CLOSE datagram via plain sendto (no RIO, no teardown).
	// For shutdown paths on threads that no longer drain the RIO completion
	// queues, where an async RIOSend can be silently dropped, leaving the
	// core frozen on the last frame instead of returning to connection-search.
	void CmdSendClose(void);
	// Send a 1-byte CMD_GET_STATUS keepalive on the video socket (normal send
	// path: RIO on Windows). Call at <= (idle timeout)/2 while alive but not
	// blitting (paused / loading / in a menu) to hold the session open against
	// the core's idle timeout (OSD: Server -> Idle timeout). Any datagram resets
	// the core's activity timer; CMD_GET_STATUS just has no side effects. No-op
	// unless a session is live.
	void CmdSendKeepAlive(void);
	// Re-send the 1-byte input subscribe on the existing inputs socket.
	// UDP-loss insurance for threaded clients; no-op before BindInputs.
	void ResendInputSubscribe(void);
	// 1 while a CmdInit-established session is live
	uint8_t isConnected(void);
	// GM_CAP_* actually negotiated for the live session (0 on a v1 session or
	// after the CMD_GET_VERSION probe dropped the caps byte)
	uint8_t getInputCaps(void);
	// Opt-in ACK watchdog (default OFF): after 10 blits with no frameEcho
	// advance, CmdBlit transparently reconnects and replays the stashed modeline.
	// The reconnect is video side only: the inputs socket and its local port survive.
	// CmdInit re-zeroes fpga.* + m_frame on every (re)connect, so a stale
	// session's counter can never leak into the raster servo, and DiffTimeRaster
	// clamps an implausible echo/gpu spread to skip (never hang) the sync. The
	// core restarts its own frame counter on the fresh session, so for full
	// raster-sync recovery an integrator should realign its own blit-frame
	// counter when reconnectEpoch() changes (a divergent counter still runs,
	// just coarser-paced, thanks to the clamp).
	void setAutoReconnect(uint8_t on);
	// Monotonic count of successful auto-reconnects (host observability)
	uint32_t reconnectEpoch(void);

	void setVerbose(uint8_t sev);
	const char* getVersion();
	// Opt-in raw-frame dump for corpus capture (tools/nlc_bench): every CmdBlit's
	// pre-compression buffer -> dir/frame_WxH_fmt_NNNNNN.raw until maxFrames.
	// Also armed by env GM_FRAME_DUMP=<dir> (+ GM_FRAME_DUMP_MAX, default 120).
	void setFrameDump(const char* dir, uint32_t maxFrames);

 private:

	uint8_t m_verbose;
	// frame-dump state (corpus capture; off by default)
	uint8_t  m_dumpFrames;
	uint32_t m_dumpCount;
	uint32_t m_dumpMax;
	char     m_dumpDir[240];
	void DumpFrame(uint8_t field);

#ifdef _WIN32
	SOCKET m_sockFD;
	RIO_EXTENSION_FUNCTION_TABLE m_rio;
	RIO_CQ m_sendQueue;
	RIO_CQ m_receiveQueue;
	RIO_RQ m_requestQueue;
	HANDLE m_hIOCP;
	RIO_BUFFERID m_sendRioBufferId;
	RIO_BUF m_sendRioBuffer;
	RIO_BUFFERID m_receiveRioBufferId;
	RIO_BUF m_receiveRioBuffer;
	RIO_BUFFERID m_sendRioBufferBlitId[2];	
	RIO_BUF *m_pBufsBlit[2];
	RIO_BUFFERID m_sendRioBufferAudioId;
	RIO_BUF m_sendRioBufferAudio;
	RIO_BUF *m_pBufsAudio;
	SOCKET m_sockInputsFD;

	LARGE_INTEGER m_tickStart;
	LARGE_INTEGER m_tickEnd;
	LARGE_INTEGER m_tickSync;
	LARGE_INTEGER m_tickCongestion;
#else
	int m_sockFD;
	int m_sockInputsFD;

	struct timespec m_tickStart;
	struct timespec m_tickEnd;
	struct timespec m_tickSync;
	struct timespec m_tickCongestion;
#endif
	struct sockaddr_in m_serverAddr;
	struct sockaddr_in m_serverAddrInputs;
	char m_bufferSend[26];
	char m_bufferReceive[13];
	char m_bufferInputsReceive[41];
	char *m_pBufferBlit[2];
	char *m_pBufferBlitDelta;
	char *m_pBufferAudio;
	char *m_pBufferLZ4[2];
	uint8_t m_lz4Frames;
	uint8_t m_nlcDispMode;
	uint8_t m_soundChan;
	uint8_t m_rgbMode;
	uint32_t m_RGBSize;
	uint16_t m_nlcWidth;   // active width, stored at CmdSwitchres for nlc_encode (NLC codec, lz4Frames==7)
	uint8_t m_nlcPack;     // 1=TILED (default), 2=RICE. Set before CmdInit
	uint8_t m_nearLevel;   // 0-3 near-lossless quantization. Set before CmdInit
	uint8_t m_inputCaps;   // GM_CAP_* flags sent as CMD_INIT byte[5]. Set before CmdInit
	uint8_t m_keepAlive;   // GM_CAP_KEEPALIVE opt-in, OR'd into the caps byte at CmdInit
	uint32_t m_preEncodedSize; // one-shot pre-encoded payload size for the next CmdBlit (0 = encode normally)
	void buildNlcParams(void* np); // shared CmdBlit/EncodeNLC NLC parameter block (nlc_params*)
	uint8_t  m_interlace;
	uint16_t m_vTotal;
	uint32_t m_frame;
	uint32_t m_frameTime;
	uint32_t m_widthTime;
	uint32_t m_streamTime;
	uint32_t m_emulationTime;
	uint16_t m_mtu;
	uint8_t m_doCongestionControl;
	uint8_t m_core_version;
	uint32_t m_network_ping;
	uint8_t m_delta_enabled[2];
	uint8_t m_isConnected;
	uint8_t m_negotiatedCaps;   // caps granted for the live session (version probe may drop m_inputCaps)
	uint8_t m_videoTorndown;    // run-once guard for teardownVideo(); re-armed by CmdInit
	uint8_t m_autoReconnect;    // opt-in CmdBlit ACK watchdog (default 0)

	// auto-reconnect stash: CmdInit params + last CmdSwitchres modeline so the
	// watchdog can rebuild the session without the caller's involvement.
	// Cleared by the full CmdClose so a deliberate close is never auto-undone.
	char     m_initHost[64];
	uint16_t m_initPort;
	int      m_initLz4Frames;
	uint32_t m_initSoundRate;
	uint8_t  m_initSoundChan;
	uint8_t  m_initRgbMode;
	uint16_t m_initMtu;
	uint8_t  m_switchresValid;
	double   m_initPClock;
	uint16_t m_initHActive;
	uint16_t m_initHBegin;
	uint16_t m_initHEnd;
	uint16_t m_initHTotal;
	uint16_t m_initVActive;
	uint16_t m_initVBegin;
	uint16_t m_initVEnd;
	uint16_t m_initVTotal;
	uint8_t  m_initInterlace;
	uint32_t m_lastFrameEchoSeen;
	uint32_t m_noAckBlitCount;
	uint64_t m_lastReconnectAttemptMs;
	uint32_t m_reconnectEpoch;

	// RIO completion-path telemetry (root-caused a field audio-load stall:
	// the send CQ was never drained, so once it filled RIOSend failed
	// silently). Reset at CmdInit; summarized from WaitSync at LOG level 1.
	uint64_t m_rioSendPosted;        // data RIOSend calls attempted (blit + audio)
	uint64_t m_rioSendFailed;        // ... that returned FALSE
	uint64_t m_rioSendDrained;       // send completions reaped from m_sendQueue
	uint64_t m_rioRecvRepostFailed;  // per-ACK RIOReceive re-post that returned FALSE
	uint64_t m_rioAckTimeout;        // getACK(>0) waits that timed out
	uint64_t m_rioLastSummaryMs;     // rate-limit for the telemetry summary line

	void teardownVideo(void);
	void resetSessionState(void); // zero the per-session raster state (fpga.* + m_frame); constructor + every CmdInit
	uint32_t drainSendCompletions(void);
	void rioServiceQueues(void);  // PCSX2 LOCAL PATCH: drain + telemetry, from WaitSync/CmdBlit/CmdSendKeepAlive
	uint8_t inputsBound(void);
	uint64_t monotonicMs(void);
	char *AllocateBufferSpace(const DWORD bufSize, const DWORD bufCount, DWORD& totalBufferSize, DWORD& totalBufferCount);
	void Send(void *cmd, int cmdSize);
	void SendStream(uint8_t whichBuffer, uint8_t field, uint32_t bytesToSend, uint32_t cSize);
	void setTimeStart(void);
	void setTimeEnd(void);
	uint32_t DiffTime(void);
	void setFpgaStatus(void);
	void setFpgaJoystick(int len);
	void setFpgaPS2(int len);
};

#endif
