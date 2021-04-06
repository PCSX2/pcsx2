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

#include "PrecompiledHeader.h"
#include "usb-pad.h"
#include "lg/lg_ff.h"

namespace usb_pad
{

	void SetConstantForce(FFDevice* ffdev, int force)
	{
		//parsed_ff_data ff;

		int level = ff_lg_u8_to_s16(force);
		ffdev->SetConstantForce(level);
	}

	void SetSpringForce(FFDevice* ffdev, const spring& force, int caps)
	{
		parsed_ff_data ff;

		ff.u.condition.left_saturation = ff_lg_u8_to_u16(force.clip);
		ff.u.condition.right_saturation = ff_lg_u8_to_u16(force.clip);
		ff.u.condition.left_coeff = ff_lg_get_condition_coef(caps, force.k1, force.s1);
		ff.u.condition.right_coeff = ff_lg_get_condition_coef(caps, force.k2, force.s2);

		if (caps & FF_LG_CAPS_HIGH_RES_DEADBAND)
		{
			uint16_t d2 = ff_lg_get_spring_deadband(caps, force.dead2, (force.s2 >> 1) & 0x7);
			uint16_t d1 = ff_lg_get_spring_deadband(caps, force.dead1, (force.s1 >> 1) & 0x7);
			ff.u.condition.center = ff_lg_u16_to_s16((d1 + d2) / 2);
			ff.u.condition.deadband = d2 - d1;
		}
		else
		{
			ff.u.condition.center = ff_lg_u8_to_s16((force.dead1 + force.dead2) / 2);
			ff.u.condition.deadband = ff_lg_u8_to_u16(force.dead2 - force.dead1);
		}

		ffdev->SetSpringForce(ff);
	}

	void SetDamperForce(FFDevice* ffdev, const damper& force, int caps)
	{
		parsed_ff_data ff;

		ff.u.condition.left_saturation = ff_lg_get_damper_clip(caps, force.clip);
		ff.u.condition.right_saturation = ff_lg_get_damper_clip(caps, force.clip);
		ff.u.condition.left_coeff =
			ff_lg_get_condition_coef(caps, force.k1, force.s1);
		ff.u.condition.right_coeff =
			ff_lg_get_condition_coef(caps, force.k2, force.s2);
		ff.u.condition.center = 0;
		ff.u.condition.deadband = 0;

		ffdev->SetDamperForce(ff);
	}

	void SetFrictionForce(FFDevice* ffdev, const friction& frict)
	{
		parsed_ff_data ff;

		//noideaTM
		ff.u.condition.center = 0;
		ff.u.condition.deadband = 0;
		int s1 = frict.s1 & 1 ? -1 : 1;
		int s2 = frict.s2 & 1 ? -1 : 1;

		ff.u.condition.left_coeff = frict.k1 * 0x7FFF / 255 * s1;
		ff.u.condition.right_coeff = frict.k2 * 0x7FFF / 255 * s2;

		ff.u.condition.left_saturation = 0x7FFF * frict.clip / 255;
		ff.u.condition.right_saturation = ff.u.condition.left_saturation;

		ffdev->SetFrictionForce(ff);
	}

	void SetAutoCenter(FFDevice* ffdev, const autocenter& effect)
	{
		DevCon.WriteLn("%s: k1 %d k2 %d clip %d\n", __func__, effect.k1, effect.k2, effect.clip);
		ffdev->SetAutoCenter((effect.k1 * effect.clip / 255) * 100 / 255); // FIXME
	}

