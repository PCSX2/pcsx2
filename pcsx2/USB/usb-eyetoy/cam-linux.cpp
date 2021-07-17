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

#include "cam-linux.h"
#include "usb-eyetoy-webcam.h"
#include "jpgd.h"
#include "jpge.h"
#include "jo_mpeg.h"
#include "USB/gtk.h"
#include "common/Console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

namespace usb_eyetoy
{
	namespace linux_api
	{

		static pthread_t eyetoy_thread = 0;
		static unsigned char eyetoy_running = 0;

		static int fd = -1;
		buffer_t* buffers;
		static unsigned int n_buffers;
		static unsigned int pixelformat;

		buffer_t mpeg_buffer;
		std::mutex mpeg_mutex;
		int frame_width;
		int frame_height;
		FrameFormat frame_format;
		bool mirroring_enabled = true;

		static int xioctl(int fh, unsigned long int request, void* arg)
		{
			int r;
			do
			{
				r = ioctl(fh, request, arg);
			} while (-1 == r && EINTR == errno);
			return r;
		}

		static void store_mpeg_frame(const unsigned char* data, const unsigned int len)
		{
			mpeg_mutex.lock();
			memcpy(mpeg_buffer.start, data, len);
			mpeg_buffer.length = len;
			mpeg_mutex.unlock();
		}

		static void process_image(const unsigned char* data, int size)
		{
			const int bytesPerPixel = 3;
			int comprBufSize = frame_width * frame_height * bytesPerPixel;
			if (pixelformat == V4L2_PIX_FMT_YUYV)
			{
				unsigned char* comprBuf = (unsigned char*)calloc(1, comprBufSize);
				int comprLen = 0;
				if (frame_format == format_mpeg)
				{
					comprLen = jo_write_mpeg(comprBuf, data, frame_width, frame_height, JO_YUYV, mirroring_enabled ? JO_FLIP_X : JO_NONE, JO_NONE);
				}
				else if (frame_format == format_jpeg)
				{
					unsigned char* data2 = (unsigned char*)calloc(1, comprBufSize);
					for (int y = 0; y < frame_height; y++)
					{
						for (int x = 0; x < frame_width; x += 2)
						{
							const unsigned char* src = data + (y * frame_width + x) * 2;
							unsigned char* dst = data2 + (y * frame_width + x) * bytesPerPixel;

							int y1 = (int) src[0] << 8;
							int u  = (int) src[1] - 128;
							int y2 = (int) src[2] << 8;
							int v  = (int) src[3] - 128;

							int r  = (y1 + (259 * v) >> 8);
							int g  = (y1 - (88 * u) - (183 * v)) >> 8;
							int b  = (y1 + (454 * u)) >> 8;
							dst[0] = (r > 255) ? 255 : ((r < 0) ? 0 : r);
							dst[1] = (g > 255) ? 255 : ((g < 0) ? 0 : g);
							dst[2] = (b > 255) ? 255 : ((b < 0) ? 0 : b);

							r = (y2 + (259 * v) >> 8);
							g = (y2 - (88 * u) - (183 * v)) >> 8;
							b = (y2 + (454 * u)) >> 8;
							dst[3] = (r > 255) ? 255 : ((r < 0) ? 0 : r);
							dst[4] = (g > 255) ? 255 : ((g < 0) ? 0 : g);
							dst[5] = (b > 255) ? 255 : ((b < 0) ? 0 : b);
						}
					}
					jpge::params params;
					params.m_quality = 80;
					params.m_subsampling = jpge::H2V1;
					comprLen = comprBufSize;
					if (!jpge::compress_image_to_jpeg_file_in_memory(comprBuf, comprLen, frame_width, frame_height, 3, data2, params))
					{
						comprLen = 0;
					}
					free(data2);
				}
				else if (frame_format == format_yuv400)
				{
					int in_pos = 0;
					for (int my = 0; my < 8; my++)
						for (int mx = 0; mx < 10; mx++)
							for (int y = 0; y < 8; y++)
								for (int x = 0; x < 8; x++)
								{
									int srcx = 4* (8*mx + x);
									int srcy = 4* (8*my + y);
									unsigned char* src = (unsigned char*)data + (srcy * frame_width + srcx) * 2/*Y+UV*/;
									if (srcy >= frame_height)
									{
										comprBuf[in_pos++] = 0x01;
									}
									else
									{
										comprBuf[in_pos++] = src[0];//Y
									}
								}
					comprLen = 80 * 64;
				}
				store_mpeg_frame(comprBuf, comprLen);
				free(comprBuf);
			}
			else if (pixelformat == V4L2_PIX_FMT_JPEG)
			{
				if (frame_format == format_mpeg)
				{
					int width, height, actual_comps;
					unsigned char* rgbData = jpgd::decompress_jpeg_image_from_memory(data, size, &width, &height, &actual_comps, 3);
					unsigned char* comprBuf = (unsigned char*)calloc(1, comprBufSize);
					int comprLen = jo_write_mpeg(comprBuf, rgbData, frame_width, frame_height, JO_RGB24, mirroring_enabled ? JO_FLIP_X : JO_NONE, JO_NONE);
					free(rgbData);
					store_mpeg_frame(comprBuf, comprLen);
					free(comprBuf);
				}
				else if (frame_format == format_jpeg)
				{
					store_mpeg_frame(data, size);
				}
				else if (frame_format == format_yuv400)
				{
					int width, height, actual_comps;
					unsigned char* rgbData = jpgd::decompress_jpeg_image_from_memory(data, size, &width, &height, &actual_comps, 3);
					unsigned char* comprBuf = (unsigned char*)calloc(1, comprBufSize);
					int comprLen = 0;
					int in_pos = 0;
					for (int my = 0; my < 8; my++)
						for (int mx = 0; mx < 10; mx++)
							for (int y = 0; y < 8; y++)
								for (int x = 0; x < 8; x++)
								{
									int srcx = 4* (8*mx + x);
									int srcy = 4* (8*my + y);
									unsigned char* src = rgbData + (srcy * frame_width + srcx) * bytesPerPixel;
									if (srcy >= frame_height)
									{
										comprBuf[in_pos++] = 0x01;
									}
									else
									{
										float r = src[0];
										float g = src[1];
										float b = src[2];
										comprBuf[in_pos++] = 0.299f * r + 0.587f * g + 0.114f * b;
									}
								}
					comprLen = 80 * 64;
					free(rgbData);
					store_mpeg_frame(comprBuf, comprLen);
					free(comprBuf);
				}
			}
			else
			{
				Console.Warning("Camera: Unknown format %c%c%c%c", pixelformat, pixelformat >> 8, pixelformat >> 16, pixelformat >> 24);
			}
		}

