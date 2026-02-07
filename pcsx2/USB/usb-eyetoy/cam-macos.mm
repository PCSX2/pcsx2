// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Console.h"
#include "videodev.h"
#include "cam-jpeg.h"
#include "cam-macos.h"
#include "usb-eyetoy-webcam.h"
#include "jo_mpeg.h"

#include <thread>

@interface CameraDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
	usb_eyetoy::macos_api::buffer_t _buffer;
	std::mutex _mutex;
	int _frameWidth;
	int _frameHeight;
	usb_eyetoy::FrameFormat _frameFormat;
	bool _mirroringEnabled;
	std::atomic<bool> _active;
}
- (void)setupWithWidth:(int)width height:(int)height format:(usb_eyetoy::FrameFormat)format mirror:(bool)mirror;
- (void)shutdown;
- (int)getImage:(uint8_t*)buf length:(size_t)len;
- (void)storeFrame:(const unsigned char*)data length:(unsigned int)len;
@end

@implementation CameraDelegate

- (instancetype)init
{
	self = [super init];
	if (self)
	{
		_buffer.start = calloc(1, 640 * 480 * 2);
		_buffer.length = 0;
		_frameWidth = 0;
		_frameHeight = 0;
		_frameFormat = usb_eyetoy::format_mpeg;
		_mirroringEnabled = true;
		_active = false;
	}
	return self;
}

- (void)dealloc
{
	free(_buffer.start);
	[super dealloc];
}

- (void)setupWithWidth:(int)width height:(int)height format:(usb_eyetoy::FrameFormat)format mirror:(bool)mirror
{
	_frameWidth = width;
	_frameHeight = height;
	_frameFormat = format;
	_mirroringEnabled = mirror;
	_active = true;
}

- (void)shutdown
{
	_active = false;
}

- (void)storeFrame:(const unsigned char*)data length:(unsigned int)len
{
	if (!_active.load(std::memory_order_relaxed))
		return;
	std::lock_guard lock(_mutex);
	if (len > 0)
		memcpy(_buffer.start, data, len);
	_buffer.length = len;
}

- (int)getImage:(uint8_t*)buf length:(size_t)len
{
	if (!_active.load(std::memory_order_relaxed))
		return 0;
	std::lock_guard lock(_mutex);
	int len2 = static_cast<int>(_buffer.length);
	if (len < _buffer.length)
		len2 = static_cast<int>(len);
	if (len2 > 0)
		memcpy(buf, _buffer.start, static_cast<size_t>(len2));
	_buffer.length = 0;
	return len2;
}

- (void)convertBgraToRgb24Scaled:(const unsigned char*)data srcWidth:(int)srcWidth srcHeight:(int)srcHeight stride:(int)stride rgbBuf:(std::vector<u8>&)rgbBuf
{
	const size_t rgb_size = static_cast<size_t>(_frameWidth) * static_cast<size_t>(_frameHeight) * 3;
	rgbBuf.resize(rgb_size);

	for (int y = 0; y < _frameHeight; y++)
	{
		int src_y = (y * srcHeight) / _frameHeight;
		const unsigned char* src_row = data + static_cast<size_t>(src_y) * static_cast<size_t>(stride);
		unsigned char* dst_row = rgbBuf.data() + static_cast<size_t>(y) * static_cast<size_t>(_frameWidth) * 3;

		for (int x = 0; x < _frameWidth; x++)
		{
			int src_x = (x * srcWidth) / _frameWidth;
			const unsigned char* src = src_row + static_cast<size_t>(src_x) * 4;
			unsigned char* dst = dst_row + static_cast<size_t>(x) * 3;
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
		}
	}
}

