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

#include "USB/gtk.h"
#include "audiodev-pulse.h"
#ifdef DYNLINK_PULSE
#include "USB/dynlink/pulse.h"
#endif

namespace usb_mic
{
	namespace audiodev_pulse
	{

		static void pa_context_state_cb(pa_context* c, void* userdata)
		{
			pa_context_state_t state;
			int* pa_ready = (int*)userdata;

			state = pa_context_get_state(c);
			switch (state)
			{
				// There are just here for reference
				case PA_CONTEXT_UNCONNECTED:
					*pa_ready = 3;
					break;
				case PA_CONTEXT_CONNECTING:
				case PA_CONTEXT_AUTHORIZING:
				case PA_CONTEXT_SETTING_NAME:
				default:
					break;
				case PA_CONTEXT_FAILED:
				case PA_CONTEXT_TERMINATED:
					*pa_ready = 2;
					break;
				case PA_CONTEXT_READY:
					*pa_ready = 1;
					break;
			}
		}

		static void pa_sourcelist_cb(pa_context* c, const pa_source_info* l, int eol, void* userdata)
		{
			AudioDeviceInfoList* devicelist = static_cast<AudioDeviceInfoList*>(userdata);

			if (eol > 0)
			{
				return;
			}

			AudioDeviceInfo dev;
			dev.strID = l->name;
			dev.strName = l->description;
			//dev.intID = l->index;
			devicelist->push_back(dev);
		}

		static void pa_sinklist_cb(pa_context* c, const pa_sink_info* l, int eol, void* userdata)
		{
			AudioDeviceInfoList* devicelist = static_cast<AudioDeviceInfoList*>(userdata);

			if (eol > 0)
			{
				return;
			}

			AudioDeviceInfo dev;
			dev.strID = l->name;
			dev.strName = l->description;
			//dev.intID = l->index;
			devicelist->push_back(dev);
		}

		static int pa_get_devicelist(AudioDeviceInfoList& list, AudioDir dir)
		{
			pa_mainloop* pa_ml;
			pa_mainloop_api* pa_mlapi;
			pa_operation* pa_op;
			pa_context* pa_ctx;

			int state = 0;
			int pa_ready = 0;

			pa_ml = pa_mainloop_new();
			pa_mlapi = pa_mainloop_get_api(pa_ml);
			pa_ctx = pa_context_new(pa_mlapi, "USB-devicelist");

			pa_context_connect(pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL);

			pa_context_set_state_callback(pa_ctx, pa_context_state_cb, &pa_ready);

			for (;;)
			{

				if (pa_ready == 0)
				{
					pa_mainloop_iterate(pa_ml, 1, NULL);
					continue;
				}

				// Connection failed
				if (pa_ready == 2)
				{
					pa_context_disconnect(pa_ctx);
					pa_context_unref(pa_ctx);
					pa_mainloop_free(pa_ml);
					return -1;
				}

				switch (state)
				{
					case 0:
						if (dir == AUDIODIR_SOURCE)
							pa_op = pa_context_get_source_info_list(pa_ctx,
																	pa_sourcelist_cb,
																	&list);
						else
							pa_op = pa_context_get_sink_info_list(pa_ctx,
																  pa_sinklist_cb,
																  &list);
						state++;
						break;
					case 1:
						if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE)
						{
							pa_operation_unref(pa_op);
							pa_context_disconnect(pa_ctx);
							pa_context_unref(pa_ctx);
							pa_mainloop_free(pa_ml);
							return 0;
						}
						break;
					default:
						return -1;
				}
				pa_mainloop_iterate(pa_ml, 1, NULL);
			}
		}

