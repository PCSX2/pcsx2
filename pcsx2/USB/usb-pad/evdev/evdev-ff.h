#ifndef EVDEV_FF_H
#define EVDEV_FF_H

#include <linux/input.h>
#include "../usb-pad.h"

namespace usb_pad { namespace evdev {

class EvdevFF : public FFDevice
{
public:
	EvdevFF(int fd);
	~EvdevFF();

	void SetConstantForce(int level);
	void SetSpringForce(const parsed_ff_data& ff);
	void SetDamperForce(const parsed_ff_data& ff);
	void SetFrictionForce(const parsed_ff_data& ff);
	void SetAutoCenter(int value);
	void SetGain(int gain);
	void DisableForce(EffectID force);

private:
	int mHandle;
	ff_effect mEffect;
	int mEffIds[5] = {-1, -1, -1, -1, -1}; //save ids just in case

	bool mUseRumble;
	int mLastValue;
};

}} //namespace
#endif
