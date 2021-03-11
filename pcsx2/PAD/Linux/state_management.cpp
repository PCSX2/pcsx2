/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "state_management.h"
#include "Device.h"

// Typical packet response on the bus
static const u8 ConfigExit[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 noclue[7] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x00, 0x5A};
static const u8 setMode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 queryModelDS2[7] = {0x5A, 0x03, 0x02, 0x00, 0x02, 0x01, 0x00};
static const u8 queryModelDS1[7] = {0x5A, 0x01, 0x02, 0x00, 0x02, 0x01, 0x00};
static const u8 queryComb[7] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
static const u8 queryMode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const u8 setNativeMode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A};

static u8 queryMaskMode[7] = {0x5A, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x5A};

static const u8 queryAct[2][7] = {
	{0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
	{0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14}};

QueryInfo query;
Pad pads[2][4];
int slots[2] = {0, 0};

//////////////////////////////////////////////////////////////////////
// QueryInfo implementation
//////////////////////////////////////////////////////////////////////

void QueryInfo::reset()
{
	port = 0;
	slot = 0;
	lastByte = 1;
	currentCommand = 0;
	numBytes = 0;
	queryDone = 1;
	memset(response, 0xF3, sizeof(response));
}

u8 QueryInfo::start_poll(int _port)
{
	if (port > 1)
	{
		reset();
		return 0;
	}

	queryDone = 0;
	port = _port;
	slot = slots[port];
	numBytes = 2;
	lastByte = 0;

	return 0xFF;
}

//////////////////////////////////////////////////////////////////////
// Pad implementation
//////////////////////////////////////////////////////////////////////

void Pad::set_mode(int _mode)
{
	mode = _mode;
}

void Pad::set_vibrate(int motor, u8 val)
{
	nextVibrate[motor] = val;
}

void Pad::reset_vibrate()
{
	set_vibrate(0, 0);
	set_vibrate(1, 0);
	memset(vibrate, 0xFF, sizeof(vibrate));
	vibrate[0] = 0x5A;
}

void Pad::reset()
{
	memset(this, 0, sizeof(PadFreezeData));

	set_mode(MODE_DIGITAL);
	umask[0] = 0xFF;
	umask[1] = 0xFF;
	umask[2] = 0x03;

	// Sets up vibrate variable.
	reset_vibrate();
}

void Pad::rumble(unsigned port)
{
	for (unsigned motor = 0; motor < 2; motor++)
	{
		// TODO:  Probably be better to send all of these at once.
		if (nextVibrate[motor] | currentVibrate[motor])
		{
			currentVibrate[motor] = nextVibrate[motor];

			Device::DoRumble(motor, port);
		}
	}
}

void Pad::stop_vibrate_all()
{
#if 0
	for (int i=0; i<8; i++) {
		SetVibrate(i&1, i>>1, 0, 0);
		SetVibrate(i&1, i>>1, 1, 0);
	}
#endif
	// FIXME equivalent ?
	for (int port = 0; port < 2; port++)
		for (int slot = 0; slot < 4; slot++)
			pads[port][slot].reset_vibrate();
}

void Pad::reset_all()
{
	for (int port = 0; port < 2; port++)
		for (int slot = 0; slot < 4; slot++)
			pads[port][slot].reset();
}

void Pad::rumble_all()
{
	for (unsigned port = 0; port < 2; port++)
		for (unsigned slot = 0; slot < 4; slot++)
			pads[port][slot].rumble(port);
}

//////////////////////////////////////////////////////////////////////
// Pad implementation
//////////////////////////////////////////////////////////////////////

inline bool IsDualshock2()
{
// FIXME
#if 0
	return config.padConfigs[query.port][query.slot].type == Dualshock2Pad ||
			(config.padConfigs[query.port][query.slot].type == GuitarPad && config.GH2);
#else
	return true;
#endif
}

u8 pad_start_poll(u8 pad)
{
	return query.start_poll(pad - 1);
}

u8 pad_poll(u8 value)
{
	if (query.lastByte + 1 >= query.numBytes)
	{
		return 0;
	}
	if (query.lastByte && query.queryDone)
	{
		return query.response[++query.lastByte];
	}

	Pad* pad = &pads[query.port][query.slot];

	if (query.lastByte == 0)
	{
		query.lastByte++;
		query.currentCommand = value;

		switch (value)
		{
			case CMD_CONFIG_MODE:
				if (pad->config)
				{
					// In config mode.  Might not actually be leaving it.
					query.set_result(ConfigExit);
					return 0xF3;
				}
				[[fallthrough]]; // fallthrough on purpose (but I don't know why)

			case CMD_READ_DATA_AND_VIBRATE:
			{
				query.response[2] = 0x5A;
#if 0
				int i;
				Update(query.port, query.slot);
				ButtonSum *sum = &pad->sum;

				u8 b1 = 0xFF, b2 = 0xFF;
				for (i = 0; i<4; i++) {
					b1 -= (sum->buttons[i]   > 0) << i;
				}
				for (i = 0; i<8; i++) {
					b2 -= (sum->buttons[i+4] > 0) << i;
				}
#endif

// FIXME
#if 0
				if (config.padConfigs[query.port][query.slot].type == GuitarPad && !config.GH2) {
					sum->buttons[15] = 255;
					// Not sure about this.  Forces wammy to be from 0 to 0x7F.
					// if (sum->sticks[2].vert > 0) sum->sticks[2].vert = 0;
				}
#endif

#if 0
				for (i = 4; i<8; i++) {
					b1 -= (sum->buttons[i+8] > 0) << i;
				}
#endif

// FIXME
#if 0
				//Left, Right and Down are always pressed on Pop'n Music controller.
				if (config.padConfigs[query.port][query.slot].type == PopnPad)
					b1=b1 & 0x1f;
#endif

				uint16_t buttons = g_key_status.get(query.port);

				query.numBytes = 5;

				query.response[3] = (buttons >> 8) & 0xFF;
				query.response[4] = (buttons >> 0) & 0xFF;

				if (pad->mode != MODE_DIGITAL)
				{ // ANALOG || DS2 native
					query.numBytes = 9;

					query.response[5] = g_key_status.get(query.port, PAD_R_RIGHT);
					query.response[6] = g_key_status.get(query.port, PAD_R_UP);
					query.response[7] = g_key_status.get(query.port, PAD_L_RIGHT);
					query.response[8] = g_key_status.get(query.port, PAD_L_UP);

					if (pad->mode != MODE_ANALOG)
					{ // DS2 native
						query.numBytes = 21;

						query.response[9] = !test_bit(buttons, 13) ? g_key_status.get(query.port, PAD_RIGHT) : 0;
						query.response[10] = !test_bit(buttons, 15) ? g_key_status.get(query.port, PAD_LEFT) : 0;
						query.response[11] = !test_bit(buttons, 12) ? g_key_status.get(query.port, PAD_UP) : 0;
						query.response[12] = !test_bit(buttons, 14) ? g_key_status.get(query.port, PAD_DOWN) : 0;

						query.response[13] = !test_bit(buttons, 4) ? g_key_status.get(query.port, PAD_TRIANGLE) : 0;
						query.response[14] = !test_bit(buttons, 5) ? g_key_status.get(query.port, PAD_CIRCLE) : 0;
						query.response[15] = !test_bit(buttons, 6) ? g_key_status.get(query.port, PAD_CROSS) : 0;
						query.response[16] = !test_bit(buttons, 7) ? g_key_status.get(query.port, PAD_SQUARE) : 0;
						query.response[17] = !test_bit(buttons, 2) ? g_key_status.get(query.port, PAD_L1) : 0;
						query.response[18] = !test_bit(buttons, 3) ? g_key_status.get(query.port, PAD_R1) : 0;
						query.response[19] = !test_bit(buttons, 0) ? g_key_status.get(query.port, PAD_L2) : 0;
						query.response[20] = !test_bit(buttons, 1) ? g_key_status.get(query.port, PAD_R2) : 0;
					}
				}

#if 0
				query.response[3] = b1;
				query.response[4] = b2;

				query.numBytes = 5;
				if (pad->mode != MODE_DIGITAL) {
					query.response[5] = Cap((sum->sticks[0].horiz+255)/2);
					query.response[6] = Cap((sum->sticks[0].vert+255)/2);
					query.response[7] = Cap((sum->sticks[1].horiz+255)/2);
					query.response[8] = Cap((sum->sticks[1].vert+255)/2);

					query.numBytes = 9;
					if (pad->mode != MODE_ANALOG) {
						// Good idea?  No clue.
						//query.response[3] &= pad->mask[0];
						//query.response[4] &= pad->mask[1];

						// No need to cap these, already done int CapSum().
						query.response[9] = (unsigned char)sum->buttons[13]; //D-pad right
						query.response[10] = (unsigned char)sum->buttons[15]; //D-pad left
						query.response[11] = (unsigned char)sum->buttons[12]; //D-pad up
						query.response[12] = (unsigned char)sum->buttons[14]; //D-pad down

						query.response[13] = (unsigned char) sum->buttons[8];
						query.response[14] = (unsigned char) sum->buttons[9];
						query.response[15] = (unsigned char) sum->buttons[10];
						query.response[16] = (unsigned char) sum->buttons[11];
						query.response[17] = (unsigned char) sum->buttons[6];
						query.response[18] = (unsigned char) sum->buttons[7];
						query.response[19] = (unsigned char) sum->buttons[4];
						query.response[20] = (unsigned char) sum->buttons[5];
						query.numBytes = 21;
					}
				}
#endif
			}

				query.lastByte = 1;
				return pad->mode;

			case CMD_SET_VREF_PARAM:
				query.set_final_result(noclue);
				break;

			case CMD_QUERY_DS2_ANALOG_MODE:
				// Right?  Wrong?  No clue.
				if (pad->mode == MODE_DIGITAL)
				{
					queryMaskMode[1] = queryMaskMode[2] = queryMaskMode[3] = 0;
					queryMaskMode[6] = 0x00;
				}
				else
				{
					queryMaskMode[1] = pad->umask[0];
					queryMaskMode[2] = pad->umask[1];
					queryMaskMode[3] = pad->umask[2];
					// Not entirely sure about this.
					//queryMaskMode[3] = 0x01 | (pad->mode == MODE_DS2_NATIVE)*2;
					queryMaskMode[6] = 0x5A;
				}
				query.set_final_result(queryMaskMode);
				break;

			case CMD_SET_MODE_AND_LOCK:
				query.set_result(setMode);
				pad->reset_vibrate();
				break;

			case CMD_QUERY_MODEL_AND_MODE:
				if (IsDualshock2())
				{
					query.set_final_result(queryModelDS2);
				}
				else
				{
					query.set_final_result(queryModelDS1);
				}
				// Not digital mode.
				query.response[5] = (pad->mode & 0xF) != 1;
				break;

			case CMD_QUERY_ACT:
				query.set_result(queryAct[0]);
				break;

			case CMD_QUERY_COMB:
				query.set_final_result(queryComb);
				break;

			case CMD_QUERY_MODE:
				query.set_result(queryMode);
				break;

			case CMD_VIBRATION_TOGGLE:
				memcpy(query.response + 2, pad->vibrate, 7);
				query.numBytes = 9;
				//query.set_result(pad->vibrate); // warning copy 7b not 8 (but it is really important?)
				pad->reset_vibrate();
				break;

			case CMD_SET_DS2_NATIVE_MODE:
				if (IsDualshock2())
				{
					query.set_result(setNativeMode);
				}
				else
				{
					query.set_final_result(setNativeMode);
				}
				break;

			default:
				query.numBytes = 0;
				query.queryDone = 1;
				break;
		}

		return 0xF3;
	}
	else
	{
		query.lastByte++;

		switch (query.currentCommand)
		{
			case CMD_READ_DATA_AND_VIBRATE:
				if (query.lastByte == pad->vibrateI[0])
					pad->set_vibrate(1, 255 * (value & 1));
				else if (query.lastByte == pad->vibrateI[1])
					pad->set_vibrate(0, value);

				break;

			case CMD_CONFIG_MODE:
				if (query.lastByte == 3)
				{
					query.queryDone = 1;
					pad->config = value;
				}
				break;

			case CMD_SET_MODE_AND_LOCK:
				if (query.lastByte == 3 && value < 2)
				{
					pad->set_mode(value ? MODE_ANALOG : MODE_DIGITAL);
				}
				else if (query.lastByte == 4)
				{
					if (value == 3)
						pad->modeLock = 3;
					else
						pad->modeLock = 0;

					query.queryDone = 1;
				}
				break;

			case CMD_QUERY_ACT:
				if (query.lastByte == 3)
				{
					if (value < 2)
						query.set_result(queryAct[value]);
					// bunch of 0's
					// else query.set_result(setMode);
					query.queryDone = 1;
				}
				break;

			case CMD_QUERY_MODE:
				if (query.lastByte == 3 && value < 2)
				{
					query.response[6] = 4 + value * 3;
					query.queryDone = 1;
				}
				// bunch of 0's
				//else data = setMode;
				break;

			case CMD_VIBRATION_TOGGLE:
				if (query.lastByte >= 3)
				{
					if (value == 0)
					{
						pad->vibrateI[0] = (u8)query.lastByte;
					}
					else if (value == 1)
					{
						pad->vibrateI[1] = (u8)query.lastByte;
					}
					pad->vibrate[query.lastByte - 2] = value;
				}
				break;

			case CMD_SET_DS2_NATIVE_MODE:
				if (query.lastByte > 2 && query.lastByte < 6)
				{
					pad->umask[query.lastByte - 3] = value;
				}
				pad->set_mode(MODE_DS2_NATIVE);
				break;

			default:
				return 0;
		}

		return query.response[query.lastByte];
	}
}
