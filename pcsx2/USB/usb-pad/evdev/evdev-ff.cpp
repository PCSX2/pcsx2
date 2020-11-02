#include "evdev-ff.h"
#include "../../osdebugout.h"
#include "../../usb-pad/lg/lg_ff.h"
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace usb_pad { namespace evdev {

#define BITS_TO_UCHAR(x) \
	(((x) + 8 * sizeof (unsigned char) - 1) / (8 * sizeof (unsigned char)))
#define testBit(bit, array) ( (((uint8_t*)(array))[(bit) / 8] >> ((bit) % 8)) & 1 )

EvdevFF::EvdevFF(int fd): mHandle(fd), mUseRumble(false)
{
	unsigned char features[BITS_TO_UCHAR(FF_MAX)];
	if (ioctl(mHandle, EVIOCGBIT(EV_FF, sizeof(features)), features) < 0) {
		OSDebugOut("Get features failed: %s\n", strerror(errno));
	}

	int effects = 0;
	if (ioctl(mHandle, EVIOCGEFFECTS, &effects) < 0) {
		OSDebugOut("Get effects failed: %s\n", strerror(errno));
	}

	if (!testBit(FF_CONSTANT, features)) {
		OSDebugOut("device does not support FF_CONSTANT\n");
		if (testBit(FF_RUMBLE, features))
			mUseRumble = true;
	}

	if (!testBit(FF_SPRING, features)) {
		OSDebugOut("device does not support FF_SPRING\n");
	}

	if (!testBit(FF_DAMPER, features)) {
		OSDebugOut("device does not support FF_DAMPER\n");
	}

	if (!testBit(FF_GAIN, features)) {
		OSDebugOut("device does not support FF_GAIN\n");
	}

	if (!testBit(FF_AUTOCENTER, features)) {
		OSDebugOut("device does not support FF_AUTOCENTER\n");
	}

	memset(&mEffect, 0, sizeof(mEffect));

	// TODO check features and do FF_RUMBLE instead if gamepad?
	// XXX linux status (hid-lg4ff.c) - only constant and autocenter are implemented
	mEffect.u.constant.level = 0;	/* Strength : 0x2000 == 25 % */
	// Logitech wheels' force vs turn direction: 255 - left, 127/128 - neutral, 0 - right
	// left direction
	mEffect.direction = 0x4000;
	mEffect.u.constant.envelope.attack_length = 0;//0x100;
	mEffect.u.constant.envelope.attack_level = 0;
	mEffect.u.constant.envelope.fade_length = 0;//0x100;
	mEffect.u.constant.envelope.fade_level = 0;
	mEffect.trigger.button = 0;
	mEffect.trigger.interval = 0;
	mEffect.replay.length = 0x7FFFUL;  /* mseconds */
	mEffect.replay.delay = 0;

	SetGain(100);
	SetAutoCenter(0);
}

EvdevFF::~EvdevFF()
{
	for(int i=0; i<countof(mEffIds); i++)
	{
		if (mEffIds[i] != -1 && ioctl(mHandle, EVIOCRMFF, mEffIds[i]) == -1) {
			OSDebugOut("Failed to unload EffectID(%d) effect.\n", i);
		}
	}
}

void EvdevFF::DisableForce(EffectID force)
{
	struct input_event play;
	play.type = EV_FF;
	play.code = mEffIds[force];
	play.value = 0;
	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Stop effect failed: %s\n", strerror(errno));
	}
}

void EvdevFF::SetConstantForce(/*const parsed_ff_data& ff*/ int level)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;
	mEffect.u = {};

	if (!mUseRumble) {
		mEffect.type = FF_CONSTANT;
		mEffect.id = mEffIds[EFF_CONSTANT];
		mEffect.u.constant.level = /*ff.u.constant.*/level;

		OSDebugOut("Constant force: %d\n", level);
		if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
			OSDebugOut("Failed to upload constant effect: %s\n", strerror(errno));
			return;
		}
		play.code = mEffect.id;
		mEffIds[EFF_CONSTANT] = mEffect.id;
	} else {

		mEffect.type = FF_RUMBLE;
		mEffect.id = mEffIds[EFF_RUMBLE];

		mEffect.replay.length = 500;
		mEffect.replay.delay = 0;
		mEffect.u.rumble.weak_magnitude = 0;
		mEffect.u.rumble.strong_magnitude = 0;

		int mag = std::abs(/*ff.u.constant.*/level);
		int diff = std::abs(mag - mLastValue);

		// TODO random limits to cull down on too much rumble
		if (diff > 8292 && diff < 32767)
			mEffect.u.rumble.weak_magnitude = mag;
		if (diff / 8192 > 0)
			mEffect.u.rumble.strong_magnitude = mag;

		mLastValue = mag;

		if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
			OSDebugOut("Failed to upload constant effect: %s\n", strerror(errno));
			return;
		}
		play.code = mEffect.id;
		mEffIds[EFF_RUMBLE] = mEffect.id;
	}

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}

}