		static int read_frame()
		{
			struct v4l2_buffer buf;
			CLEAR(buf);
			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_MMAP;

			if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
			{
				switch (errno)
				{
					case EAGAIN:
						return 0;

					case EIO:
					default:
						Console.Warning("Camera: %s error %d, %s", "VIDIOC_DQBUF", errno, strerror(errno));
						return -1;
				}
			}

			assert(buf.index < n_buffers);

			process_image((const unsigned char*)buffers[buf.index].start, buf.bytesused);

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
			{
				Console.Warning("Camera: %s error %d, %s", "VIDIOC_QBUF", errno, strerror(errno));
				return -1;
			}

			return 1;
		}

		std::vector<std::string> getDevList()
		{
			std::vector<std::string> devList;
			char dev_name[64];
			int fd;
			struct v4l2_capability cap;

			for (int index = 0; index < 64; index++)
			{
				snprintf(dev_name, sizeof(dev_name), "/dev/video%d", index);

				if ((fd = open(dev_name, O_RDONLY)) < 0)
				{
					continue;
				}

				if (ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0)
				{
					devList.push_back((char*)cap.card);
				}

				close(fd);
			}
			return devList;
		}

		static int v4l_open(std::string selectedDevice)
		{
			char dev_name[64];
			struct v4l2_capability cap;

			fd = -1;
			for (int index = 0; index < 64; index++)
			{
				snprintf(dev_name, sizeof(dev_name), "/dev/video%d", index);

				if ((fd = open(dev_name, O_RDWR | O_NONBLOCK, 0)) < 0)
				{
					continue;
				}

				CLEAR(cap);
				if (ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0)
				{
					Console.Warning("Camera: %s / %s", dev_name, (char*)cap.card);
					if (!selectedDevice.empty() && strcmp(selectedDevice.c_str(), (char*)cap.card) == 0)
					{
						goto cont;
					}
				}

				close(fd);
				fd = -1;
			}

			if (fd < 0)
			{
				snprintf(dev_name, sizeof(dev_name), "/dev/video0");
				fd = open(dev_name, O_RDWR | O_NONBLOCK, 0);
				if (-1 == fd)
				{
					Console.Warning("Camera: Cannot open '%s': %d, %s", dev_name, errno, strerror(errno));
					return -1;
				}
			}

		cont:

			CLEAR(cap);
			if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap))
			{
				if (EINVAL == errno)
				{
					Console.Warning("Camera: %s is no V4L2 device", dev_name);
					return -1;
				}
				else
				{
					Console.Warning("Camera: %s error %d, %s", "VIDIOC_QUERYCAP", errno, strerror(errno));
					return -1;
				}
			}