- (void)processFrameData:(const unsigned char*)data width:(int)width height:(int)height stride:(int)stride
{
	if (!_active.load(std::memory_order_relaxed))
		return;

	constexpr int bytesPerPixel = 3;
	const size_t comprBufSize = static_cast<size_t>(_frameWidth) * static_cast<size_t>(_frameHeight) * bytesPerPixel;

	std::vector<u8> rgbBuf;
	[self convertBgraToRgb24Scaled:data srcWidth:width srcHeight:height stride:stride rgbBuf:rgbBuf];

	std::vector<u8> comprBuf(comprBufSize);
	if (_frameFormat == usb_eyetoy::format_mpeg)
	{
		const size_t comprLen = jo_write_mpeg(comprBuf.data(), rgbBuf.data(), _frameWidth, _frameHeight,
			JO_RGB24, _mirroringEnabled ? JO_FLIP_X : JO_NONE, JO_NONE);
		comprBuf.resize(comprLen);
	}
	else if (_frameFormat == usb_eyetoy::format_jpeg)
	{
		if (!CompressCamJPEG(&comprBuf, rgbBuf.data(), _frameWidth, _frameHeight, 80))
			comprBuf.clear();
	}
	else if (_frameFormat == usb_eyetoy::format_yuv400)
	{
		comprBuf.resize(80 * 64);
		const int rgb_stride = _frameWidth * bytesPerPixel;
		int in_pos = 0;
		for (int my = 0; my < 8; my++)
		{
			for (int mx = 0; mx < 10; mx++)
			{
				for (int y = 0; y < 8; y++)
				{
					for (int x = 0; x < 8; x++)
					{
						int srcx = 4 * (8 * mx + x);
						int srcy = 4 * (8 * my + y);
						if (srcy >= _frameHeight || srcx >= _frameWidth)
						{
							comprBuf[in_pos++] = 0x01;
							continue;
						}
						const unsigned char* src = rgbBuf.data() + static_cast<size_t>(srcy) * static_cast<size_t>(rgb_stride) +
						                           static_cast<size_t>(srcx) * bytesPerPixel;
						float r = src[0];
						float g = src[1];
						float b = src[2];
						comprBuf[in_pos++] = static_cast<u8>(0.299f * r + 0.587f * g + 0.114f * b);
					}
				}
			}
		}
	}
	else
	{
		comprBuf.clear();
	}

	[self storeFrame:comprBuf.data() length:static_cast<unsigned int>(comprBuf.size())];
}

- (void)captureOutput:(AVCaptureOutput*)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection*)connection
{
	CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
	if (!imageBuffer)
		return;

	CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

	size_t bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);
	void* baseAddress = CVPixelBufferGetBaseAddress(imageBuffer);
	size_t height = CVPixelBufferGetHeight(imageBuffer);
	size_t width = CVPixelBufferGetWidth(imageBuffer);
	size_t size = bytesPerRow * height;

	if (baseAddress && size > 0)
	{
		[self processFrameData:(const unsigned char*)baseAddress
		                 width:(int)width
		                height:(int)height
		                stride:(int)bytesPerRow];
	}

	CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
}

@end

namespace usb_eyetoy
{
	namespace macos_api
	{
		std::vector<std::pair<std::string, std::string>> getDevList()
		{
			std::vector<std::pair<std::string, std::string>> devList;

			@autoreleasepool
			{
				AVCaptureDeviceDiscoverySession* discoverySession = [AVCaptureDeviceDiscoverySession
					discoverySessionWithDeviceTypes:@[ AVCaptureDeviceTypeBuiltInWideAngleCamera, AVCaptureDeviceTypeExternalUnknown ]
					                      mediaType:AVMediaTypeVideo
					                       position:AVCaptureDevicePositionUnspecified];
				NSArray<AVCaptureDevice*>* devices = discoverySession.devices;
				if (devices.count == 0)
					Console.Warning("Camera: You have no video capture hardware");

				for (AVCaptureDevice* device in devices)
				{
					std::string id([device.uniqueID UTF8String]);
					std::string name([device.localizedName UTF8String]);
					devList.emplace_back(id, name);
				}
			}

			return devList;
		}

		AVFCapture::AVFCapture() = default;

		AVFCapture::~AVFCapture()
		{
			Close();
		}

