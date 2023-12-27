// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "videodev.h"

namespace usb_eyetoy
{
	namespace linux_api
	{
		std::vector<std::pair<std::string, std::string>> getDevList();

		typedef struct _buffer_t
		{
			void* start;
			size_t length;
		} buffer_t;

		class V4L2 : public VideoDevice
		{
		public:
			V4L2();
			~V4L2();
			int Open(int width, int height, FrameFormat format, int mirror);
			int Close();
			int GetImage(uint8_t* buf, size_t len);
			void SetMirroring(bool state);
			int Reset() { return 0; };
		};
	} // namespace linux_api
} // namespace usb_eyetoy