		// GTK+ config. dialog stuff
		static void populateDeviceWidget(GtkComboBox* widget, const std::string& devName, const AudioDeviceInfoList& devs)
		{
			gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(widget)));
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "None");
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);

			int i = 1;
			for (auto& dev : devs)
			{
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), dev.strName.c_str());
				if (!devName.empty() && devName == dev.strID)
					gtk_combo_box_set_active(GTK_COMBO_BOX(widget), i);
				i++;
			}
		}

		static void deviceChanged(GtkComboBox* widget, gpointer data)
		{
			*(int*)data = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
		}

		static int GtkConfigure(int port, const char* dev_type, void* data)
		{
			GtkWidget* ro_frame;

			int dev_idxs[] = {0, 0, 0, 0};

			AudioDeviceInfoList srcDevs;
			if (pa_get_devicelist(srcDevs, AUDIODIR_SOURCE) != 0)
			{
				return RESULT_FAILED;
			}

			AudioDeviceInfoList sinkDevs;
			if (pa_get_devicelist(sinkDevs, AUDIODIR_SINK) != 0)
			{
				return RESULT_FAILED;
			}

			GtkWidget* dlg = gtk_dialog_new_with_buttons(
				"PulseAudio Settings", GTK_WINDOW(data), GTK_DIALOG_MODAL,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
			gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
			gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);
			GtkWidget* dlg_area_box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));


			GtkWidget* main_vbox = gtk_vbox_new(FALSE, 5);
			gtk_box_pack_start(GTK_BOX(dlg_area_box), main_vbox, TRUE, FALSE, 5);

			ro_frame = gtk_frame_new("Audio Devices");
			gtk_box_pack_start(GTK_BOX(main_vbox), ro_frame, TRUE, FALSE, 5);

			GtkWidget* frame_vbox = gtk_vbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(ro_frame), frame_vbox);

			const char* labels[] = {"Source 1", "Source 2", "Sink 1", "Sink 2"};
			for (int i = 0; i < 2; i++)
			{
				std::string devName;
				LoadSetting(dev_type, port, APINAME, (i ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0), devName);

				GtkWidget* cb = new_combobox(labels[i], frame_vbox);
				g_signal_connect(G_OBJECT(cb), "changed", G_CALLBACK(deviceChanged), (gpointer)&dev_idxs[i]);
				populateDeviceWidget(GTK_COMBO_BOX(cb), devName, srcDevs);
			}

			//TODO only one for now
			for (int i = 2; i < 3 /*4*/; i++)
			{
				std::string devName;
				LoadSetting(dev_type, port, APINAME, (i - 2 ? N_AUDIO_SINK1 : N_AUDIO_SINK0), devName);

				GtkWidget* cb = new_combobox(labels[i], frame_vbox);
				g_signal_connect(G_OBJECT(cb), "changed", G_CALLBACK(deviceChanged), (gpointer)&dev_idxs[i]);
				populateDeviceWidget(GTK_COMBO_BOX(cb), devName, sinkDevs);
			}

			ro_frame = gtk_frame_new("Buffer lengths");
			gtk_box_pack_start(GTK_BOX(main_vbox), ro_frame, TRUE, FALSE, 5);

			frame_vbox = gtk_vbox_new(FALSE, 5);
			gtk_container_add(GTK_CONTAINER(ro_frame), frame_vbox);

			const char* labels_buff[] = {"Sources", "Sinks"};
			const char* buff_var_name[] = {N_BUFFER_LEN_SRC, N_BUFFER_LEN_SINK};
			GtkWidget* scales[2];

			GtkWidget* table = gtk_table_new(2, 2, true);
			gtk_container_add(GTK_CONTAINER(frame_vbox), table);
			gtk_table_set_homogeneous(GTK_TABLE(table), FALSE);
			GtkAttachOptions opt = (GtkAttachOptions)(GTK_EXPAND | GTK_FILL); // default

			for (int i = 0; i < 2; i++)
			{
				GtkWidget* label = gtk_label_new(labels_buff[i]);
				gtk_table_attach(GTK_TABLE(table), label,
								 0, 1,
								 0 + i, 1 + i,
								 GTK_SHRINK, GTK_SHRINK, 5, 1);

				//scales[i] = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 1, 1000, 1);
				scales[i] = gtk_hscale_new_with_range(1, 1000, 1);
				gtk_table_attach(GTK_TABLE(table), scales[i],
								 1, 2,
								 0 + i, 1 + i,
								 opt, opt, 5, 1);

				int32_t var;
				if (LoadSetting(dev_type, port, APINAME, buff_var_name[i], var))
					gtk_range_set_value(GTK_RANGE(scales[i]), var);
				else
					gtk_range_set_value(GTK_RANGE(scales[i]), 50);
			}

			gtk_widget_show_all(dlg);
			gint result = gtk_dialog_run(GTK_DIALOG(dlg));

			int scale_vals[2];
			for (int i = 0; i < 2; i++)
			{
				scale_vals[i] = gtk_range_get_value(GTK_RANGE(scales[i]));
			}

			gtk_widget_destroy(dlg);

			// Wait for all gtk events to be consumed ...
			while (gtk_events_pending())
				gtk_main_iteration_do(FALSE);

			int ret = RESULT_CANCELED;
			if (result == GTK_RESPONSE_OK)
			{
				ret = RESULT_OK;
				for (int i = 0; i < 2; i++)
				{
					int idx = dev_idxs[i];
					{
						std::string var;

						if (idx > 0)
							var = srcDevs[idx - 1].strID;

						if (!SaveSetting(dev_type, port, APINAME, (i ? N_AUDIO_SOURCE1 : N_AUDIO_SOURCE0), var))
							ret = RESULT_FAILED;
					}

					idx = dev_idxs[i + 2];
					{
						std::string var;

						if (idx > 0)
							var = sinkDevs[idx - 1].strID;

						if (!SaveSetting(dev_type, port, APINAME, (i ? N_AUDIO_SINK1 : N_AUDIO_SINK0), var))
							ret = RESULT_FAILED;
					}

					// Save buffer lengths
					if (!SaveSetting(dev_type, port, APINAME, buff_var_name[i], scale_vals[i]))
						ret = RESULT_FAILED;
				}
			}

			return ret;
		}

		uint32_t PulseAudioDevice::GetBuffer(short* buff, uint32_t frames)
		{
			auto now = hrc::now();
			auto dur = std::chrono::duration_cast<ms>(now - mLastGetBuffer).count();

			// init time point
			if (mLastOut.time_since_epoch().count() == 0)
				mLastOut = now;

			//Disconnected, try reconnect after every 1sec, hopefully game retries to read samples
			if (mPAready == 3 && dur >= 1000)
			{
				mLastGetBuffer = now;
				[[maybe_unused]]
				int ret = pa_context_connect(mPContext,
											 mServer,
											 PA_CONTEXT_NOFLAGS,
											 NULL);

				//TODO reconnect stream as well?

			}
			else
				mLastGetBuffer = now;

			std::lock_guard<std::mutex> lk(mMutex);
			ssize_t samples_to_read = frames * GetChannels();
			short* pDst = (short*)buff;
			assert(samples_to_read <= mOutBuffer.size<short>());

			while (samples_to_read > 0)
			{
				ssize_t samples = std::min(samples_to_read, (ssize_t)mOutBuffer.peek_read<short>());
				if (!samples)
					break;
				memcpy(pDst, mOutBuffer.front(), samples * sizeof(short));
				mOutBuffer.read<short>(samples);
				pDst += samples;
				samples_to_read -= samples;
			}
			return (frames - (samples_to_read / GetChannels()));
		}

		uint32_t PulseAudioDevice::SetBuffer(short* buff, uint32_t frames)
		{
			auto now = hrc::now();
			auto dur = std::chrono::duration_cast<ms>(now - mLastGetBuffer).count();

			// init time point
			if (mLastOut.time_since_epoch().count() == 0)
				mLastOut = now;

			//Disconnected, try reconnect after every 1sec
			if (mPAready == 3 && dur >= 1000)
			{
				mLastGetBuffer = now;
				int ret = pa_context_connect(mPContext,
											 mServer,
											 PA_CONTEXT_NOFLAGS,
											 NULL);

				//TODO reconnect stream as well?

				if (ret != PA_OK)
					return frames;
			}
			else
				mLastGetBuffer = now;

			std::lock_guard<std::mutex> lk(mMutex);
			size_t nbytes = frames * sizeof(short) * GetChannels();
			mInBuffer.write((uint8_t*)buff, nbytes);

			return frames;
		}

		bool PulseAudioDevice::GetFrames(uint32_t* size)
		{
			std::lock_guard<std::mutex> lk(mMutex);
			*size = mOutBuffer.size<short>() / GetChannels();
			return true;
		}

		void PulseAudioDevice::SetResampling(int samplerate)
		{
			mSamplesPerSec = samplerate;
			if (mAudioDir == AUDIODIR_SOURCE)
				mResampleRatio = double(samplerate) / double(mSSpec.rate);
			else
				mResampleRatio = double(mSSpec.rate) / double(samplerate);
			//mResample = true;
			ResetBuffers();
		}

		void PulseAudioDevice::Start()
		{
			ResetBuffers();
			mPaused = false;
			if (mStream)
			{
				pa_threaded_mainloop_lock(mPMainLoop);
				if (pa_stream_is_corked(mStream) > 0)
				{
					pa_operation* op = pa_stream_cork(mStream, 0, stream_success_cb, this);
					if (op)
						pa_operation_unref(op);
				}
				pa_threaded_mainloop_unlock(mPMainLoop);
			}
		}

		void PulseAudioDevice::Stop()
		{
			mPaused = true;
			if (mStream)
			{
				pa_threaded_mainloop_lock(mPMainLoop);
				if (!pa_stream_is_corked(mStream))
				{
					pa_operation* op = pa_stream_cork(mStream, 1, stream_success_cb, this);
					if (op)
						pa_operation_unref(op);
				}
				pa_threaded_mainloop_unlock(mPMainLoop);
			}
		}

		bool PulseAudioDevice::Compare(AudioDevice* compare)
		{
			if (compare)
			{
				PulseAudioDevice* src = static_cast<PulseAudioDevice*>(compare);
				if (src && mDeviceName == src->mDeviceName)
					return true;
			}
			return false;
		}

		void PulseAudioDevice::Uninit()
		{
			if (mStream)
			{
				pa_threaded_mainloop_lock(mPMainLoop);
				[[maybe_unused]]int ret = pa_stream_disconnect(mStream);
				pa_stream_unref(mStream);
				mStream = nullptr;
				pa_threaded_mainloop_unlock(mPMainLoop);
			}
			if (mPMainLoop)
			{
				pa_threaded_mainloop_stop(mPMainLoop);
			}
			if (mPContext)
			{
				pa_context_disconnect(mPContext);
				pa_context_unref(mPContext);
				mPContext = nullptr;
			}
			if (mPMainLoop)
			{
				pa_threaded_mainloop_free(mPMainLoop);
				mPMainLoop = nullptr;
			}
		}

		bool PulseAudioDevice::Init()
		{
			int ret = 0;
			pa_operation* pa_op = nullptr;

			mPMainLoop = pa_threaded_mainloop_new();
			pa_mainloop_api* mlapi = pa_threaded_mainloop_get_api(mPMainLoop);

			mPContext = pa_context_new(mlapi, "USB");

			pa_context_set_state_callback(mPContext,
										  context_state_cb,
										  this);

			// Lock the mainloop so that it does not run and crash before the context is ready
			pa_threaded_mainloop_lock(mPMainLoop);
			pa_threaded_mainloop_start(mPMainLoop);

			ret = pa_context_connect(mPContext,
									 mServer,
									 PA_CONTEXT_NOFLAGS,
									 NULL);

			if (ret != PA_OK)
				goto unlock_and_fail;

			// wait for pa_context_state_cb
			for (;;)
			{
				if (mPAready == 1)
					break;
				if (mPAready == 2 || mQuit)
					goto unlock_and_fail;
				pa_threaded_mainloop_wait(mPMainLoop);
			}

			mStream = pa_stream_new(mPContext,
									"USB-pulse",
									&mSSpec,
									NULL);

			if (!mStream)
				goto unlock_and_fail;

			pa_stream_set_state_callback(mStream, stream_state_cb, this);

			// Sets individual read callback fragsize but recording itself
			// still "lags" ~1sec (read_cb is called in bursts) without
			// PA_STREAM_ADJUST_LATENCY
			pa_buffer_attr buffer_attr;
			buffer_attr.maxlength = (uint32_t)-1;
			buffer_attr.tlength = (uint32_t)-1;
			buffer_attr.prebuf = (uint32_t)-1;
			buffer_attr.minreq = (uint32_t)-1;
			buffer_attr.fragsize = pa_usec_to_bytes(mBuffering * 1000, &mSSpec);

			if (mAudioDir == AUDIODIR_SOURCE)
			{
				pa_stream_set_read_callback(mStream,
											stream_read_cb,
											this);

				ret = pa_stream_connect_record(mStream,
											   mDeviceName.c_str(),
											   &buffer_attr,
											   PA_STREAM_ADJUST_LATENCY);
			}
			else
			{
				pa_stream_set_write_callback(mStream,
											 stream_write_cb,
											 this);

				buffer_attr.maxlength = pa_bytes_per_second(&mSSpec);
				buffer_attr.prebuf = 0; // Don't stop on underrun but then
										// stream also only starts manually with uncorking.
				buffer_attr.tlength = pa_usec_to_bytes(mBuffering * 1000, &mSSpec);
				pa_stream_flags_t flags = (pa_stream_flags_t)(PA_STREAM_INTERPOLATE_TIMING |
															  PA_STREAM_NOT_MONOTONIC |
															  PA_STREAM_AUTO_TIMING_UPDATE |
															  //PA_STREAM_VARIABLE_RATE |
															  PA_STREAM_ADJUST_LATENCY);

				ret = pa_stream_connect_playback(mStream,
												 mDeviceName.c_str(),
												 &buffer_attr,
												 flags,
												 nullptr,
												 nullptr);
			}

			if (ret != PA_OK)
				goto unlock_and_fail;

			// Wait for the stream to be ready
			for (;;)
			{
				pa_stream_state_t stream_state = pa_stream_get_state(mStream);
				assert(PA_STREAM_IS_GOOD(stream_state));
				if (stream_state == PA_STREAM_READY)
					break;
				if (stream_state == PA_STREAM_FAILED)
					goto unlock_and_fail;
				pa_threaded_mainloop_wait(mPMainLoop);
			}


			pa_op = pa_stream_cork(mStream, 0, stream_success_cb, this);
			if (pa_op)
				pa_operation_unref(pa_op);

			pa_threaded_mainloop_unlock(mPMainLoop);

			pa_op = pa_stream_update_timing_info(mStream, stream_success_cb, nullptr);
			if (pa_op)
				pa_operation_unref(pa_op);
			//const pa_timing_info* pa_ti = pa_stream_get_timing_info(mStream);

			pa_usec_t r_usec;
			int negative;
			ret = pa_stream_get_latency(mStream, &r_usec, &negative);

			// Setup resampler
			mResampler = src_delete(mResampler);

			mResampler = src_new(SRC_SINC_FASTEST, GetChannels(), &ret);
			if (!mResampler)
			{
				goto error;
			}

			mLastGetBuffer = hrc::now();
			return true;
		unlock_and_fail:
			pa_threaded_mainloop_unlock(mPMainLoop);
		error:
			Uninit();
			return false;
		}

		void PulseAudioDevice::ResetBuffers()
		{
			size_t bytes;
			std::lock_guard<std::mutex> lk(mMutex);
			pa_sample_spec ss(mSSpec);
			ss.rate = mSamplesPerSec;

			if (mAudioDir == AUDIODIR_SOURCE)
			{
				bytes = pa_bytes_per_second(&mSSpec) * mBuffering / 1000;
				bytes += bytes % pa_frame_size(&mSSpec); //align just in case
				mInBuffer.reserve(bytes);

				bytes = pa_bytes_per_second(&ss) * mBuffering / 1000;
				bytes += bytes % pa_frame_size(&ss);
				mOutBuffer.reserve(bytes);
			}
			else
			{
				bytes = pa_bytes_per_second(&mSSpec) * mBuffering / 1000;
				bytes += bytes % pa_frame_size(&mSSpec);
				mOutBuffer.reserve(bytes);

				bytes = pa_bytes_per_second(&ss) * mBuffering / 1000;
				bytes += bytes % pa_frame_size(&ss);
				mInBuffer.reserve(bytes);
			}

			src_reset(mResampler);
		}

		int PulseAudioDevice::Configure(int port, const char* dev_type, void* data)
		{
			int ret = RESULT_FAILED;
			if (PulseAudioDevice::AudioInit())
			{
				ret = GtkConfigure(port, dev_type, data);
				PulseAudioDevice::AudioDeinit();
			}
			return ret;
		}

		void PulseAudioDevice::AudioDevices(std::vector<AudioDeviceInfo>& devices, AudioDir& dir)
		{
			pa_get_devicelist(devices, dir);
		}

		bool PulseAudioDevice::AudioInit()
		{
#ifdef DYNLINK_PULSE
			return DynLoadPulse();
#else
			return true;
#endif
		}

		void PulseAudioDevice::AudioDeinit()
		{
#ifdef DYNLINK_PULSE
			DynUnloadPulse();
#endif
		}

		void PulseAudioDevice::context_state_cb(pa_context* c, void* userdata)
		{
			pa_context_state_t state;
			PulseAudioDevice* padev = (PulseAudioDevice*)userdata;

			state = pa_context_get_state(c);
			switch (state)
			{
				case PA_CONTEXT_CONNECTING:
				case PA_CONTEXT_AUTHORIZING:
				case PA_CONTEXT_SETTING_NAME:
				default:
					break;
				case PA_CONTEXT_UNCONNECTED:
					padev->mPAready = 3;
					break;
				case PA_CONTEXT_FAILED:
				case PA_CONTEXT_TERMINATED:
					padev->mPAready = 2;
					break;
				case PA_CONTEXT_READY:
					padev->mPAready = 1;
					break;
			}

			pa_threaded_mainloop_signal(padev->mPMainLoop, 0);
		}

		void PulseAudioDevice::stream_state_cb(pa_stream* s, void* userdata)
		{
			PulseAudioDevice* padev = (PulseAudioDevice*)userdata;
			pa_threaded_mainloop_signal(padev->mPMainLoop, 0);
		}

		void PulseAudioDevice::stream_read_cb(pa_stream* p, size_t nbytes, void* userdata)
		{
			std::vector<float> rebuf;
			SRC_DATA data;
			PulseAudioDevice* padev = (PulseAudioDevice*)userdata;
			const void* padata = NULL;

			if (padev->mQuit)
				return;


			int ret = pa_stream_peek(p, &padata, &nbytes);

			if (ret != PA_OK)
				return;

			//auto dur = std::chrono::duration_cast<ms>(hrc::now() - padev->mLastGetBuffer).count();
			if (padev->mPaused /*|| dur > 5000*/ || (!padata && nbytes /* hole */))
			{
				ret = pa_stream_drop(p);
				return;
			}

			std::lock_guard<std::mutex> lock(padev->mMutex);

			padev->mInBuffer.write((uint8_t*)padata, nbytes);

			//if copy succeeded, drop samples at pulse's side
			ret = pa_stream_drop(p);

			size_t resampled = static_cast<size_t>(padev->mInBuffer.size<float>() * padev->mResampleRatio * padev->mTimeAdjust);
			if (resampled == 0)
				resampled = padev->mInBuffer.size<float>();
			rebuf.resize(resampled);

			size_t output_frames_gen = 0, input_frames_used = 0;
			float* pBegin = rebuf.data();
			float* pEnd = pBegin + rebuf.size();

			memset(&data, 0, sizeof(SRC_DATA));

			while (padev->mInBuffer.peek_read() > 0)
			{
				data.data_in = (float*)padev->mInBuffer.front();
				data.input_frames = padev->mInBuffer.peek_read<float>() / padev->GetChannels();
				data.data_out = pBegin;
				data.output_frames = (pEnd - pBegin) / padev->GetChannels();
				data.src_ratio = padev->mResampleRatio * padev->mTimeAdjust;

				src_process(padev->mResampler, &data);
				output_frames_gen += data.output_frames_gen;
				pBegin += data.output_frames_gen * padev->GetChannels();
				input_frames_used += data.input_frames_used;

				size_t samples = data.input_frames_used * padev->GetChannels();
				if (!samples)
					break; //TODO happens?
				padev->mInBuffer.read<float>(samples);
			}

			size_t output_samples = output_frames_gen * padev->GetChannels();
			float* pSrc = rebuf.data();
			while (output_samples > 0)
			{
				size_t samples = std::min(output_samples, padev->mOutBuffer.peek_write<short>(true));
				src_float_to_short_array(pSrc, padev->mOutBuffer.back<short>(), samples);
				padev->mOutBuffer.write<short>(samples);
				output_samples -= samples;
				pSrc += samples;
			}
		}

		void PulseAudioDevice::stream_write_cb(pa_stream* p, size_t nbytes, void* userdata)
		{
			void* pa_buffer = NULL;
			size_t pa_bytes;
			// The length of the data to write in bytes, must be in multiples of the stream's sample spec frame size
			ssize_t remaining_bytes = nbytes;
			int ret = PA_OK;
			std::vector<float> inFloats;
			SRC_DATA data;
			memset(&data, 0, sizeof(SRC_DATA));

			PulseAudioDevice* padev = (PulseAudioDevice*)userdata;
			if (padev->mQuit)
				return;

			{
				std::lock_guard<std::mutex> lock(padev->mMutex);
				// Convert short samples to float and to final output sample rate
				if (padev->mInBuffer.size() > 0)
				{
					inFloats.resize(padev->mInBuffer.size<short>());
					float* pDst = inFloats.data();

					while (padev->mInBuffer.peek_read() > 0)
					{
						size_t samples = padev->mInBuffer.peek_read<short>();
						src_short_to_float_array(
							(const short*)padev->mInBuffer.front(),
							pDst, samples);
						pDst += samples;
						padev->mInBuffer.read<short>(samples);
					}

					size_t input_frames_used = 0;
					size_t in_offset = 0;
					while (padev->mOutBuffer.peek_write<float>() > 0)
					{
						data.data_in = inFloats.data() + in_offset;
						data.input_frames = (inFloats.size() - in_offset) / padev->GetChannels();
						data.data_out = padev->mOutBuffer.back<float>();
						data.output_frames = padev->mOutBuffer.peek_write<float>() / padev->GetChannels();
						data.src_ratio = padev->mResampleRatio * padev->mTimeAdjust;

						src_process(padev->mResampler, &data);

						input_frames_used += data.input_frames_used;
						in_offset = input_frames_used * padev->GetChannels();

						padev->mOutBuffer.write<float>(data.output_frames_gen * padev->GetChannels());

						if (inFloats.size() <= in_offset || data.output_frames_gen == 0)
							break;
					}
				}
			}

			// Write converted float samples or silence to PulseAudio stream
			while (remaining_bytes > 0)
			{
				pa_bytes = remaining_bytes;

				ret = pa_stream_begin_write(padev->mStream, &pa_buffer, &pa_bytes);
				if (ret != PA_OK)
				{
					goto exit;
				}

				ssize_t final_bytes = 0;
				// read twice because possible wrap
				while (padev->mOutBuffer.size() > 0)
				{
					ssize_t read = std::min((ssize_t)pa_bytes - final_bytes, (ssize_t)padev->mOutBuffer.peek_read());
					if (read <= 0)
						break;

					memcpy((uint8_t*)pa_buffer + final_bytes, padev->mOutBuffer.front(), read);
					final_bytes += read;
					padev->mOutBuffer.read(read);
				}

				if ((ssize_t)pa_bytes > final_bytes)
					memset((uint8_t*)pa_buffer + final_bytes, 0, pa_bytes - final_bytes);

				ret = pa_stream_write(padev->mStream, pa_buffer, pa_bytes, NULL, 0LL, PA_SEEK_RELATIVE);
				if (ret != PA_OK)
				{
					pa_stream_cancel_write(padev->mStream); //TODO needed?
					goto exit;
				}

				remaining_bytes -= pa_bytes;
			}

		exit:

			return;
		}

	} // namespace audiodev_pulse
} // namespace usb_mic
