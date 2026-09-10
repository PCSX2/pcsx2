/**************************************************************

   groovymister_wrapper.cpp - GroovyMiSTer C wrapper API

   ---------------------------------------------------------

   GroovyMiSTer  noGPU client for Groovy_MiSTer core

 **************************************************************/

#define MODULE_API_EXPORTS_GMW
#include "groovymister.h"
#include "groovymister_wrapper.h"
#ifdef __cplusplus
extern "C" {
#endif


GroovyMister* gmw;
int gmw_inputsBinded;

// The pre-init setters (caps, NLC knobs, auto-reconnect, log level) must work
// before gmw_init/gmw_bindInputs have created the singleton, so it is created
// on demand and call ordering never matters.
static GroovyMister* gmw_instance(void)
{
	if (gmw == NULL)
	{
		gmw = new GroovyMister;
		gmw_inputsBinded = 0;
	}
	return gmw;
}

MODULE_API_GMW int gmw_init(const char* misterHost, uint8_t lz4Frames, uint32_t soundRate, uint8_t soundChan, uint8_t rgbMode, uint16_t mtu)
{
	return gmw_instance()->CmdInit(misterHost, 32100, lz4Frames, soundRate, soundChan, rgbMode, mtu);
}

MODULE_API_GMW void gmw_close(void)
{
	if (gmw != NULL)
	{
		gmw->CmdClose();
		delete gmw;
		gmw = NULL;
		gmw_inputsBinded = 0;
	}
	else
	{
		printf("[MiSTer] gmw_close failed\n");
	}
}

// Send CMD_CLOSE only (no teardown / no delete). Safe to call repeatedly and
// before gmw_close. Lets the socket-owning sender thread tell the MiSTer to
// return to connection-search on stream stop even when the RIO completion
// queues are no longer drained.
MODULE_API_GMW void gmw_send_close(void)
{
	if (gmw != NULL)
	{
		gmw->CmdSendClose();
	}
}

// Send a 1-byte CMD_GET_STATUS keepalive to hold an idle session against the
// core's idle timeout. Call while alive but not blitting (paused/loading/menu).
MODULE_API_GMW void gmw_send_keepalive(void)
{
	if (gmw != NULL)
	{
		gmw->CmdSendKeepAlive();
	}
}

MODULE_API_GMW uint8_t gmw_is_connected(void)
{
	return (gmw != NULL) ? gmw->isConnected() : 0;
}

MODULE_API_GMW uint32_t gmw_reconnect_epoch(void)
{
	return (gmw != NULL) ? gmw->reconnectEpoch() : 0;
}

MODULE_API_GMW int gmw_switchres(double pClock, uint16_t hActive, uint16_t hBegin, uint16_t hEnd, uint16_t hTotal, uint16_t vActive, uint16_t vBegin, uint16_t vEnd, uint16_t vTotal, uint8_t interlace)
{
	if (gmw != NULL)
	{
		return gmw->CmdSwitchres(pClock, hActive, hBegin, hEnd, hTotal, vActive, vBegin, vEnd, vTotal, interlace);
	}
	printf("[MiSTer] gmw_switchres failed\n");
	return -1;
}

MODULE_API_GMW char* gmw_get_pBufferBlit(uint8_t field)
{
	if (gmw != NULL)
	{
		return gmw->getPBufferBlit(field);
	}
	else
	{
		printf("[MiSTer] gmw_get_pBufferBlit failed\n");
		return NULL;
	}
}

MODULE_API_GMW char* gmw_get_pBufferBlitDelta(void)
{
	if (gmw != NULL)
	{
		return gmw->getPBufferBlitDelta();
	}
	else
	{
		printf("[MiSTer] gmw_get_pBufferBlitDelta failed\n");
		return NULL;
	}
}

MODULE_API_GMW void gmw_blit(uint32_t frame, uint8_t field, uint16_t vCountSync, uint32_t margin, uint32_t matchDeltaBytes)
{
	if (gmw != NULL)
	{
		gmw->CmdBlit(frame, field, vCountSync, margin, matchDeltaBytes);
	}
	else
	{
		printf("[MiSTer] gmw_blit failed\n");
	}
}

MODULE_API_GMW char* gmw_get_pBufferAudio(void)
{
	if (gmw != NULL)
	{
		return gmw->getPBufferAudio();
	}
	else
	{
		printf("[MiSTer] gmw_get_pBufferAudio failed\n");
		return NULL;
	}
}

MODULE_API_GMW void gmw_audio(uint16_t soundSize)
{
	if (gmw != NULL)
	{
		gmw->CmdAudio(soundSize);
	}
	else
	{
		printf("[MiSTer] gmw_audio failed\n");
	}
}

MODULE_API_GMW void gmw_waitSync(void)
{
	if (gmw != NULL)
	{
		gmw->WaitSync();
	}
	else
	{
		printf("[MiSTer] gmw_waitSync failed\n");
	}
}


MODULE_API_GMW int gmw_diffTimeRaster(void)
{
	if (gmw != NULL)
	{
		return gmw->DiffTimeRaster();
	}
	else
	{
		printf("[MiSTer] gmw_diffTimeRaster failed\n");
		return 0;
	}
}

MODULE_API_GMW uint32_t gmw_getACK(uint8_t dwMilliseconds)
{
	if (gmw != NULL)
	{
		return gmw->getACK(dwMilliseconds);
	}
	else
	{
		printf("[MiSTer] gmw_getACK failed\n");
		return 0;
	}
}

MODULE_API_GMW void gmw_getStatus(gmw_fpgaStatus* status)
{
	if (gmw != NULL)
	{
		status->frame = gmw->fpga.frame;
		status->frameEcho = gmw->fpga.frameEcho;
		status->vCount = gmw->fpga.vCount;
		status->vCountEcho = gmw->fpga.vCountEcho;

		status->vramEndFrame = gmw->fpga.vramEndFrame;
		status->vramReady = gmw->fpga.vramReady;
		status->vramSynced = gmw->fpga.vramSynced;
		status->vgaFrameskip = gmw->fpga.vgaFrameskip;
		status->vgaVblank = gmw->fpga.vgaVblank;
		status->vgaF1 = gmw->fpga.vgaF1;
		status->audio = gmw->fpga.audio;
		status->vramQueue = gmw->fpga.vramQueue;
	}
	else
	{
		printf("[MiSTer] gmw_getStatus failed\n");
	}
}

MODULE_API_GMW void gmw_bindInputs(const char* misterHost)
{
	gmw_instance();
	if (!gmw_inputsBinded)
	{
		gmw->BindInputs(misterHost, 32101);
	}
	gmw_inputsBinded = 1;
}

// Re-send only the input subscribe datagram (no socket re-create). Safe to
// call repeatedly; only meaningful after gmw_bindInputs.
MODULE_API_GMW void gmw_resubscribe_inputs(void)
{
	if (gmw != NULL)
	{
		gmw->ResendInputSubscribe();
	}
}

MODULE_API_GMW void gmw_set_input_caps(uint8_t caps)
{
	gmw_instance()->setInputCaps(caps);
}

// Pre-CmdInit setter, so it uses gmw_instance() to create the client if needed, matching
// gmw_set_input_caps. Kept separate from the input caps: they are independent opt-ins and
// the client ORs them into the CMD_INIT caps byte itself.
MODULE_API_GMW void gmw_set_keepalive(uint8_t on)
{
	gmw_instance()->setKeepAlive(on);
}

MODULE_API_GMW uint8_t gmw_get_input_caps(void)
{
	return (gmw != NULL) ? gmw->getInputCaps() : 0;
}

// Internally guarded (connected, inputs bound, GMW_CAP_RUMBLE negotiated) and
// deliberately quiet otherwise, since the pad layer may push motor state at any time.
MODULE_API_GMW void gmw_send_rumble(uint8_t player, uint8_t strong, uint8_t weak)
{
	if (gmw != NULL)
	{
		gmw->SendRumble(player, strong, weak);
	}
}

MODULE_API_GMW void gmw_pollInputs(void)
{
	if (gmw != NULL)
	{
		gmw->PollInputs();
	}
	else
	{
		printf("[MiSTer] gmw_pollInputs failed\n");
	}
}

MODULE_API_GMW void gmw_getJoyInputs(gmw_fpgaJoyInputs* joyInputs)
{
	if (gmw != NULL)
	{
		joyInputs->joyFrame     = gmw->joyInputs.joyFrame;
		joyInputs->joyOrder     = gmw->joyInputs.joyOrder;
		joyInputs->joy1         = gmw->joyInputs.joy1;
		joyInputs->joy2         = gmw->joyInputs.joy2;
		joyInputs->joy1LXAnalog = gmw->joyInputs.joy1LXAnalog;
		joyInputs->joy1LYAnalog = gmw->joyInputs.joy1LYAnalog;
		joyInputs->joy1RXAnalog = gmw->joyInputs.joy1RXAnalog;
		joyInputs->joy1RYAnalog = gmw->joyInputs.joy1RYAnalog;
		joyInputs->joy2LXAnalog = gmw->joyInputs.joy2LXAnalog;
		joyInputs->joy2LYAnalog = gmw->joyInputs.joy2LYAnalog;
		joyInputs->joy2RXAnalog = gmw->joyInputs.joy2RXAnalog;
		joyInputs->joy2RYAnalog = gmw->joyInputs.joy2RYAnalog;
		joyInputs->joy1LTAnalog = gmw->joyInputs.joy1LTAnalog;
		joyInputs->joy1RTAnalog = gmw->joyInputs.joy1RTAnalog;
		joyInputs->joy2LTAnalog = gmw->joyInputs.joy2LTAnalog;
		joyInputs->joy2RTAnalog = gmw->joyInputs.joy2RTAnalog;
	}
	else
	{
		memset(joyInputs, 0, sizeof(*joyInputs));
		printf("[MiSTer] gmw_getJoyInputs failed\n");
	}
}

MODULE_API_GMW void gmw_getPS2Inputs(gmw_fpgaPS2Inputs* ps2Inputs)
{
	if (gmw != NULL)
	{
		ps2Inputs->ps2Frame  = gmw->ps2Inputs.ps2Frame;
		ps2Inputs->ps2Order  = gmw->ps2Inputs.ps2Order;
		memcpy(&ps2Inputs->ps2Keys, &gmw->ps2Inputs.ps2Keys, sizeof(ps2Inputs->ps2Keys));
		ps2Inputs->ps2Mouse  = gmw->ps2Inputs.ps2Mouse;
		ps2Inputs->ps2MouseX = gmw->ps2Inputs.ps2MouseX;
		ps2Inputs->ps2MouseY = gmw->ps2Inputs.ps2MouseY;
		ps2Inputs->ps2MouseZ = gmw->ps2Inputs.ps2MouseZ;
	}
	else
	{
		memset(ps2Inputs, 0, sizeof(*ps2Inputs));
		printf("[MiSTer] gmw_getPS2Inputs failed\n");
	}
}

MODULE_API_GMW const char* gmw_get_version()
{
	if (gmw != NULL)
	{
		return gmw->getVersion();
	}
	else
	{
		printf("[MiSTer] gmw_get_version failed\n");
		return NULL;
	}
}

MODULE_API_GMW void gmw_set_log_level(int level)
{
	gmw_instance()->setVerbose(level);
}

// Route LOG() output into the host's logger (process-wide sink); verbose maps
// onto the instance log level so one call configures both.
MODULE_API_GMW void gmw_set_log_callback(void (*fn)(const char* msg), int verbose)
{
	gm_set_log_sink(fn);
	gmw_instance()->setVerbose(verbose);
}

// Codec-corpus capture: dump the next `maxFrames` uncompressed pre-encode
// frames into <dir>/frame_WxH_fmt_NNNNNN.raw, then auto-stop. Off by default.
// Pass dir=NULL or "" to disable. Call from the thread that drives gmw_blit to
// avoid racing DumpFrame's reads of the dump state inside CmdBlit.
MODULE_API_GMW void gmw_set_frame_dump(const char* dir, uint32_t maxFrames)
{
	gmw_instance()->setFrameDump(dir, maxFrames);
}

// NLC codec tuning passthroughs. Both ride the CMD_INIT byte[1] packing, so these
// MUST be called BEFORE gmw_init for the FPGA to see them. Setting them at runtime
// is a no-op until the next CmdInit (an auto-reconnect re-issues CMD_INIT with the
// current values, so the session stays consistent across reconnects).
MODULE_API_GMW void gmw_set_near_level(uint8_t k)
{
	gmw_instance()->setNearLevel(k);
}

MODULE_API_GMW void gmw_set_nlc_pack(uint8_t pack)
{
	gmw_instance()->setNlcPack(pack);
}

MODULE_API_GMW void gmw_set_nlc_disp_mode(uint8_t mode)
{
	gmw_instance()->setNlcDispMode(mode);
}

MODULE_API_GMW void gmw_set_auto_reconnect(uint8_t on)
{
	gmw_instance()->setAutoReconnect(on);
}

// NLC pre-encode fast path. See the ordering contract in groovymister_wrapper.h.
MODULE_API_GMW char* gmw_get_pBufferPreEncoded(void)
{
	if (gmw != NULL)
	{
		return gmw->getPBufferPreEncoded();
	}
	printf("[MiSTer] gmw_get_pBufferPreEncoded failed\n");
	return NULL;
}

MODULE_API_GMW void gmw_set_pre_encoded_size(uint32_t cSize)
{
	if (gmw != NULL)
	{
		gmw->setPreEncodedSize(cSize);
	}
	else
	{
		printf("[MiSTer] gmw_set_pre_encoded_size failed\n");
	}
}

MODULE_API_GMW uint32_t gmw_encode_nlc(const char* rgbFrame, char* out)
{
	if (gmw != NULL)
	{
		return gmw->EncodeNLC(rgbFrame, out);
	}
	printf("[MiSTer] gmw_encode_nlc failed\n");
	return 0;
}


#ifdef __cplusplus
}
#endif