	// Unless passing ff packets straight to a device, parse it here
	void Pad::ParseFFData(const ff_data* ffdata, bool isDFP)
	{
		if (!mFFdev)
			return;

		static int warned = 0;
		int caps = 0;
		DevCon.WriteLn("FFB %02X, %02X, %02X, %02X : %02X, %02X, %02X, %02X",
				   ffdata->cmdslot, ffdata->type, ffdata->u.params[0], ffdata->u.params[1],
				   ffdata->u.params[2], ffdata->u.params[3], ffdata->u.params[4], ffdata->padd0);

		if (ffdata->cmdslot != CMD_EXTENDED_CMD)
		{

			uint8_t slots = (ffdata->cmdslot & 0xF0) >> 4;
			uint8_t cmd = ffdata->cmdslot & 0x0F;

			switch (cmd)
			{
				case CMD_DOWNLOAD:
					for (int i = 0; i < 4; i++)
					{
						if (slots & (1 << i))
							mFFstate.slot_type[i] = ffdata->type;
					}
					break;
				case CMD_DOWNLOAD_AND_PLAY: //0x01
				{
					for (int i = 0; i < 4; i++)
					{
						if (slots & (1 << i))
						{
							mFFstate.slot_type[i] = ffdata->type;
							if (ffdata->type == FTYPE_CONSTANT)
								mFFstate.slot_force[i] = ffdata->u.params[i];
						}
					}

					switch (ffdata->type)
					{
						case FTYPE_CONSTANT:
							if (slots == 0xF)
							{
								int force = 0;
								for (int i = 0; i < 4; i++)
								{
									int t = (int)ffdata->u.params[i];
									if (t < 128)
										t++;
									force = (std::min)((std::max)(force + t - 128, -128), 127);
								}
								SetConstantForce(mFFdev, 128 + force);
							}
							else
							{
								for (int i = 0; i < 4; i++)
								{
									if (slots == (1 << i))
										SetConstantForce(mFFdev, ffdata->u.params[i]);
								}
							}
							break;
						case FTYPE_SPRING:
							SetSpringForce(mFFdev, ffdata->u.spring, isDFP ? 0 : FF_LG_CAPS_OLD_LOW_RES_COEF);
							break;
						case FTYPE_HIGH_RESOLUTION_SPRING:
							SetSpringForce(mFFdev, ffdata->u.spring, FF_LG_CAPS_HIGH_RES_COEF | FF_LG_CAPS_HIGH_RES_DEADBAND);
							break;
						case FTYPE_VARIABLE: //Ramp-like
							//SetRampVariable(mFFdev, ffdata->u.variable);
							//SetConstantForce(mFFdev, ffdata->u.params[0]);
							if (slots & (1 << 0))
							{
								if (ffdata->u.variable.t1 && ffdata->u.variable.s1)
								{
									if (warned == 0)
									{
										DevCon.WriteLn("variable force cannot be converted to constant force (l1=%hhu, t1=%hhu, s1=%hhu, d1=%hhu\n",
												   ffdata->u.variable.l1, ffdata->u.variable.t1, ffdata->u.variable.s1, ffdata->u.variable.d1);
										warned = 1;
									}
								}
								else
								{
									SetConstantForce(mFFdev, ffdata->u.variable.l1);
								}
							}
							else if (slots & (1 << 2))
							{
								if (ffdata->u.variable.t2 && ffdata->u.variable.s2)
								{
									if (warned == 0)
									{
										DevCon.WriteLn("variable force cannot be converted to constant force (l2=%hhu, t2=%hhu, s2=%hhu, d2=%hhu\n",
												   ffdata->u.variable.l2, ffdata->u.variable.t2, ffdata->u.variable.s2, ffdata->u.variable.d2);
										warned = 1;
									}
								}
								else
								{
									SetConstantForce(mFFdev, ffdata->u.variable.l2);
								}
							}
							break;
						case FTYPE_FRICTION:
							SetFrictionForce(mFFdev, ffdata->u.friction);
							break;
						case FTYPE_DAMPER:
							SetDamperForce(mFFdev, ffdata->u.damper, 0);
							break;
						case FTYPE_HIGH_RESOLUTION_DAMPER:
							caps = FF_LG_CAPS_HIGH_RES_COEF;
							if (isDFP)
								caps |= FF_LG_CAPS_DAMPER_CLIP;
							SetDamperForce(mFFdev, ffdata->u.damper, caps);
							break;
						case FTYPE_AUTO_CENTER_SPRING:
							SetAutoCenter(mFFdev, ffdata->u.autocenter);
							break;
						default:
							DevCon.WriteLn("CMD_DOWNLOAD_AND_PLAY: unhandled force type 0x%02X in slots 0x%02X\n", ffdata->type, slots);
							break;
					}
				}
				break;
				case CMD_STOP: //0x03
				{
					for (int i = 0; i < 4; i++)
					{
						if (slots & (1 << i))
						{
							switch (mFFstate.slot_type[i])
							{
								case FTYPE_CONSTANT:
									mFFdev->DisableForce(EFF_CONSTANT);
									break;
								case FTYPE_VARIABLE:
									//mFFdev->DisableRamp();
									mFFdev->DisableForce(EFF_CONSTANT);
									break;
								case FTYPE_SPRING:
								case FTYPE_HIGH_RESOLUTION_SPRING:
									mFFdev->DisableForce(EFF_SPRING);
									break;
								case FTYPE_AUTO_CENTER_SPRING:
									mFFdev->SetAutoCenter(0);
									break;
								case FTYPE_FRICTION:
									mFFdev->DisableForce(EFF_FRICTION);
									break;
								case FTYPE_DAMPER:
								case FTYPE_HIGH_RESOLUTION_DAMPER:
									mFFdev->DisableForce(EFF_DAMPER);
									break;
								default:
									DevCon.WriteLn("CMD_STOP: unhandled force type 0x%02X in slot 0x%02X\n", ffdata->type, slots);
									break;
							}
						}
					}
				}
				break;
				case CMD_DEFAULT_SPRING_ON: //0x04
					DevCon.WriteLn("CMD_DEFAULT_SPRING_ON: unhandled cmd\n");
					break;
				case CMD_DEFAULT_SPRING_OFF: //0x05
				{
					if (slots == 0x0F)
					{
						//just release force
						SetConstantForce(mFFdev, 127);
					}
					else
					{
						DevCon.WriteLn("CMD_DEFAULT_SPRING_OFF: unhandled slots 0x%02X\n", slots);
					}
				}
				break;
				case CMD_NORMAL_MODE: //0x08
					DevCon.WriteLn("CMD_NORMAL_MODE: unhandled cmd\n");
					break;
				case CMD_SET_LED: //0x09
					DevCon.WriteLn("CMD_SET_LED: unhandled cmd\n");
					break;
				case CMD_RAW_MODE: //0x0B
					DevCon.WriteLn("CMD_RAW_MODE: unhandled cmd\n");
					break;
				case CMD_SET_DEFAULT_SPRING: //0x0E
					DevCon.WriteLn("CMD_SET_DEFAULT_SPRING: unhandled cmd\n");
					break;
				case CMD_SET_DEAD_BAND: //0x0F
					DevCon.WriteLn("CMD_SET_DEAD_BAND: unhandled cmd\n");
					break;
			}
		}
		else
		{
			// 0xF8, 0x05, 0x01, 0x00
			//if(ffdata->type == 5) //TODO
			//	sendCrap = true;
			if (ffdata->type == EXT_CMD_WHEEL_RANGE_900_DEGREES)
			{
			}
			if (ffdata->type == EXT_CMD_WHEEL_RANGE_200_DEGREES)
			{
			}
			DevCon.WriteLn("CMD_EXTENDED: unhandled cmd 0x%02X%02X%02X\n",
					   ffdata->type, ffdata->u.params[0], ffdata->u.params[1]);
		}
	}

} // namespace usb_pad