			if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
			{
				Console.Warning("Camera: %s is no video capture device", dev_name);
				return -1;
			}

			if (!(cap.capabilities & V4L2_CAP_STREAMING))
			{
				Console.Warning("Camera: %s does not support streaming i/o", dev_name);
				return -1;
			}

			struct v4l2_cropcap cropcap;
			CLEAR(cropcap);
			cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap))
			{
				struct v4l2_crop crop;
				crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				crop.c = cropcap.defrect;

				if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop))
				{
					switch (errno)
					{
						case EINVAL:
							break;
						default:
							break;
					}
				}
			}

			struct v4l2_fmtdesc fmtd;
			CLEAR(fmtd);
			struct v4l2_frmsizeenum frmsize;
			CLEAR(frmsize);

			fmtd.index = 0;
			fmtd.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			while (xioctl(fd, VIDIOC_ENUM_FMT, &fmtd) >= 0)
			{
				frmsize.pixel_format = fmtd.pixelformat;
				frmsize.index = 0;
				while (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0)
				{
					Console.Warning("Camera: supported format[%d] '%s' : %dx%d",
							fmtd.index, fmtd.description,
							frmsize.discrete.width, frmsize.discrete.height);
					frmsize.index++;
				}
				fmtd.index++;
			}

			struct v4l2_format fmt;
			CLEAR(fmt);
			fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			fmt.fmt.pix.width = frame_width;
			fmt.fmt.pix.height = frame_height;
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

			if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
			{
				Console.Warning("Camera: %s error %d, %s", "VIDIOC_S_FMT", errno, strerror(errno));
				return -1;
			}
			pixelformat = fmt.fmt.pix.pixelformat;
			Console.Warning("Camera: selected format: res=%dx%d, fmt=%c%c%c%c", fmt.fmt.pix.width, fmt.fmt.pix.height,
					pixelformat, pixelformat >> 8, pixelformat >> 16, pixelformat >> 24);

			struct v4l2_requestbuffers req;
			CLEAR(req);
			req.count = 4;
			req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			req.memory = V4L2_MEMORY_MMAP;

			if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
			{
				if (EINVAL == errno)
				{
					Console.Warning("Camera: %s does not support memory mapping", dev_name);
					return -1;
				}
				else
				{
					Console.Warning("Camera: %s error %d, %s", "VIDIOC_REQBUFS", errno, strerror(errno));
					return -1;
				}
			}

			if (req.count < 2)
			{
				Console.Warning("Camera: Insufficient buffer memory on %s", dev_name);
				return -1;
			}

			buffers = (buffer_t*)calloc(req.count, sizeof(*buffers));

			if (!buffers)
			{
				Console.Warning("Camera: Out of memory");
				return -1;
			}

			for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
			{
				struct v4l2_buffer buf;

				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = n_buffers;

				if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
				{
					Console.Warning("Camera: %s error %d, %s", "VIDIOC_QUERYBUF", errno, strerror(errno));
					return -1;
				}

				buffers[n_buffers].length = buf.length;
				buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

				if (MAP_FAILED == buffers[n_buffers].start)
				{
					Console.Warning("Camera: %s error %d, %s", "mmap", errno, strerror(errno));
					return -1;
				}
			}

			for (unsigned int i = 0; i < n_buffers; ++i)
			{
				struct v4l2_buffer buf;
				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = i;

				if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
				{
					Console.Warning("Camera: %s error %d, %s", "VIDIOC_QBUF", errno, strerror(errno));
					return -1;
				}
			}

			enum v4l2_buf_type type;
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
			{
				Console.Warning("Camera: %s error %d, %s", "VIDIOC_STREAMON", errno, strerror(errno));
				return -1;
			}
			return 0;
		}


		static void* v4l_thread(void* arg)
		{
			while (eyetoy_running)
			{
				for (;;)
				{
					fd_set fds;

					FD_ZERO(&fds);
					FD_SET(fd, &fds);

					struct timeval timeout = {2, 0}; // 2sec
					int ret = select(fd + 1, &fds, NULL, NULL, &timeout);

					if (ret < 0)
					{
						if (errno == EINTR)
							continue;
						Console.Warning("Camera: %s error %d, %s", "select", errno, strerror(errno));
						break;
					}

					if (ret == 0)
					{
						Console.Warning("Camera: select timeout");
						break;
					}

					if (read_frame())
						break;
				}
			}
			eyetoy_running = 0;
			Console.Warning("Camera: V4L2 thread quit");
			return NULL;
		}

		static int v4l_close()
		{
			enum v4l2_buf_type type;
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
			{
				Console.Warning("Camera: %s error %d, %s", "VIDIOC_STREAMOFF", errno, strerror(errno));
				return -1;
			}

			for (unsigned int i = 0; i < n_buffers; ++i)
			{
				if (-1 == munmap(buffers[i].start, buffers[i].length))
				{
					Console.Warning("Camera: %s error %d, %s", "munmap", errno, strerror(errno));
					return -1;
				}
			}
			free(buffers);

			if (-1 == close(fd))
			{
				Console.Warning("Camera: %s error %d, %s", "close", errno, strerror(errno));
				return -1;
			}
			fd = -1;
			return 0;
		}

		void create_dummy_frame_eyetoy()
		{
			const int bytesPerPixel = 3;
			int comprBufSize = frame_width * frame_height * bytesPerPixel;
			unsigned char* rgbData = (unsigned char*)calloc(1, comprBufSize);
			for (int y = 0; y < frame_height; y++)
			{
				for (int x = 0; x < frame_width; x++)
				{
					unsigned char* ptr = rgbData + (y * frame_width + x) * bytesPerPixel;
					ptr[0] = 255 * y / frame_height;
					ptr[1] = 255 * x / frame_width;
					ptr[2] = 255 * y / frame_height;
				}
			}
			unsigned char* comprBuf = (unsigned char*)calloc(1, comprBufSize);
			int comprLen = 0;
			if (frame_format == format_mpeg)
			{
				comprLen = jo_write_mpeg(comprBuf, rgbData, frame_width, frame_height, JO_RGB24, JO_NONE, JO_NONE);
			}
			else if (frame_format == format_jpeg)
			{
				jpge::params params;
				params.m_quality = 80;
				params.m_subsampling = jpge::H2V1;
				comprLen = comprBufSize;
				if (!jpge::compress_image_to_jpeg_file_in_memory(comprBuf, comprLen, frame_width, frame_height, 3, rgbData, params))
				{
					comprLen = 0;
				}
			}
			free(rgbData);

			store_mpeg_frame(comprBuf, comprLen);
			free(comprBuf);
		}

		void create_dummy_frame_ov511p()
		{
			int comprBufSize = 80 * 64;
			unsigned char* comprBuf = (unsigned char*)calloc(1, comprBufSize);
			if (frame_format == format_yuv400)
			{
				for (int y = 0; y < 64; y++)
				{
					for (int x = 0; x < 80; x++)
					{
						comprBuf[80 * y + x] = 255 * y / 80;
					}
				}
			}
			store_mpeg_frame(comprBuf, comprBufSize);
			free(comprBuf);
		}

		V4L2::V4L2(int port)
		{
			mPort = port;
			mpeg_buffer.start = calloc(1, 640 * 480 * 2);
		}

		V4L2::~V4L2()
		{
			free(mpeg_buffer.start);
			mpeg_buffer.start = nullptr;
		}

		int V4L2::Open(int width, int height, FrameFormat format, int mirror)
		{
			frame_width = width;
			frame_height = height;
			frame_format = format;
			mirroring_enabled = mirror;
			if (format == format_yuv400)
			{
				create_dummy_frame_ov511p();
			}
			else
			{
				create_dummy_frame_eyetoy();
			}

			if (eyetoy_running)
			{
				eyetoy_running = 0;
				pthread_join(eyetoy_thread, NULL);
				v4l_close();
			}
			std::string selectedDevice;
			LoadSetting(EyeToyWebCamDevice::TypeName(), mPort, APINAME, N_DEVICE, selectedDevice);
			if (v4l_open(selectedDevice) != 0)
				return -1;
			pthread_create(&eyetoy_thread, NULL, &v4l_thread, NULL);
			eyetoy_running = 1;
			return 0;
		};

		int V4L2::Close()
		{
			if (eyetoy_running)
			{
				eyetoy_running = 0;
				if (eyetoy_thread)
					pthread_join(eyetoy_thread, NULL);
				eyetoy_thread = 0;
				v4l_close();
			}
			return 0;
		};

		int V4L2::GetImage(uint8_t* buf, size_t len)
		{
			mpeg_mutex.lock();
			int len2 = mpeg_buffer.length;
			if (len < mpeg_buffer.length)
				len2 = len;
			memcpy(buf, mpeg_buffer.start, len2);
			mpeg_buffer.length = 0;
			mpeg_mutex.unlock();
			return len2;
		};

		void V4L2::SetMirroring(bool state)
		{
			mirroring_enabled = state;
		}

		static void deviceChanged(GtkComboBox* widget, gpointer data)
		{
			*(int*)data = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
		}

		int GtkConfigure(int port, const char* dev_type, void* data)
		{
			std::string selectedDevice;
			LoadSetting(dev_type, port, APINAME, N_DEVICE, selectedDevice);

			GtkWidget* dlg = gtk_dialog_new_with_buttons(
				"V4L2 Settings", GTK_WINDOW(data), GTK_DIALOG_MODAL,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
			gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
			gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
			gtk_window_set_default_size(GTK_WINDOW(dlg), 320, 75);

			GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
			GtkWidget* main_hbox = gtk_hbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(dlg_area_box), main_hbox);
			GtkWidget* right_vbox = gtk_vbox_new(FALSE, 5);
			gtk_box_pack_start(GTK_BOX(main_hbox), right_vbox, TRUE, TRUE, 5);

			GtkWidget* rs_cb = new_combobox("Device:", right_vbox);

			std::vector<std::string> devList = getDevList();
			int sel_idx = 0;
			for (uint32_t idx = 0; idx < (uint32_t)devList.size(); idx++)
			{
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(rs_cb), devList.at(idx).c_str());
				if (!selectedDevice.empty() && selectedDevice == devList.at(idx))
				{
					gtk_combo_box_set_active(GTK_COMBO_BOX(rs_cb), idx);
					sel_idx = idx;
				}
			}

			int sel_new;
			g_signal_connect(G_OBJECT(rs_cb), "changed", G_CALLBACK(deviceChanged), (gpointer)&sel_new);

			gtk_widget_show_all(dlg);
			gint result = gtk_dialog_run(GTK_DIALOG(dlg));

			int ret = RESULT_OK;
			if (result == GTK_RESPONSE_OK)
			{
				if (devList.size() && sel_new != sel_idx)
				{
					if (!SaveSetting(dev_type, port, APINAME, N_DEVICE, devList.at(sel_new)))
					{
						ret = RESULT_FAILED;
					}
				}
			}
			else
			{
				ret = RESULT_CANCELED;
			}

			gtk_widget_destroy(dlg);
			return ret;
		}

		int V4L2::Configure(int port, const char* dev_type, void* data)
		{
			return GtkConfigure(port, dev_type, data);
		};

	} // namespace linux_api
} // namespace usb_eyetoy
