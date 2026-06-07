// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "videodev.h"
#include "common/MRCHelpers.h"

#ifdef __OBJC__
#import <AVFoundation/AVFoundation.h>
#endif

@class CameraDelegate;

namespace usb_eyetoy
{
	namespace macos_api
	{
		std::vector<std::pair<std::string, std::string>> getDevList();

		typedef struct _buffer_t
		{
			void* start;
			size_t length;
		} buffer_t;

		class AVFCapture : public VideoDevice
		{
		public:
			AVFCapture();
			~AVFCapture();
			int Open(int width, int height, FrameFormat format, int mirror);
			int Close();
			int GetImage(uint8_t* buf, size_t len);
			void SetMirroring(bool state);
			int Reset() { return 0; }

		private:
			MRCOwned<AVCaptureSession*> captureSession;
			MRCOwned<AVCaptureVideoDataOutput*> captureOutput;
			MRCOwned<AVCaptureDeviceInput*> captureInput;
			MRCOwned<CameraDelegate*> captureDelegate;
			dispatch_queue_t captureQueue = nullptr;

			int m_frame_width = 0;
			int m_frame_height = 0;
			FrameFormat m_frame_format = format_mpeg;
			int m_mirroring_enabled = true;
		};
	} // namespace macos_api
} // namespace usb_eyetoy