void EvdevFF::SetSpringForce(const parsed_ff_data& ff)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	mEffect.type = FF_SPRING;
	mEffect.id = mEffIds[EFF_SPRING];
	mEffect.u = {};
	mEffect.u.condition[0].left_saturation = ff.u.condition.left_saturation;
	mEffect.u.condition[0].right_saturation = ff.u.condition.right_saturation;
	mEffect.u.condition[0].left_coeff = ff.u.condition.left_coeff;
	mEffect.u.condition[0].right_coeff = ff.u.condition.right_coeff;
	mEffect.u.condition[0].center = ff.u.condition.center;
	mEffect.u.condition[0].deadband = ff.u.condition.deadband;

	OSDebugOut("Spring force: coef %d/%d sat %d/%d\n",
		mEffect.u.condition[0].left_coeff, mEffect.u.condition[0].right_coeff,
		mEffect.u.condition[0].left_saturation, mEffect.u.condition[0].right_saturation);

	if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
		OSDebugOut("Failed to upload spring effect: %s\n", strerror(errno));
		return;
	}

	play.code = mEffect.id;
	mEffIds[EFF_SPRING] = mEffect.id;

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}
}

void EvdevFF::SetDamperForce(const parsed_ff_data& ff)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	mEffect.u = {};
	mEffect.type = FF_DAMPER;
	mEffect.id = mEffIds[EFF_DAMPER];
	mEffect.u.condition[0].left_saturation = ff.u.condition.left_saturation;
	mEffect.u.condition[0].right_saturation = ff.u.condition.right_saturation;
	mEffect.u.condition[0].left_coeff = ff.u.condition.left_coeff;
	mEffect.u.condition[0].right_coeff = ff.u.condition.right_coeff;
	mEffect.u.condition[0].center = ff.u.condition.center;
	mEffect.u.condition[0].deadband = ff.u.condition.deadband;

	OSDebugOut("Damper force: %d/%d\n", mEffect.u.condition[0].left_coeff, mEffect.u.condition[0].right_coeff);

	if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
		OSDebugOut("Failed to upload damper effect: %s\n", strerror(errno));
		return;
	}

	play.code = mEffect.id;
	mEffIds[EFF_DAMPER] = mEffect.id;

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}
}

void EvdevFF::SetFrictionForce(const parsed_ff_data& ff)
{
	struct input_event play;
	play.type = EV_FF;
	play.value = 1;

	mEffect.u = {};
	mEffect.type = FF_FRICTION;
	mEffect.id = mEffIds[EFF_FRICTION];
	mEffect.u.condition[0].left_saturation = ff.u.condition.left_saturation;
	mEffect.u.condition[0].right_saturation = ff.u.condition.right_saturation;
	mEffect.u.condition[0].left_coeff = ff.u.condition.left_coeff;
	mEffect.u.condition[0].right_coeff = ff.u.condition.right_coeff;
	mEffect.u.condition[0].center = ff.u.condition.center;
	mEffect.u.condition[0].deadband = ff.u.condition.deadband;

	OSDebugOut("Friction force: %d/%d\n", mEffect.u.condition[0].left_coeff, mEffect.u.condition[0].right_coeff);
	if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0) {
		OSDebugOut("Failed to upload friction effect: %s\n", strerror(errno));
		return;
	}

	play.code = mEffect.id;
	mEffIds[EFF_FRICTION] = mEffect.id;

	if (write(mHandle, (const void*) &play, sizeof(play)) == -1) {
		OSDebugOut("Play effect failed: %s\n", strerror(errno));
	}
}

void EvdevFF::SetAutoCenter(int value)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_AUTOCENTER;
	ie.value = value * 0xFFFFUL / 100;

	OSDebugOut("Autocenter: %d\n", value);
	if (write(mHandle, &ie, sizeof(ie)) == -1)
		OSDebugOut("Failed to set autocenter: %s\n", strerror(errno));
}

void EvdevFF::SetGain(int gain /* between 0 and 100 */)
{
	struct input_event ie;

	ie.type = EV_FF;
	ie.code = FF_GAIN;
	ie.value = 0xFFFFUL * gain / 100;

	OSDebugOut("Gain: %d\n", gain);
	if (write(mHandle, &ie, sizeof(ie)) == -1)
		OSDebugOut("Failed to set gain: %s\n", strerror(errno));
}

}} //namespace