		int AVFCapture::Open(int width, int height, FrameFormat format, int mirror)
		{
			m_frame_width = width;
			m_frame_height = height;
			m_frame_format = format;
			m_mirroring_enabled = mirror;

			@autoreleasepool
			{
				AVCaptureDevice* device = nullptr;

				if (!mHostDevice.empty())
				{
					NSString* deviceID = [NSString stringWithUTF8String:mHostDevice.c_str()];
					device = [AVCaptureDevice deviceWithUniqueID:deviceID];
				}

				if (!device)
				{
					device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
				}

				if (!device)
				{
					Console.Error("Camera: No video capture device found");
					return -1;
				}
				else if (!mHostDevice.empty())
				{
					const char* uniqueId = [device.uniqueID UTF8String];
					if (!uniqueId || mHostDevice != std::string(uniqueId))
					{
						Console.Warning("Camera: cannot find '%s', using '%s'", mHostDevice.c_str(), [device.localizedName UTF8String]);
					}
				}

				NSError* error = nullptr;

				MRCOwned<AVCaptureSession*> session = MRCTransfer([[AVCaptureSession alloc] init]);
				if (!session)
				{
					Console.Error("Camera: Failed to create capture session");
					return -1;
				}

				if ([device lockForConfiguration:&error])
				{
					AVCaptureDeviceFormat* matchingFormat = nil;
					for (AVCaptureDeviceFormat* format in device.formats)
					{
						CMFormatDescriptionRef desc = format.formatDescription;
						CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(desc);
						if (dims.width == m_frame_width && dims.height == m_frame_height)
						{
							matchingFormat = format;
							break;
						}
					}
					if (matchingFormat)
					{
						device.activeFormat = matchingFormat;
					}
					else
					{
						Console.Warning("Camera: requested %dx%d not found, using device default", m_frame_width, m_frame_height);
					}
					[device unlockForConfiguration];
				}
				else
				{
					Console.Warning("Camera: Could not lock device config: %s", error ? [error.localizedDescription UTF8String] : "unknown");
				}

				CMVideoDimensions activeDims = CMVideoFormatDescriptionGetDimensions(device.activeFormat.formatDescription);
				Console.Warning("Camera: selected format: res=%dx%d, fmt=BGRA", activeDims.width, activeDims.height);

				MRCOwned<AVCaptureDeviceInput*> input = MRCTransfer([[AVCaptureDeviceInput alloc] initWithDevice:device error:&error]);
				if (!input || error)
				{
					Console.Error("Camera: Failed to create device input");
					return -1;
				}

				if (![session canAddInput:input])
				{
					Console.Error("Camera: Cannot add input to capture session");
					return -1;
				}

				[session addInput:input];

				MRCOwned<AVCaptureVideoDataOutput*> output = MRCTransfer([[AVCaptureVideoDataOutput alloc] init]);
				if (!output)
				{
					Console.Error("Camera: Failed to create video output");
					return -1;
				}

				NSDictionary* outputSettings = @{
					(NSString*)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)
				};
				[output setVideoSettings:outputSettings];

				MRCOwned<CameraDelegate*> delegate = MRCTransfer([[CameraDelegate alloc] init]);
				if (!delegate)
				{
					Console.Error("Camera: Failed to create camera delegate");
					return -1;
				}

				[delegate setupWithWidth:m_frame_width height:m_frame_height format:m_frame_format mirror:(m_mirroring_enabled != 0)];
				captureQueue = dispatch_queue_create("camera.capture", DISPATCH_QUEUE_SERIAL);
				[output setSampleBufferDelegate:delegate queue:captureQueue];

				if (![session canAddOutput:output])
				{
					Console.Error("Camera: Cannot add output to capture session");
					dispatch_release(captureQueue);
					captureQueue = nullptr;
					return -1;
				}

				[session addOutput:output];

				[session startRunning];

				captureSession = std::move(session);
				captureOutput = std::move(output);
				captureInput = std::move(input);
				captureDelegate = std::move(delegate);

				Console.Warning("Camera: Opened device with resolution %dx%d", m_frame_width, m_frame_height);
				return 0;
			}
		}

		int AVFCapture::Close()
		{
			AVCaptureSession* session = captureSession;
			AVCaptureVideoDataOutput* output = captureOutput;
			AVCaptureDeviceInput* input = captureInput;

			[captureDelegate shutdown];
			[output setSampleBufferDelegate:nil queue:nil];

			// Release the dispatch queue
			if (captureQueue)
			{
				dispatch_release(captureQueue);
				captureQueue = nullptr;
			}

			@autoreleasepool
			{
				// Stop, then remove inputs/outputs, then release references
				if (session)
				{
					if ([session isRunning])
						[session stopRunning];

					if (input)
						[session removeInput:input];
					if (output)
						[session removeOutput:output];
				}

				captureDelegate.Reset();
				captureOutput.Reset();
				captureInput.Reset();
				captureSession.Reset();
			}

			return 0;
		}

		int AVFCapture::GetImage(uint8_t* buf, size_t len)
		{
			return [captureDelegate getImage:buf length:len];
		}

		void AVFCapture::SetMirroring(bool state)
		{
			m_mirroring_enabled = state;
		}
	} // namespace macos_api

	std::vector<std::pair<std::string, std::string>> VideoDevice::GetDeviceList()
	{
		return macos_api::getDevList();
	}

	std::unique_ptr<VideoDevice> VideoDevice::CreateInstance()
	{
		return std::make_unique<macos_api::AVFCapture>();
	}

} // namespace usb_eyetoy
