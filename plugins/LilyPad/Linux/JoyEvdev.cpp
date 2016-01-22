/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2015  PCSX2 Dev Team/ChickenLiver
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU Lesser General Public License as published by the Free
 *  Software Found- ation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with PCSX2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Global.h"
#include "InputManager.h"
#include "Linux/JoyEvdev.h"
#include "Linux/bitmaskros.h"

JoyEvdev::JoyEvdev(int fd, bool ds3, const wchar_t *id) : Device(LNX_JOY, OTHER, id, id), m_fd(fd) {
	// XXX LNX_JOY => DS3 or ???

	m_abs.clear();
	m_btn.clear();
	m_rel.clear();
	int last = 0;

	uint8_t abs_bitmap[nUcharsForNBits(ABS_CNT)] = {0};
	uint8_t btn_bitmap[nUcharsForNBits(KEY_CNT)] = {0};
	uint8_t rel_bitmap[nUcharsForNBits(REL_CNT)] = {0};

	// Add buttons
	if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(btn_bitmap)), btn_bitmap) >= 0) {
		for (int bit = BTN_MISC; bit < KEY_CNT; bit++) {
			if (testBit(bit, btn_bitmap)) {
				AddPhysicalControl(PSHBTN, last, 0);
				m_btn.push_back(bit);
				last++;
			}
		}
	}

	// Add Absolute axis
	if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bitmap)), abs_bitmap) >= 0) {
		for (int bit = 0; bit < ABS_CNT; bit++) {
			ControlType type = ABSAXIS; // FIXME DS3

			if (testBit(bit, abs_bitmap)) {
				input_absinfo info;
				if (ioctl(m_fd, EVIOCGABS(bit), &info) < 0) {
					fprintf(stderr, "Invalid IOCTL EVIOCGID\n");
					continue;
				}

				AddPhysicalControl(ABSAXIS, last, 0);
				last++;
				if (std::abs(info.value - 127) < 2) {
					fprintf(stderr, "HALF Axis info %d=>%d, current %d, flat %d, resolution %d\n", info.minimum, info.maximum, info.value, info.flat, info.resolution);

					// Half axis must be split into 2 parts...
					AddPhysicalControl(ABSAXIS, last, 0);
					last++;

					m_abs.push_back(abs_info(bit, info.minimum, info.value, type));
					m_abs.push_back(abs_info(bit, info.value, info.maximum, type));
				} else {
					fprintf(stderr, "FULL Axis info %d=>%d, current %d, flat %d, resolution %d\n", info.minimum, info.maximum, info.value, info.flat, info.resolution);

					m_abs.push_back(abs_info(bit, info.minimum, info.maximum, type));
				}
			}
		}
	}

	// Add relative axis
	if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bitmap)), rel_bitmap) >= 0) {
		for (int bit = 0; bit < REL_CNT; bit++) {
			if (testBit(bit, rel_bitmap)) {
				AddPhysicalControl(RELAXIS, last, last);
				m_rel.push_back(bit);
				last++;

				fprintf(stderr, "Add relative nb %d\n", bit);
			}
		}
	}

	fprintf(stderr, "New device created. Found axe:%zu, buttons:%zu, m_rel:%zu\n\n", m_abs.size(), m_btn.size(), m_rel.size());
}

JoyEvdev::~JoyEvdev() {
	close(m_fd);
}

int JoyEvdev::Activate(InitInfo* args) {
	AllocState();

	uint16_t size = m_abs.size()+m_rel.size()+m_btn.size();
	memset(physicalControlState, 0, sizeof(int)*size);

	active = 1;
	return 1;
}

int JoyEvdev::Update() {
    struct input_event events[32];
	int len;
	int status = 0;
	//fprintf(stderr, "Update was called\n");
	
	// Do a big read to reduce kernel validation
    while ((len = read(m_fd, events, (sizeof events))) > 0) {
		int evt_nb = len / sizeof(input_event);
		//fprintf(stderr, "Poll %d events available\n", evt_nb);
		for (int i = 0; i < evt_nb; i++) {
			switch(events[i].type) {
				case EV_ABS:
					{
						for (size_t idx = 0; idx < m_abs.size(); idx++) {
							if (m_abs[idx].code == events[i].code) {
								// XXX strict or not ?
								if ((events[i].value >= m_abs[idx].min) && (events[i].value <= m_abs[idx].max)) {
									// XXX FIX shitty api
									int scale = m_abs[idx].scale(events[i].value);
									fprintf(stderr, "axis value %d scaled to %d\n", events[i].value, scale);
									physicalControlState[idx + m_btn.size()] = scale;
									status = 1;
								}
							}
						}
					}
					break;
				case EV_KEY: 
					{
						for (size_t idx = 0; idx < m_btn.size(); idx++) {
							if (m_btn[idx] == events[i].code) {
								fprintf(stderr, "Event KEY:%d detected with value %d\n", events[i].code, events[i].value);
								physicalControlState[idx] = FULLY_DOWN * events[i].value;
								status = 1;
								break;
							}
						}

					}
					break;
				case EV_REL:
					// XXX
					break;
				default:
					break;
			}
		}

	}

	return status;
}


static std::wstring CorrectJoySupport(int fd) {
	struct input_id id;
	if (ioctl(fd, EVIOCGID, &id) < 0) {
		fprintf(stderr, "Invalid IOCTL EVIOCGID\n");
		return L"";
	}

	char dev_name[128];
	if (ioctl(fd, EVIOCGNAME(128), dev_name) < 0) {
		fprintf(stderr, "Invalid IOCTL EVIOCGNAME\n");
		return L"";
	}

	fprintf(stderr, "Found input device => bustype:%x, vendor:%x, product:%x, version:%x\n", id.bustype, id.vendor, id.product, id.version);
	fprintf(stderr, "\tName:%s\n", dev_name);

	std::string s(dev_name);
	return std::wstring(s.begin(), s.end());
}

void EnumJoystickEvdev() {
	// Technically it must be done with udev but another lib for 
	// avoid a loop is too much for me (even if udev is mandatory
	// so maybe later)
	int found_devices = 0;
	std::string input_root("/dev/input/event");
	for (int i = 0; i < 32; i++) {
		std::string dev = input_root + std::to_string(i);

		int fd = open(dev.c_str(), O_RDWR | O_NONBLOCK);
		if (fd < 0) {
			continue;
		}

		std::wstring id = CorrectJoySupport(fd);
		if (id.size() != 0) {
			bool ds3 = id.find(L"PLAYSTATION(R)3") != std::string::npos;
			if (ds3) {
				fprintf(stderr, "DS3 device detected !!!\n");
			}
			dm->AddDevice(new JoyEvdev(fd, ds3, id.c_str()));
		} else if (fd >= 0)
			close(fd);
	}

}
