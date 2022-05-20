/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
#include "common/StringUtil.h"
#include "GS/GSState.h"
#include "GSDeviceOGL.h"
#include "GLState.h"
#include "GS/GSGL.h"
#include "GS/GSUtil.h"
#include "Host.h"
#include "HostDisplay.h"
#include <cinttypes>
#include <fstream>
#include <sstream>

//#define ONLY_LINES

// TODO port those value into PerfMon API
#ifdef ENABLE_OGL_DEBUG_MEM_BW
u64 g_real_texture_upload_byte = 0;
u64 g_vertex_upload_byte = 0;
u64 g_uniform_upload_byte = 0;
#endif

static constexpr u32 g_vs_cb_index        = 1;
static constexpr u32 g_ps_cb_index        = 0;

static constexpr u32 VERTEX_BUFFER_SIZE = 32 * 1024 * 1024;
static constexpr u32 INDEX_BUFFER_SIZE = 16 * 1024 * 1024;
static constexpr u32 VERTEX_UNIFORM_BUFFER_SIZE = 8 * 1024 * 1024;
static constexpr u32 FRAGMENT_UNIFORM_BUFFER_SIZE = 8 * 1024 * 1024;

int   GSDeviceOGL::m_shader_inst = 0;
int   GSDeviceOGL::m_shader_reg  = 0;
FILE* GSDeviceOGL::m_debug_gl_file = NULL;

GSDeviceOGL::GSDeviceOGL()
	: m_fbo(0)
	, m_fbo_read(0)
	, m_palette_ss(0)
{
	// Reset the debug file
#ifdef ENABLE_OGL_DEBUG
	m_debug_gl_file = fopen("GS_opengl_debug.txt", "w");
#endif

	m_disable_hw_gl_draw = theApp.GetConfigB("disable_hw_gl_draw");
}

GSDeviceOGL::~GSDeviceOGL()
{
#ifdef ENABLE_OGL_DEBUG
	if (m_debug_gl_file)
	{
		fclose(m_debug_gl_file);
		m_debug_gl_file = NULL;
	}
#endif

	// Clean vertex buffer state
	if (m_vertex_array_object)
		glDeleteVertexArrays(1, &m_vertex_array_object);
	m_vertex_stream_buffer.reset();
	m_index_stream_buffer.reset();

	// Clean m_convert
	delete m_convert.dss;
	delete m_convert.dss_write;

	// Clean m_date
	delete m_date.dss;

	// Clean various opengl allocation
	glDeleteFramebuffers(1, &m_fbo);
	glDeleteFramebuffers(1, &m_fbo_read);

	// Delete HW FX
	m_vertex_uniform_stream_buffer.reset();
	m_fragment_uniform_stream_buffer.reset();
	glDeleteSamplers(1, &m_palette_ss);

	m_programs.clear();

	glDeleteSamplers(std::size(m_ps_ss), m_ps_ss);

	for (GSDepthStencilOGL* ds : m_om_dss)
		delete ds;

	PboPool::Destroy();
}

void GSDeviceOGL::GenerateProfilerData()
{
	if (m_profiler.last_query < 3)
	{
		glDeleteQueries(1 << 16, m_profiler.timer_query);
		return;
	}

	// Wait latest quey to get valid result
	GLuint available = 0;
	while (!available)
	{
		glGetQueryObjectuiv(m_profiler.timer(), GL_QUERY_RESULT_AVAILABLE, &available);
	}

	GLuint64 time_start = 0;
	GLuint64 time_end = 0;
	std::vector<double> times;
	constexpr double ms = 0.000001;

	const int replay = theApp.GetConfigI("linux_replay");
	const int first_query = replay > 1 ? m_profiler.last_query / replay : 0;

	glGetQueryObjectui64v(m_profiler.timer_query[first_query], GL_QUERY_RESULT, &time_start);
	for (u32 q = first_query + 1; q < m_profiler.last_query; q++)
	{
		glGetQueryObjectui64v(m_profiler.timer_query[q], GL_QUERY_RESULT, &time_end);
		u64 t = time_end - time_start;
		times.push_back((double)t * ms);

		time_start = time_end;
	}

	// Latest value is often silly, just drop it
	times.pop_back();

	glDeleteQueries(1 << 16, m_profiler.timer_query);

	const double frames = times.size();
	double mean = 0.0;
	double sd = 0.0;

	auto minmax_time = std::minmax_element(times.begin(), times.end());

	for (auto t : times)
		mean += t;
	mean = mean / frames;

	for (auto t : times)
		sd += pow(t - mean, 2);
	sd = sqrt(sd / frames);

	u32 time_repartition[16] = {0};
	for (auto t : times)
	{
		size_t slot = std::min<size_t>(t / 2.0, std::size(time_repartition) - 1);
		time_repartition[slot]++;
	}

	fprintf(stderr, "\nPerformance Profile for %.0f frames:\n", frames);
	fprintf(stderr, "Min  %4.2f ms\t(%4.2f fps)\n", *minmax_time.first, 1000.0 / *minmax_time.first);
	fprintf(stderr, "Mean %4.2f ms\t(%4.2f fps)\n", mean, 1000.0 / mean);
	fprintf(stderr, "Max  %4.2f ms\t(%4.2f fps)\n", *minmax_time.second, 1000.0 / *minmax_time.second);
	fprintf(stderr, "SD   %4.2f ms\n", sd);
	fprintf(stderr, "\n");
	fprintf(stderr, "Frame Repartition\n");
	for (u32 i = 0; i < std::size(time_repartition); i++)
	{
		fprintf(stderr, "%3u ms => %3u ms\t%4u\n", 2 * i, 2 * (i + 1), time_repartition[i]);
	}

	FILE* csv = fopen("GS_profile.csv", "w");
	if (csv)
	{
		for (size_t i = 0; i < times.size(); i++)
		{
			fprintf(csv, "%zu,%lf\n", i, times[i]);
		}

		fclose(csv);
	}
}

GSTexture* GSDeviceOGL::CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format)
{
	GL_PUSH("Create surface");
	return new GSTextureOGL(type, width, height, levels, format, m_fbo_read);
}

bool GSDeviceOGL::Create(HostDisplay* display)
{
	if (!GSDevice::Create(display))
		return false;

	if (display->GetRenderAPI() != HostDisplay::RenderAPI::OpenGL)
		return false;

	// Check openGL requirement as soon as possible so we can switch to another
	// renderer/device
	if (!GLLoader::check_gl_requirements())
		return false;

	if (!theApp.GetConfigB("disable_shader_cache"))
	{
		if (!m_shader_cache.Open(false, EmuFolders::Cache, SHADER_VERSION))
			Console.Warning("Shader cache failed to open.");
	}
	else
	{
		Console.WriteLn("Not using shader cache.");
	}

	// optional features based on context
	m_features.broken_point_sampler = GLLoader::vendor_id_amd;
	m_features.geometry_shader = GLLoader::found_geometry_shader;
	m_features.image_load_store = GLLoader::found_GL_ARB_shader_image_load_store && GLLoader::found_GL_ARB_clear_texture;
	m_features.texture_barrier = GSConfig.OverrideTextureBarriers != 0 || GLLoader::found_framebuffer_fetch;
	m_features.provoking_vertex_last = true;
	m_features.dxt_textures = GL_EXT_texture_compression_s3tc;
	m_features.bptc_textures = GL_VERSION_4_2 || GL_ARB_texture_compression_bptc || GL_EXT_texture_compression_bptc;
	m_features.prefer_new_textures = false;
	m_features.framebuffer_fetch = GLLoader::found_framebuffer_fetch;
	m_features.dual_source_blend = GLLoader::has_dual_source_blend && !GSConfig.DisableDualSourceBlend;
	m_features.stencil_buffer = true;

	GLint point_range[2] = {};
	GLint line_range[2] = {};
	glGetIntegerv(GL_ALIASED_POINT_SIZE_RANGE, point_range);
	glGetIntegerv(GL_ALIASED_LINE_WIDTH_RANGE, line_range);
	m_features.point_expand = (point_range[0] <= static_cast<GLint>(GSConfig.UpscaleMultiplier) && point_range[1] >= static_cast<GLint>(GSConfig.UpscaleMultiplier));
	m_features.line_expand = (line_range[0] <= static_cast<GLint>(GSConfig.UpscaleMultiplier) && line_range[1] >= static_cast<GLint>(GSConfig.UpscaleMultiplier));
	Console.WriteLn("Using %s for point expansion and %s for line expansion.",
		m_features.point_expand ? "hardware" : "geometry shaders", m_features.line_expand ? "hardware" : "geometry shaders");

	{
		auto shader = Host::ReadResourceFileToString("shaders/opengl/common_header.glsl");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/opengl/common_header.glsl.");
			return false;
		}

		m_shader_common_header = std::move(*shader);
	}

	// because of fbo bindings below...
	GLState::Clear();

	// ****************************************************************
	// Debug helper
	// ****************************************************************
#ifdef ENABLE_OGL_DEBUG
	if (GSConfig.UseDebugDevice)
	{
		glDebugMessageCallback((GLDEBUGPROC)DebugOutputToFile, NULL);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, true);
		// Useless info message on Nvidia driver
		GLuint ids[] = {0x20004};
		glDebugMessageControl(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE, std::size(ids), ids, false);
	}
#endif

	// WARNING it must be done after the control setup (at least on MESA)
	GL_PUSH("GSDeviceOGL::Create");

	// ****************************************************************
	// Various object
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Various");

		glGenFramebuffers(1, &m_fbo);
		// Always write to the first buffer
		OMSetFBO(m_fbo);
		const GLenum target[1] = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, target);
		OMSetFBO(0);

		glGenFramebuffers(1, &m_fbo_read);
		// Always read from the first buffer
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		// Some timers to help profiling
		if (GLLoader::in_replayer)
		{
			glCreateQueries(GL_TIMESTAMP, 1 << 16, m_profiler.timer_query);
		}
	}

	// ****************************************************************
	// Vertex buffer state
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Vertex Buffer");

		glGenVertexArrays(1, &m_vertex_array_object);
		glBindVertexArray(m_vertex_array_object);

		m_vertex_stream_buffer = GL::StreamBuffer::Create(GL_ARRAY_BUFFER, VERTEX_BUFFER_SIZE);
		m_index_stream_buffer = GL::StreamBuffer::Create(GL_ELEMENT_ARRAY_BUFFER, INDEX_BUFFER_SIZE);
		m_vertex_uniform_stream_buffer = GL::StreamBuffer::Create(GL_UNIFORM_BUFFER, VERTEX_UNIFORM_BUFFER_SIZE);
		m_fragment_uniform_stream_buffer = GL::StreamBuffer::Create(GL_UNIFORM_BUFFER, FRAGMENT_UNIFORM_BUFFER_SIZE);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &m_uniform_buffer_alignment);
		if (!m_vertex_stream_buffer || !m_index_stream_buffer || !m_vertex_uniform_stream_buffer || !m_fragment_uniform_stream_buffer)
		{
			Host::ReportErrorAsync("GS", "Failed to create vertex/index/uniform streaming buffers");
			return false;
		}

		// rebind because of VAO state
		m_vertex_stream_buffer->Bind();
		m_index_stream_buffer->Bind();

		static_assert(sizeof(GSVertexPT1) == sizeof(GSVertex), "wrong GSVertex size");
		for (u32 i = 0; i < 8; i++)
			glEnableVertexAttribArray(i);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GSVertexPT1), (const GLvoid*)(0));
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GSVertexPT1), (const GLvoid*)(16));
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(GSVertex), (const GLvoid*)(8));
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GSVertex), (const GLvoid*)(12));
		glVertexAttribIPointer(4, 2, GL_UNSIGNED_SHORT, sizeof(GSVertex), (const GLvoid*)(16));
		glVertexAttribIPointer(5, 1, GL_UNSIGNED_INT, sizeof(GSVertex), (const GLvoid*)(20));
		glVertexAttribIPointer(6, 2, GL_UNSIGNED_SHORT, sizeof(GSVertex), (const GLvoid*)(24));
		glVertexAttribPointer(7, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GSVertex), (const GLvoid*)(28));
	}

	// must be done after va is created
	GLState::Clear();
	RestoreAPIState();

	// ****************************************************************
	// Pre Generate the different sampler object
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Sampler");

		for (u32 key = 0; key < std::size(m_ps_ss); key++)
		{
			m_ps_ss[key] = CreateSampler(PSSamplerSelector(key));
		}
	}

	// ****************************************************************
	// convert
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Convert");

		// these all share the same vertex shader
		const auto shader = Host::ReadResourceFileToString("shaders/opengl/convert.glsl");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/opengl/convert.glsl.");
			return false;
		}

		m_convert.vs = GetShaderSource("vs_main", GL_VERTEX_SHADER, m_shader_common_header, *shader, {});

		for (size_t i = 0; i < std::size(m_convert.ps); i++)
		{
			const char* name = shaderName(static_cast<ShaderConvert>(i));
			const std::string macro_sel = (static_cast<ShaderConvert>(i) == ShaderConvert::RGBA_TO_8I) ?
                                              StringUtil::StdStringFromFormat("#define PS_SCALE_FACTOR %d\n", GSConfig.UpscaleMultiplier) :
                                              std::string();
			const std::string ps(GetShaderSource(name, GL_FRAGMENT_SHADER, m_shader_common_header, *shader, macro_sel));
			if (!m_shader_cache.GetProgram(&m_convert.ps[i], m_convert.vs, {}, ps))
				return false;
			m_convert.ps[i].SetFormattedName("Convert pipe %s", name);

			if (static_cast<ShaderConvert>(i) == ShaderConvert::YUV)
				m_convert.ps[i].RegisterUniform("EMOD");
		}

		const PSSamplerSelector point;
		m_convert.pt = GetSamplerID(point);

		PSSamplerSelector bilinear;
		bilinear.biln = true;
		m_convert.ln = GetSamplerID(bilinear);

		m_convert.dss = new GSDepthStencilOGL();
		m_convert.dss_write = new GSDepthStencilOGL();
		m_convert.dss_write->EnableDepth();
		m_convert.dss_write->SetDepth(GL_ALWAYS, true);
	}

	// ****************************************************************
	// merge
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Merge");

		const auto shader = Host::ReadResourceFileToString("shaders/opengl/merge.glsl");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/opengl/merge.glsl.");
			return false;
		}

		for (size_t i = 0; i < std::size(m_merge_obj.ps); i++)
		{
			const std::string ps(GetShaderSource(StringUtil::StdStringFromFormat("ps_main%d", i), GL_FRAGMENT_SHADER, m_shader_common_header, *shader, {}));
			if (!m_shader_cache.GetProgram(&m_merge_obj.ps[i], m_convert.vs, {}, ps))
				return false;
			m_merge_obj.ps[i].SetFormattedName("Merge pipe %zu", i);
			m_merge_obj.ps[i].RegisterUniform("BGColor");
		}
	}

	// ****************************************************************
	// interlace
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Interlace");

		const auto shader = Host::ReadResourceFileToString("shaders/opengl/interlace.glsl");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/opengl/interlace.glsl.");
			return false;
		}

		for (size_t i = 0; i < std::size(m_interlace.ps); i++)
		{
			const std::string ps(GetShaderSource(StringUtil::StdStringFromFormat("ps_main%d", i), GL_FRAGMENT_SHADER, m_shader_common_header, *shader, {}));
			if (!m_shader_cache.GetProgram(&m_interlace.ps[i], m_convert.vs, {}, ps))
				return false;
			m_interlace.ps[i].SetFormattedName("Merge pipe %zu", i);
			m_interlace.ps[i].RegisterUniform("ZrH");
		}
	}

	// ****************************************************************
	// Shade boost
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Shadeboost");

		const auto shader = Host::ReadResourceFileToString("shaders/opengl/shadeboost.glsl");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/opengl/shadeboost.glsl.");
			return false;
		}

		const std::string ps(GetShaderSource("ps_main", GL_FRAGMENT_SHADER, m_shader_common_header, *shader, {}));
		if (!m_shader_cache.GetProgram(&m_shadeboost.ps, m_convert.vs, {}, ps))
			return false;
		m_shadeboost.ps.RegisterUniform("params");
		m_shadeboost.ps.SetName("Shadeboost pipe");
	}

	// ****************************************************************
	// rasterization configuration
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Rasterization");

#ifdef ONLY_LINES
		glLineWidth(5.0);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
		glDisable(GL_CULL_FACE);
		glEnable(GL_SCISSOR_TEST);
		glDisable(GL_MULTISAMPLE);
		glDisable(GL_DITHER); // Honestly I don't know!
	}

	// ****************************************************************
	// DATE
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::Date");

		m_date.dss = new GSDepthStencilOGL();
		m_date.dss->EnableStencil();
		m_date.dss->SetStencil(GL_ALWAYS, GL_REPLACE);
	}

	// ****************************************************************
	// Use DX coordinate convention
	// ****************************************************************

	// VS gl_position.z => [-1,-1]
	// FS depth => [0, 1]
	// because of -1 we loose lot of precision for small GS value
	// This extension allow FS depth to range from -1 to 1. So
	// gl_position.z could range from [0, 1]
	// Change depth convention
	if (GLExtension::Has("GL_ARB_clip_control"))
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	// ****************************************************************
	// HW renderer shader
	// ****************************************************************
	if (!CreateTextureFX())
		return false;

	// ****************************************************************
	// Pbo Pool allocation
	// ****************************************************************
	{
		GL_PUSH("GSDeviceOGL::PBO");

		// Mesa seems to use it to compute the row length. In our case, we are
		// tightly packed so don't bother with this parameter and set it to the
		// minimum alignment (1 byte)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		PboPool::Init();
	}

	// ****************************************************************
	// Get Available Memory
	// ****************************************************************
	GLint vram[4] = {0};
	if (GLLoader::vendor_id_amd)
	{
		// Full vram, remove a small margin for others buffer
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, vram);
	}
	else if (GLExtension::Has("GL_NVX_gpu_memory_info"))
	{
		// GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX <= give full memory
		// Available vram
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, vram);
	}
	else
	{
		fprintf(stdout, "No extenstion supported to get available memory. Use default value !\n");
	}

	// When VRAM is at least 2GB, we set the limit to the default i.e. 3.8 GB
	// When VRAM is below 2GB, we add a factor 2 because RAM can be used. Potentially
	// low VRAM gpu can go higher but perf will be bad anyway.
	if (vram[0] > 0 && vram[0] < 1800000)
		GLState::available_vram = (s64)(vram[0]) * 1024ul * 2ul;

	fprintf(stdout, "Available VRAM/RAM:%lldMB for textures\n", GLState::available_vram >> 20u);

	// Basic to ensure structures are correctly packed
	static_assert(sizeof(VSSelector) == 1, "Wrong VSSelector size");
	static_assert(sizeof(PSSelector) == 12, "Wrong PSSelector size");
	static_assert(sizeof(PSSamplerSelector) == 1, "Wrong PSSamplerSelector size");
	static_assert(sizeof(OMDepthStencilSelector) == 1, "Wrong OMDepthStencilSelector size");
	static_assert(sizeof(OMColorMaskSelector) == 1, "Wrong OMColorMaskSelector size");

	return true;
}

bool GSDeviceOGL::CreateTextureFX()
{
	GL_PUSH("GSDeviceOGL::CreateTextureFX");

	auto vertex_shader = Host::ReadResourceFileToString("shaders/opengl/tfx_vgs.glsl");
	auto fragment_shader = Host::ReadResourceFileToString("shaders/opengl/tfx_fs.glsl");
	if (!vertex_shader.has_value() || !fragment_shader.has_value())
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/opengl/tfx_{vgs,fs}.glsl.");
		return false;
	}

	m_shader_tfx_vgs = std::move(*vertex_shader);
	m_shader_tfx_fs = std::move(*fragment_shader);

	// warning 1 sampler by image unit. So you cannot reuse m_ps_ss...
	m_palette_ss = CreateSampler(PSSamplerSelector(0));
	glBindSampler(1, m_palette_ss);

	// Enable all bits for stencil operations. Technically 1 bit is
	// enough but buffer is polluted with noise. Clear will be limited
	// to the mask.
	glStencilMask(0xFF);
	for (u32 key = 0; key < std::size(m_om_dss); key++)
	{
		m_om_dss[key] = CreateDepthStencil(OMDepthStencilSelector(key));
	}

	if (GLLoader::in_replayer)
	{
		glQueryCounter(m_profiler.timer(), GL_TIMESTAMP);
		m_profiler.last_query++;
	}

	return true;
}

void GSDeviceOGL::ResetAPIState()
{
	if (GLState::point_size)
		glDisable(GL_PROGRAM_POINT_SIZE);
	if (GLState::line_width != 1.0f)
		glLineWidth(1.0f);
}

void GSDeviceOGL::RestoreAPIState()
{
	glBindVertexArray(m_vertex_array_object);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLState::fbo);

	glViewportIndexedf(0, 0, 0, static_cast<float>(GLState::viewport.x), static_cast<float>(GLState::viewport.y));
	glScissorIndexed(0, GLState::scissor.x, GLState::scissor.y, GLState::scissor.width(), GLState::scissor.height());

	glBlendEquationSeparate(GLState::eq_RGB, GL_FUNC_ADD);
	glBlendFuncSeparate(GLState::f_sRGB, GLState::f_dRGB, GL_ONE, GL_ZERO);
	
	const float bf = static_cast<float>(GLState::bf) / 128.0f;
	glBlendColor(bf, bf, bf, bf);

	if (GLState::blend)
	{
		glEnable(GL_BLEND);
	}
	else
	{
		glDisable(GL_BLEND);
	}

	const OMColorMaskSelector msel{ GLState::wrgba };
	glColorMask(msel.wr, msel.wg, msel.wb, msel.wa);

	GLState::depth ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
	glDepthFunc(GLState::depth_func);
	glDepthMask(GLState::depth_mask);

	if (GLState::stencil)
	{
		glEnable(GL_STENCIL_TEST);
	}
	else
	{
		glDisable(GL_STENCIL_TEST);
	}

	glStencilFunc(GLState::stencil_func, 1, 1);
	glStencilOp(GL_KEEP, GL_KEEP, GLState::stencil_pass);

	glBindSampler(0, GLState::ps_ss);
	
	for (GLuint i = 0; i < sizeof(GLState::tex_unit) / sizeof(GLState::tex_unit[0]); i++)
		glBindTextureUnit(i, GLState::tex_unit[i]);

	if (GLState::point_size)
		glEnable(GL_PROGRAM_POINT_SIZE);
	if (GLState::line_width != 1.0f)
		glLineWidth(GLState::line_width);
}

void GSDeviceOGL::DrawPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	glDrawArrays(m_draw_topology, m_vertex.start, m_vertex.count);
}

void GSDeviceOGL::DrawIndexedPrimitive()
{
	if (!m_disable_hw_gl_draw)
	{
		g_perfmon.Put(GSPerfMon::DrawCalls, 1);
		glDrawElementsBaseVertex(m_draw_topology, static_cast<u32>(m_index.count), GL_UNSIGNED_INT,
			reinterpret_cast<void*>(static_cast<u32>(m_index.start) * sizeof(u32)), static_cast<GLint>(m_vertex.start));
	}
}

void GSDeviceOGL::DrawIndexedPrimitive(int offset, int count)
{
	//ASSERT(offset + count <= (int)m_index.count);

	if (!m_disable_hw_gl_draw)
	{
		g_perfmon.Put(GSPerfMon::DrawCalls, 1);
		glDrawElementsBaseVertex(m_draw_topology, count, GL_UNSIGNED_INT,
			reinterpret_cast<void*>((static_cast<u32>(m_index.start) + static_cast<u32>(offset)) * sizeof(u32)),
			static_cast<GLint>(m_vertex.start));
	}
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t)
		return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);
	if (T->HasBeenCleaned())
		return;

	// Performance note: potentially T->Clear() could be used. Main purpose of
	// Clear() is to avoid the framebuffer setup cost. However, in this context,
	// the texture 't' will be set as the render target of the framebuffer and
	// therefore will require a framebuffer setup.

	// So using the old/standard path is faster/better albeit verbose.

	GL_PUSH("Clear RT %d", T->GetID());

	// TODO: check size of scissor before toggling it
	glDisable(GL_SCISSOR_TEST);

	const u32 old_color_mask = GLState::wrgba;
	OMSetColorMaskState();

	OMSetFBO(m_fbo);
	OMAttachRt(T);

	glClearBufferfv(GL_COLOR, 0, c.v);

	OMSetColorMaskState(OMColorMaskSelector(old_color_mask));

	glEnable(GL_SCISSOR_TEST);

	T->WasCleaned();
}

void GSDeviceOGL::ClearRenderTarget(GSTexture* t, u32 c)
{
	if (!t)
		return;

	const GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);
	ClearRenderTarget(t, color);
}

void GSDeviceOGL::InvalidateRenderTarget(GSTexture* t)
{
	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);
	if (!T || T->HasBeenCleaned())
		return;

	if (GLAD_GL_VERSION_4_3 || GLAD_GL_ES_VERSION_3_0)
	{
		OMSetFBO(m_fbo);

		if (T->GetType() == GSTexture::Type::DepthStencil || T->GetType() == GSTexture::Type::SparseDepthStencil)
		{
			OMAttachDs(T);
			const GLenum attachments[] = {GL_DEPTH_STENCIL_ATTACHMENT};
			glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, std::size(attachments), attachments);
		}
		else
		{
			OMAttachRt(T);
			const GLenum attachments[] = {GL_COLOR_ATTACHMENT0};
			glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, std::size(attachments), attachments);
		}
	}
}

void GSDeviceOGL::ClearDepth(GSTexture* t)
{
	if (!t)
		return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	GL_PUSH("Clear Depth %d", T->GetID());

	if (0 && GLLoader::found_GL_ARB_clear_texture)
	{
		// I don't know what the driver does but it creates
		// some slowdowns on Harry Potter PS
		// Maybe it triggers some texture relocations, or maybe
		// it clears also the stencil value (2 times slower)
		//
		// Let's disable this code for the moment.

		// Don't bother with Depth_Stencil insanity
		T->Clear(NULL);
	}
	else
	{
		OMSetFBO(m_fbo);
		// RT must be detached, if RT is too small, depth won't be fully cleared
		// AT tolenico 2 map clip bug
		OMAttachRt(NULL);
		OMAttachDs(T);

		// TODO: check size of scissor before toggling it
		glDisable(GL_SCISSOR_TEST);
		const float c = 0.0f;
		if (GLState::depth_mask)
		{
			glClearBufferfv(GL_DEPTH, 0, &c);
		}
		else
		{
			glDepthMask(true);
			glClearBufferfv(GL_DEPTH, 0, &c);
			glDepthMask(false);
		}
		glEnable(GL_SCISSOR_TEST);
	}
}

void GSDeviceOGL::ClearStencil(GSTexture* t, u8 c)
{
	if (!t)
		return;

	GSTextureOGL* T = static_cast<GSTextureOGL*>(t);

	GL_PUSH("Clear Stencil %d", T->GetID());

	// Keep SCISSOR_TEST enabled on purpose to reduce the size
	// of clean in DATE (impact big upscaling)
	OMSetFBO(m_fbo);
	OMAttachDs(T);
	const GLint color = c;

	glClearBufferiv(GL_STENCIL, 0, &color);
}

GLuint GSDeviceOGL::CreateSampler(PSSamplerSelector sel)
{
	GL_PUSH("Create Sampler");

	GLuint sampler;
	glCreateSamplers(1, &sampler);

	glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, sel.IsMagFilterLinear() ? GL_LINEAR : GL_NEAREST);
	if (!sel.UseMipmapFiltering())
	{
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, sel.IsMinFilterLinear() ? GL_LINEAR : GL_NEAREST);
	}
	else
	{
		if (sel.IsMipFilterLinear())
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, sel.IsMinFilterLinear() ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
		else
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, sel.IsMinFilterLinear() ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
	}

	glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, -1000.0f);
	glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, sel.lodclamp ? 0.0f : 1000.0f);

	if (sel.tau)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if (sel.tav)
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	const int anisotropy = GSConfig.MaxAnisotropy;
	if (anisotropy && sel.aniso)
	{
		if (GLExtension::Has("GL_ARB_texture_filter_anisotropic"))
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, (float)anisotropy);
		else if (GLExtension::Has("GL_EXT_texture_filter_anisotropic"))
			glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)anisotropy);
	}

	return sampler;
}

GLuint GSDeviceOGL::GetSamplerID(PSSamplerSelector ssel)
{
	return m_ps_ss[ssel.key];
}

GSDepthStencilOGL* GSDeviceOGL::CreateDepthStencil(OMDepthStencilSelector dssel)
{
	GSDepthStencilOGL* dss = new GSDepthStencilOGL();

	if (dssel.date)
	{
		dss->EnableStencil();
		if (dssel.date_one)
			dss->SetStencil(GL_EQUAL, GL_ZERO);
		else
			dss->SetStencil(GL_EQUAL, GL_KEEP);
	}

	if (dssel.ztst != ZTST_ALWAYS || dssel.zwe)
	{
		static const GLenum ztst[] =
		{
			GL_NEVER,
			GL_ALWAYS,
			GL_GEQUAL,
			GL_GREATER
		};
		dss->EnableDepth();
		dss->SetDepth(ztst[dssel.ztst], dssel.zwe);
	}

	return dss;
}

void GSDeviceOGL::InitPrimDateTexture(GSTexture* rt, const GSVector4i& area)
{
	const GSVector2i& rtsize = rt->GetSize();

	// Create a texture to avoid the useless clean@0
	if (m_date.t == NULL)
		m_date.t = CreateTexture(rtsize.x, rtsize.y, false, GSTexture::Format::PrimID);

	// Clean with the max signed value
	const int max_int = 0x7FFFFFFF;
	static_cast<GSTextureOGL*>(m_date.t)->Clear(&max_int, area);

	glBindImageTexture(3, static_cast<GSTextureOGL*>(m_date.t)->GetID(), 0, false, 0, GL_READ_WRITE, GL_R32I);
#ifdef ENABLE_OGL_DEBUG
	// Help to see the texture in apitrace
	PSSetShaderResource(3, m_date.t);
#endif
}

void GSDeviceOGL::RecycleDateTexture()
{
	if (m_date.t)
	{
		//static_cast<GSTextureOGL*>(m_date.t)->Save(format("/tmp/date_adv_%04ld.csv", GSState::s_n));

		Recycle(m_date.t);
		m_date.t = NULL;
	}
}

void GSDeviceOGL::Barrier(GLbitfield b)
{
	glMemoryBarrier(b);
}

std::string GSDeviceOGL::GetShaderSource(const std::string_view& entry, GLenum type, const std::string_view& common_header, const std::string_view& glsl_h_code, const std::string_view& macro_sel)
{
	std::string src = GenGlslHeader(entry, type, macro_sel);
	src += m_shader_common_header;
	src += glsl_h_code;
	return src;
}

std::string GSDeviceOGL::GenGlslHeader(const std::string_view& entry, GLenum type, const std::string_view& macro)
{
	std::string header = "#version 330 core\n";

	// Need GL version 420
	header += "#extension GL_ARB_shading_language_420pack: require\n";
	// Need GL version 410
	header += "#extension GL_ARB_separate_shader_objects: require\n";
	if (m_features.framebuffer_fetch)
	{
		if (GLAD_GL_EXT_shader_framebuffer_fetch)
			header += "#extension GL_EXT_shader_framebuffer_fetch : require\n";
		else if (GLAD_GL_ARM_shader_framebuffer_fetch)
			header += "#extension GL_ARM_shader_framebuffer_fetch : require\n";
	}

	if (GLLoader::found_GL_ARB_gpu_shader5)
		header += "#extension GL_ARB_gpu_shader5 : enable\n";

	if (GLLoader::found_GL_ARB_shader_image_load_store)
	{
		// Need GL version 420
		header += "#extension GL_ARB_shader_image_load_store: require\n";
	}
	else
	{
		header += "#define DISABLE_GL42_image\n";
	}

	if (m_features.framebuffer_fetch)
		header += "#define HAS_FRAMEBUFFER_FETCH 1\n";
	else
		header += "#define HAS_FRAMEBUFFER_FETCH 0\n";

	if (GLLoader::vendor_id_amd || GLLoader::vendor_id_intel)
		header += "#define BROKEN_DRIVER as_usual\n";

	// Stupid GL implementation (can't use GL_ES)
	// AMD/nvidia define it to 0
	// intel window don't define it
	// intel linux refuse to define it
	header += "#define pGL_ES 0\n";

	// Allow to puts several shader in 1 files
	switch (type)
	{
		case GL_VERTEX_SHADER:
			header += "#define VERTEX_SHADER 1\n";
			break;
		case GL_GEOMETRY_SHADER:
			header += "#define GEOMETRY_SHADER 1\n";
			break;
		case GL_FRAGMENT_SHADER:
			header += "#define FRAGMENT_SHADER 1\n";
			break;
		default:
			ASSERT(0);
	}

	// Select the entry point ie the main function
	header += "#define ";
	header += entry;
	header += " main\n";

	header += macro;

	return header;
}

std::string GSDeviceOGL::GetVSSource(VSSelector sel)
{
#ifdef PCSX2_DEVBUILD
	Console.WriteLn("Compiling new vertex shader with selector 0x%" PRIX64, sel.key);
#endif

	std::string macro = StringUtil::StdStringFromFormat("#define VS_INT_FST %d\n", sel.int_fst)
		+ StringUtil::StdStringFromFormat("#define VS_IIP %d\n", sel.iip)
		+ StringUtil::StdStringFromFormat("#define VS_POINT_SIZE %d\n", sel.point_size);
	if (sel.point_size)
		macro += StringUtil::StdStringFromFormat("#define VS_POINT_SIZE_VALUE %d\n", GSConfig.UpscaleMultiplier);

	std::string src = GenGlslHeader("vs_main", GL_VERTEX_SHADER, macro);
	src += m_shader_common_header;
	src += m_shader_tfx_vgs;
	return src;
}

std::string GSDeviceOGL::GetGSSource(GSSelector sel)
{
#ifdef PCSX2_DEVBUILD
	Console.WriteLn("Compiling new geometry shader with selector 0x%" PRIX64, sel.key);
#endif

	std::string macro = StringUtil::StdStringFromFormat("#define GS_POINT %d\n", sel.point)
		+ StringUtil::StdStringFromFormat("#define GS_LINE %d\n", sel.line)
		+ StringUtil::StdStringFromFormat("#define GS_IIP %d\n", sel.iip);

	std::string src = GenGlslHeader("gs_main", GL_GEOMETRY_SHADER, macro);
	src += m_shader_common_header;
	src += m_shader_tfx_vgs;
	return src;
}

std::string GSDeviceOGL::GetPSSource(const PSSelector& sel)
{
#ifdef PCSX2_DEVBUILD
	Console.WriteLn("Compiling new pixel shader with selector 0x%" PRIX64 "%08X", sel.key_hi, sel.key_lo);
#endif

	std::string macro = StringUtil::StdStringFromFormat("#define PS_FST %d\n", sel.fst)
		+ StringUtil::StdStringFromFormat("#define PS_WMS %d\n", sel.wms)
		+ StringUtil::StdStringFromFormat("#define PS_WMT %d\n", sel.wmt)
		+ StringUtil::StdStringFromFormat("#define PS_AEM_FMT %d\n", sel.aem_fmt)
		+ StringUtil::StdStringFromFormat("#define PS_PAL_FMT %d\n", sel.pal_fmt)
		+ StringUtil::StdStringFromFormat("#define PS_DFMT %d\n", sel.dfmt)
		+ StringUtil::StdStringFromFormat("#define PS_DEPTH_FMT %d\n", sel.depth_fmt)
		+ StringUtil::StdStringFromFormat("#define PS_CHANNEL_FETCH %d\n", sel.channel)
		+ StringUtil::StdStringFromFormat("#define PS_URBAN_CHAOS_HLE %d\n", sel.urban_chaos_hle)
		+ StringUtil::StdStringFromFormat("#define PS_TALES_OF_ABYSS_HLE %d\n", sel.tales_of_abyss_hle)
		+ StringUtil::StdStringFromFormat("#define PS_TEX_IS_FB %d\n", sel.tex_is_fb)
		+ StringUtil::StdStringFromFormat("#define PS_INVALID_TEX0 %d\n", sel.invalid_tex0)
		+ StringUtil::StdStringFromFormat("#define PS_AEM %d\n", sel.aem)
		+ StringUtil::StdStringFromFormat("#define PS_TFX %d\n", sel.tfx)
		+ StringUtil::StdStringFromFormat("#define PS_TCC %d\n", sel.tcc)
		+ StringUtil::StdStringFromFormat("#define PS_ATST %d\n", sel.atst)
		+ StringUtil::StdStringFromFormat("#define PS_FOG %d\n", sel.fog)
		+ StringUtil::StdStringFromFormat("#define PS_CLR_HW %d\n", sel.clr_hw)
		+ StringUtil::StdStringFromFormat("#define PS_FBA %d\n", sel.fba)
		+ StringUtil::StdStringFromFormat("#define PS_LTF %d\n", sel.ltf)
		+ StringUtil::StdStringFromFormat("#define PS_AUTOMATIC_LOD %d\n", sel.automatic_lod)
		+ StringUtil::StdStringFromFormat("#define PS_MANUAL_LOD %d\n", sel.manual_lod)
		+ StringUtil::StdStringFromFormat("#define PS_COLCLIP %d\n", sel.colclip)
		+ StringUtil::StdStringFromFormat("#define PS_DATE %d\n", sel.date)
		+ StringUtil::StdStringFromFormat("#define PS_TCOFFSETHACK %d\n", sel.tcoffsethack)
		+ StringUtil::StdStringFromFormat("#define PS_POINT_SAMPLER %d\n", sel.point_sampler)
		+ StringUtil::StdStringFromFormat("#define PS_BLEND_A %d\n", sel.blend_a)
		+ StringUtil::StdStringFromFormat("#define PS_BLEND_B %d\n", sel.blend_b)
		+ StringUtil::StdStringFromFormat("#define PS_BLEND_C %d\n", sel.blend_c)
		+ StringUtil::StdStringFromFormat("#define PS_BLEND_D %d\n", sel.blend_d)
		+ StringUtil::StdStringFromFormat("#define PS_IIP %d\n", sel.iip)
		+ StringUtil::StdStringFromFormat("#define PS_SHUFFLE %d\n", sel.shuffle)
		+ StringUtil::StdStringFromFormat("#define PS_READ_BA %d\n", sel.read_ba)
		+ StringUtil::StdStringFromFormat("#define PS_WRITE_RG %d\n", sel.write_rg)
		+ StringUtil::StdStringFromFormat("#define PS_FBMASK %d\n", sel.fbmask)
		+ StringUtil::StdStringFromFormat("#define PS_HDR %d\n", sel.hdr)
		+ StringUtil::StdStringFromFormat("#define PS_DITHER %d\n", sel.dither)
		+ StringUtil::StdStringFromFormat("#define PS_ZCLAMP %d\n", sel.zclamp)
		+ StringUtil::StdStringFromFormat("#define PS_BLEND_MIX %d\n", sel.blend_mix)
		+ StringUtil::StdStringFromFormat("#define PS_PABE %d\n", sel.pabe)
		+ StringUtil::StdStringFromFormat("#define PS_SCANMSK %d\n", sel.scanmsk)
		+ StringUtil::StdStringFromFormat("#define PS_SCALE_FACTOR %d\n", GSConfig.UpscaleMultiplier)
		+ StringUtil::StdStringFromFormat("#define PS_NO_COLOR %d\n", sel.no_color)
		+ StringUtil::StdStringFromFormat("#define PS_NO_COLOR1 %d\n", sel.no_color1)
		+ StringUtil::StdStringFromFormat("#define PS_NO_ABLEND %d\n", sel.no_ablend)
		+ StringUtil::StdStringFromFormat("#define PS_ONLY_ALPHA %d\n", sel.only_alpha)
	;

	std::string src = GenGlslHeader("ps_main", GL_FRAGMENT_SHADER, macro);
	src += m_shader_common_header;
	src += m_shader_tfx_fs;
	return src;
}

bool GSDeviceOGL::DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map)
{
	ASSERT(src);
	g_perfmon.Put(GSPerfMon::Readbacks, 1);

	GSTextureOGL* srcgl = static_cast<GSTextureOGL*>(src);

	out_map = srcgl->Read(rect, m_download_buffer);
	return true;
}

// Copy a sub part of texture (same as below but force a conversion)
void GSDeviceOGL::BlitRect(GSTexture* sTex, const GSVector4i& r, const GSVector2i& dsize, bool at_origin, bool linear)
{
	GL_PUSH(StringUtil::StdStringFromFormat("CopyRectConv from %d", static_cast<GSTextureOGL*>(sTex)->GetID()).c_str());
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	// NOTE: This previously used glCopyTextureSubImage2D(), but this appears to leak memory in
	// the loading screens of Evolution Snowboarding in Intel/NVIDIA drivers.
	glDisable(GL_SCISSOR_TEST);

	const GSVector4 float_r(r);

	BeginScene();
	m_convert.ps[static_cast<int>(ShaderConvert::COPY)].Bind();
	OMSetDepthStencilState(m_convert.dss);
	OMSetBlendState();
	OMSetColorMaskState();
	PSSetShaderResource(0, sTex);
	PSSetSamplerState(linear ? m_convert.ln : m_convert.pt);
	DrawStretchRect(float_r / (GSVector4(sTex->GetSize()).xyxy()), float_r, dsize);
	EndScene();

	glEnable(GL_SCISSOR_TEST);
}

// Copy a sub part of a texture into another
void GSDeviceOGL::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY)
{
	ASSERT(sTex && dTex);
	if (!(sTex && dTex))
		return;

	const GLuint& sid = static_cast<GSTextureOGL*>(sTex)->GetID();
	const GLuint& did = static_cast<GSTextureOGL*>(dTex)->GetID();

	GL_PUSH("CopyRect from %d to %d", sid, did);

#ifdef ENABLE_OGL_DEBUG
	PSSetShaderResource(6, sTex);
#endif

	dTex->CommitRegion(GSVector2i(r.z, r.w));
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	ASSERT(GLExtension::Has("GL_ARB_copy_image") && glCopyImageSubData);
	glCopyImageSubData(sid, GL_TEXTURE_2D,
		0, r.x, r.y, 0,
		did, GL_TEXTURE_2D,
		0, destX, destY, 0,
		r.width(), r.height(), 1);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[(int)shader], linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, const GL::Program& ps, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, ps, false, OMColorMaskSelector(), linear);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha)
{
	OMColorMaskSelector cms;

	cms.wr = red;
	cms.wg = green;
	cms.wb = blue;
	cms.wa = alpha;

	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[(int)ShaderConvert::COPY], false, cms, false);
}

void GSDeviceOGL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, const GL::Program& ps, bool alpha_blend, OMColorMaskSelector cms, bool linear)
{
	ASSERT(sTex);

	const bool draw_in_depth = ps == m_convert.ps[static_cast<int>(ShaderConvert::DEPTH_COPY)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT32)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT24)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT16)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGB5A1_TO_FLOAT16)];

	// Performance optimization. It might be faster to use a framebuffer blit for standard case
	// instead to emulate it with shader
	// see https://www.opengl.org/wiki/Framebuffer#Blitting

	// ************************************
	// Init
	// ************************************

	BeginScene();

	GSVector2i ds;
	if (dTex)
	{
		GL_PUSH("StretchRect from %d to %d", sTex->GetID(), dTex->GetID());
		ds = dTex->GetSize();
		dTex->CommitRegion(GSVector2i((int)dRect.z + 1, (int)dRect.w + 1));
		if (draw_in_depth)
			OMSetRenderTargets(NULL, dTex);
		else
			OMSetRenderTargets(dTex, NULL);
	}
	else
	{
		ds = GSVector2i(m_display->GetWindowWidth(), m_display->GetWindowHeight());
	}

	ps.Bind();

	// ************************************
	// om
	// ************************************

	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write);
	else
		OMSetDepthStencilState(m_convert.dss);

	OMSetBlendState(alpha_blend, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD);
	OMSetColorMaskState(cms);

	// ************************************
	// ia
	// ************************************


	// Flip y axis only when we render in the backbuffer
	// By default everything is render in the wrong order (ie dx).
	// 1/ consistency between several pass rendering (interlace)
	// 2/ in case some GS code expect thing in dx order.
	// Only flipping the backbuffer is transparent (I hope)...
	GSVector4 flip_sr = sRect;
	if (!dTex)
	{
		flip_sr.y = sRect.w;
		flip_sr.w = sRect.y;
	}

	// ************************************
	// Texture
	// ************************************

	PSSetShaderResource(0, sTex);
	PSSetSamplerState(linear ? m_convert.ln : m_convert.pt);

	// ************************************
	// Draw
	// ************************************
	DrawStretchRect(flip_sr, dRect, ds);

	// ************************************
	// End
	// ************************************

	EndScene();
}

void GSDeviceOGL::DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds)
{
	// Original code from DX
	const float left = dRect.x * 2 / ds.x - 1.0f;
	const float right = dRect.z * 2 / ds.x - 1.0f;
#if 0
	const float top = 1.0f - dRect.y * 2 / ds.y;
	const float bottom = 1.0f - dRect.w * 2 / ds.y;
#else
	// Opengl get some issues with the coordinate
	// I flip top/bottom to fix scaling of the internal resolution
	const float top = -1.0f + dRect.y * 2 / ds.y;
	const float bottom = -1.0f + dRect.w * 2 / ds.y;
#endif

	GSVertexPT1 vertices[] =
	{
		{GSVector4(left  , top   , 0.0f, 0.0f) , GSVector2(sRect.x , sRect.y)} ,
		{GSVector4(right , top   , 0.0f, 0.0f) , GSVector2(sRect.z , sRect.y)} ,
		{GSVector4(left  , bottom, 0.0f, 0.0f) , GSVector2(sRect.x , sRect.w)} ,
		{GSVector4(right , bottom, 0.0f, 0.0f) , GSVector2(sRect.z , sRect.w)} ,
	};

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);
	DrawPrimitive();
}

void GSDeviceOGL::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	GL_PUSH("DoMerge");

	const GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	const bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	const bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	const bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;

	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	OMSetColorMaskState();
	ClearRenderTarget(dTex, c);

	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg))
	{
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		StretchRect(sTex[1], sRect[1], dTex, PMODE.SLBG ? dRect[2] : dRect[1], ShaderConvert::COPY);
	}

	// Upload constant to select YUV algo
	if (feedback_write_2 || feedback_write_1)
	{
		// Write result to feedback loop
		m_convert.ps[static_cast<int>(ShaderConvert::YUV)].Bind();
		m_convert.ps[static_cast<int>(ShaderConvert::YUV)].Uniform2i(0, EXTBUF.EMODA, EXTBUF.EMODC);
	}

	// Save 2nd output
	if (feedback_write_2)
		StretchRect(dTex, full_r, sTex[2], dRect[2], ShaderConvert::YUV);

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg)
		ClearRenderTarget(dTex, c);

	if (sTex[0])
	{
		if (PMODE.AMOD == 1) // Keep the alpha from the 2nd output
			OMSetColorMaskState(OMColorMaskSelector(0x7));

		// 1st output is enabled. It must be blended
		if (PMODE.MMOD == 1)
		{
			// Blend with a constant alpha
			m_merge_obj.ps[1].Bind();
			m_merge_obj.ps[1].Uniform4fv(0, c.v);
			StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge_obj.ps[1], true, OMColorMaskSelector());
		}
		else
		{
			// Blend with 2 * input alpha
			StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge_obj.ps[0], true, OMColorMaskSelector());
		}
	}

	if (feedback_write_1)
		StretchRect(dTex, full_r, sTex[2], dRect[2], ShaderConvert::YUV);
}

void GSDeviceOGL::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	GL_PUSH("DoInterlace");

	OMSetColorMaskState();

	const GSVector4 s = GSVector4(dTex->GetSize());

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	m_interlace.ps[shader].Bind();
	m_interlace.ps[shader].Uniform2f(0, 0, 1.0f / s.y);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader], linear);
}

void GSDeviceOGL::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	// Lazy compile
	if (!m_fxaa.ps.IsValid())
	{
		// Needs ARB_gpu_shader5 for gather.
		if (!GLLoader::found_GL_ARB_gpu_shader5)
			return;

		std::string fxaa_macro = "#define FXAA_GLSL_130 1\n";
		std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/common/fxaa.fx");
		if (!shader.has_value())
			return;

		const std::string ps(GetShaderSource("ps_main", GL_FRAGMENT_SHADER, m_shader_common_header, shader->c_str(), fxaa_macro));
		if (!m_fxaa.ps.Compile(m_convert.vs, {}, ps) || !m_fxaa.ps.Link())
			return;
	}

	GL_PUSH("DoFxaa");

	OMSetColorMaskState();

	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa.ps, true);
}

void GSDeviceOGL::DoExternalFX(GSTexture* sTex, GSTexture* dTex)
{
#ifndef PCSX2_CORE
	// Lazy compile
	if (!m_shaderfx.ps.IsValid())
	{
		if (!GLLoader::found_GL_ARB_gpu_shader5) // GL4.0 extension
		{
			return;
		}

		std::string config_name(theApp.GetConfigS("shaderfx_conf"));
		std::ifstream fconfig(config_name);
		std::stringstream config;
		config << "#extension GL_ARB_gpu_shader5 : require\n";
		if (fconfig.good())
			config << fconfig.rdbuf();
		else
			fprintf(stderr, "GS: External shader config '%s' not loaded.\n", config_name.c_str());

		std::string shader_name(theApp.GetConfigS("shaderfx_glsl"));
		std::ifstream fshader(shader_name);
		std::stringstream shader;
		if (!fshader.good())
		{
			fprintf(stderr, "GS: External shader '%s' not loaded and will be disabled!\n", shader_name.c_str());
			return;
		}
		shader << fshader.rdbuf();


		const std::string ps(GetShaderSource("ps_main", GL_FRAGMENT_SHADER, m_shader_common_header, shader.str(), config.str()));
		if (!m_shaderfx.ps.Compile(m_convert.vs, {}, ps) || !m_shaderfx.ps.Link())
			return;

		m_shaderfx.ps.RegisterUniform("_xyFrame");
		m_shaderfx.ps.RegisterUniform("_rcpFrame");
		m_shaderfx.ps.RegisterUniform("_rcpFrameOpt");
	}

	GL_PUSH("DoExternalFX");

	OMSetColorMaskState();

	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	m_shaderfx.ps.Bind();
	m_shaderfx.ps.Uniform2f(0, (float)s.x, (float)s.y);
	m_shaderfx.ps.Uniform4f(1, 1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	m_shaderfx.ps.Uniform4f(2, 0.0f, 0.0f, 0.0f, 0.0f);

	StretchRect(sTex, sRect, dTex, dRect, m_shaderfx.ps, true);
#endif
}

void GSDeviceOGL::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
	GL_PUSH("DoShadeBoost");

	m_shadeboost.ps.Bind();
	m_shadeboost.ps.Uniform4fv(0, params);

	OMSetColorMaskState();

	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	StretchRect(sTex, sRect, dTex, dRect, m_shadeboost.ps, true);
}

void GSDeviceOGL::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
	GL_PUSH("DATE First Pass");

	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows

	BeginScene();

	ClearStencil(ds, 0);

	m_convert.ps[static_cast<int>(datm ? ShaderConvert::DATM_1 : ShaderConvert::DATM_0)].Bind();

	// om

	OMSetDepthStencilState(m_date.dss);
	if (GLState::blend)
	{
		glDisable(GL_BLEND);
	}
	OMSetRenderTargets(NULL, ds, &GLState::scissor);

	// ia

	IASetVertexBuffer(vertices, 4);
	IASetPrimitiveTopology(GL_TRIANGLE_STRIP);


	// Texture

	PSSetShaderResource(0, rt);
	PSSetSamplerState(m_convert.pt);

	DrawPrimitive();

	if (GLState::blend)
	{
		glEnable(GL_BLEND);
	}

	EndScene();
}

void GSDeviceOGL::IASetVertexBuffer(const void* vertices, size_t count)
{
	const u32 size = static_cast<u32>(count) * sizeof(GSVertexPT1);
	auto res = m_vertex_stream_buffer->Map(sizeof(GSVertexPT1), size);
	std::memcpy(res.pointer, vertices, size);
	m_vertex.start = res.index_aligned;
	m_vertex.count = count;
	m_vertex_stream_buffer->Unmap(size);
}

void GSDeviceOGL::IASetIndexBuffer(const void* index, size_t count)
{
	const u32 size = static_cast<u32>(count) * sizeof(u32);
	auto res = m_index_stream_buffer->Map(sizeof(u32), size);
	m_index.start = res.index_aligned;
	m_index.count = count;
	std::memcpy(res.pointer, index, size);
	m_index_stream_buffer->Unmap(size);
}

void GSDeviceOGL::IASetPrimitiveTopology(GLenum topology)
{
	m_draw_topology = topology;
}

void GSDeviceOGL::PSSetShaderResource(int i, GSTexture* sr)
{
	ASSERT(i < static_cast<int>(std::size(GLState::tex_unit)));
	// Note: Nvidia debgger doesn't support the id 0 (ie the NULL texture)
	if (sr)
	{
		const GLuint id = static_cast<GSTextureOGL*>(sr)->GetID();
		if (GLState::tex_unit[i] != id)
		{
			GLState::tex_unit[i] = id;
			glBindTextureUnit(i, id);
		}
	}
}

void GSDeviceOGL::PSSetShaderResources(GSTexture* sr0, GSTexture* sr1)
{
	PSSetShaderResource(0, sr0);
	PSSetShaderResource(1, sr1);
}

void GSDeviceOGL::PSSetSamplerState(GLuint ss)
{
	if (GLState::ps_ss != ss)
	{
		GLState::ps_ss = ss;
		glBindSampler(0, ss);
	}
}

void GSDeviceOGL::ClearSamplerCache()
{
	glDeleteSamplers(std::size(m_ps_ss), m_ps_ss);

	for (u32 key = 0; key < std::size(m_ps_ss); key++)
	{
		m_ps_ss[key] = CreateSampler(PSSamplerSelector(key));
	}
}

void GSDeviceOGL::OMAttachRt(GSTextureOGL* rt)
{
	GLuint id = 0;
	if (rt)
	{
		rt->WasAttached();
		id = rt->GetID();
	}

	if (GLState::rt != id)
	{
		GLState::rt = id;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMAttachDs(GSTextureOGL* ds)
{
	GLuint id = 0;
	if (ds)
	{
		ds->WasAttached();
		id = ds->GetID();
	}

	if (GLState::ds != id)
	{
		GLState::ds = id;

		const GLenum target = GLLoader::found_framebuffer_fetch ? GL_DEPTH_ATTACHMENT : GL_DEPTH_STENCIL_ATTACHMENT;
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, target, GL_TEXTURE_2D, id, 0);
	}
}

void GSDeviceOGL::OMSetFBO(GLuint fbo)
{
	if (GLState::fbo != fbo)
	{
		GLState::fbo = fbo;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
	}
}

void GSDeviceOGL::OMSetDepthStencilState(GSDepthStencilOGL* dss)
{
	dss->SetupDepth();
	dss->SetupStencil();
}

void GSDeviceOGL::OMSetColorMaskState(OMColorMaskSelector sel)
{
	if (sel.wrgba != GLState::wrgba)
	{
		GLState::wrgba = sel.wrgba;

		glColorMaski(0, sel.wr, sel.wg, sel.wb, sel.wa);
	}
}

void GSDeviceOGL::OMSetBlendState(bool enable, GLenum src_factor, GLenum dst_factor, GLenum op, bool is_constant, u8 constant)
{
	if (enable)
	{
		if (!GLState::blend)
		{
			GLState::blend = true;
			glEnable(GL_BLEND);
		}

		if (is_constant && GLState::bf != constant)
		{
			GLState::bf = constant;
			const float bf = (float)constant / 128.0f;
			glBlendColor(bf, bf, bf, bf);
		}

		if (GLState::eq_RGB != op)
		{
			GLState::eq_RGB = op;
			glBlendEquationSeparate(op, GL_FUNC_ADD);
		}

		if (GLState::f_sRGB != src_factor || GLState::f_dRGB != dst_factor)
		{
			GLState::f_sRGB = src_factor;
			GLState::f_dRGB = dst_factor;
			glBlendFuncSeparate(src_factor, dst_factor, GL_ONE, GL_ZERO);
		}
	}
	else
	{
		if (GLState::blend)
		{
			GLState::blend = false;
			glDisable(GL_BLEND);
		}
	}
}

void GSDeviceOGL::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	GSTextureOGL* RT = static_cast<GSTextureOGL*>(rt);
	GSTextureOGL* DS = static_cast<GSTextureOGL*>(ds);

	OMSetFBO(m_fbo);
	if (rt)
	{
		OMAttachRt(RT);
	}
	else
	{
		OMAttachRt();
	}

	// Note: it must be done after OMSetFBO
	if (ds)
		OMAttachDs(DS);
	else
		OMAttachDs();

	const GSVector2i size = rt ? rt->GetSize() : ds ? ds->GetSize() : GLState::viewport;
	if (GLState::viewport != size)
	{
		GLState::viewport = size;
		// FIXME ViewportIndexedf or ViewportIndexedfv (GL4.1)
		glViewportIndexedf(0, 0, 0, GLfloat(size.x), GLfloat(size.y));
	}

	const GSVector4i r = scissor ? *scissor : GSVector4i(size).zwxy();

	if (!GLState::scissor.eq(r))
	{
		GLState::scissor = r;
		// FIXME ScissorIndexedv (GL4.1)
		glScissorIndexed(0, r.x, r.y, r.width(), r.height());
	}
}

__fi static void WriteToStreamBuffer(GL::StreamBuffer* sb, u32 index, u32 align, const void* data, u32 size)
{
	const auto res = sb->Map(align, size);
	std::memcpy(res.pointer, data, size);
	sb->Unmap(size);

	glBindBufferRange(GL_UNIFORM_BUFFER, index, sb->GetGLBufferId(), res.buffer_offset, size);
}

void GSDeviceOGL::SetupPipeline(const ProgramSelector& psel)
{
	auto it = m_programs.find(psel);
	if (it != m_programs.end())
	{
		it->second.Bind();
		return;
	}

	const std::string vs(GetVSSource(psel.vs));
	const std::string ps(GetPSSource(psel.ps));
	const std::string gs((psel.gs.key != 0) ? GetGSSource(psel.gs) : std::string());

	GL::Program prog;
	m_shader_cache.GetProgram(&prog, vs, gs, ps);
	it = m_programs.emplace(psel, std::move(prog)).first;
	it->second.Bind();
}

void GSDeviceOGL::SetupSampler(PSSamplerSelector ssel)
{
	PSSetSamplerState(m_ps_ss[ssel.key]);
}

GLuint GSDeviceOGL::GetPaletteSamplerID()
{
	return m_palette_ss;
}

void GSDeviceOGL::SetupOM(OMDepthStencilSelector dssel)
{
	OMSetDepthStencilState(m_om_dss[dssel.key]);
}

static GSDeviceOGL::VSSelector convertSel(const GSHWDrawConfig::VSSelector sel)
{
	GSDeviceOGL::VSSelector out;
	out.int_fst = !sel.fst;
	out.iip = sel.iip;
	out.point_size = sel.point_size;
	return out;
}

// clang-format off
static constexpr std::array<GLenum, 16> s_gl_blend_factors = { {
	GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
	GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_SRC1_ALPHA, GL_ONE_MINUS_SRC1_ALPHA,
	GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, GL_ONE, GL_ZERO
} };
static constexpr std::array<GLenum, 3> s_gl_blend_ops = { {
		GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT
} };
// clang-format on

void GSDeviceOGL::RenderHW(GSHWDrawConfig& config)
{
	if (!GLState::scissor.eq(config.scissor))
	{
		glScissor(config.scissor.x, config.scissor.y, config.scissor.width(), config.scissor.height());
		GLState::scissor = config.scissor;
	}

	// Destination Alpha Setup
	switch (config.destination_alpha)
	{
		case GSHWDrawConfig::DestinationAlphaMode::Off:
		case GSHWDrawConfig::DestinationAlphaMode::Full:
			break; // No setup
		case GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking:
			InitPrimDateTexture(config.rt, config.drawarea);
			break;
		case GSHWDrawConfig::DestinationAlphaMode::StencilOne:
			ClearStencil(config.ds, 1);
			break;
		case GSHWDrawConfig::DestinationAlphaMode::Stencil:
		{
			const GSVector4 src = GSVector4(config.drawarea) / GSVector4(config.ds->GetSize()).xyxy();
			const GSVector4 dst = src * 2.f - 1.f;
			GSVertexPT1 vertices[] =
			{
				{GSVector4(dst.x, dst.y, 0.0f, 0.0f), GSVector2(src.x, src.y)},
				{GSVector4(dst.z, dst.y, 0.0f, 0.0f), GSVector2(src.z, src.y)},
				{GSVector4(dst.x, dst.w, 0.0f, 0.0f), GSVector2(src.x, src.w)},
				{GSVector4(dst.z, dst.w, 0.0f, 0.0f), GSVector2(src.z, src.w)},
			};
			SetupDATE(config.rt, config.ds, vertices, config.datm);
		}
	}

	GSTexture* hdr_rt = nullptr;
	if (config.ps.hdr)
	{
		GSVector2i size = config.rt->GetSize();
		hdr_rt = CreateRenderTarget(size.x, size.y, GSTexture::Format::FloatColor, false);
		hdr_rt->CommitRegion(GSVector2i(config.drawarea.z, config.drawarea.w));
		OMSetRenderTargets(hdr_rt, config.ds, &config.scissor);

		// save blend state, since BlitRect destroys it
		const bool old_blend = GLState::blend;
		BlitRect(config.rt, config.drawarea, config.rt->GetSize(), false, false);
		if (old_blend)
		{
			GLState::blend = old_blend;
			glEnable(GL_BLEND);
		}
	}

	BeginScene();

	IASetVertexBuffer(config.verts, config.nverts);
	IASetIndexBuffer(config.indices, config.nindices);
	GLenum topology = 0;
	switch (config.topology)
	{
		case GSHWDrawConfig::Topology::Point:    topology = GL_POINTS;    break;
		case GSHWDrawConfig::Topology::Line:     topology = GL_LINES;     break;
		case GSHWDrawConfig::Topology::Triangle: topology = GL_TRIANGLES; break;
	}
	IASetPrimitiveTopology(topology);

	PSSetShaderResources(config.tex, config.pal);
	// Always bind the RT. This way special effect can use it.
	PSSetShaderResource(2, config.rt);

	SetupSampler(config.sampler);
	OMSetBlendState(config.blend.enable, s_gl_blend_factors[config.blend.src_factor],
		s_gl_blend_factors[config.blend.dst_factor], s_gl_blend_ops[config.blend.op],
		config.blend.constant_enable, config.blend.constant);
	OMSetColorMaskState(config.colormask);
	SetupOM(config.depth);

	if (m_vs_cb_cache.Update(config.cb_vs))
	{
		WriteToStreamBuffer(m_vertex_uniform_stream_buffer.get(), g_vs_cb_index,
			m_uniform_buffer_alignment, &config.cb_vs, sizeof(config.cb_vs));
	}
	if (m_ps_cb_cache.Update(config.cb_ps))
	{
		WriteToStreamBuffer(m_fragment_uniform_stream_buffer.get(), g_ps_cb_index,
			m_uniform_buffer_alignment, &config.cb_ps, sizeof(config.cb_ps));
	}

	ProgramSelector psel;
	psel.vs = convertSel(config.vs);
	psel.ps.key_hi = config.ps.key_hi;
	psel.ps.key_lo = config.ps.key_lo;
	psel.gs.key = 0;
	psel.pad = 0;
	if (config.gs.expand)
	{
		psel.gs.iip = config.gs.iip;
		switch (config.gs.topology)
		{
			case GSHWDrawConfig::GSTopology::Point:    psel.gs.point  = 1; break;
			case GSHWDrawConfig::GSTopology::Line:     psel.gs.line   = 1; break;
			case GSHWDrawConfig::GSTopology::Sprite:   psel.gs.sprite = 1; break;
			case GSHWDrawConfig::GSTopology::Triangle: ASSERT(0);          break;
		}
	}

	SetupPipeline(psel);

	// additional non-pipeline config stuff
	const bool point_size_enabled = config.vs.point_size;
	if (GLState::point_size != point_size_enabled)
	{
		if (point_size_enabled)
			glEnable(GL_PROGRAM_POINT_SIZE);
		else
			glDisable(GL_PROGRAM_POINT_SIZE);
		GLState::point_size = point_size_enabled;
	}
	const float line_width = config.line_expand ? static_cast<float>(GSConfig.UpscaleMultiplier) : 1.0f;
	if (GLState::line_width != line_width)
	{
		GLState::line_width = line_width;
		glLineWidth(line_width);
	}

	if (config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking)
	{
		GL_PUSH("Date GL42");
		// It could be good idea to use stencil in the same time.
		// Early stencil test will reduce the number of atomic-load operation

		// Create an r32i image that will contain primitive ID
		// Note: do it at the beginning because the clean will dirty the FBO state
		//dev->InitPrimDateTexture(rtsize.x, rtsize.y);

		// I don't know how much is it legal to mount rt as Texture/RT. No write is done.
		// In doubt let's detach RT.
		OMSetRenderTargets(NULL, config.ds, &config.scissor);

		// Don't write anything on the color buffer
		// Neither in the depth buffer
		glDepthMask(false);
		// Compute primitiveID max that pass the date test (Draw without barrier)
		DrawIndexedPrimitive();

		// Ask PS to discard shader above the primitiveID max
		glDepthMask(GLState::depth_mask);

		psel.ps.date = 3;
		config.alpha_second_pass.ps.date = 3;
		SetupPipeline(psel);

		// Be sure that first pass is finished !
		Barrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	OMSetRenderTargets(hdr_rt ? hdr_rt : config.rt, config.ds, &config.scissor);

	SendHWDraw(config, psel.ps.IsFeedbackLoop());

	if (config.separate_alpha_pass)
	{
		GSHWDrawConfig::BlendState dummy_bs;
		SetHWDrawConfigForAlphaPass(&psel.ps, &config.colormask, &dummy_bs, &config.depth);
		SetupPipeline(psel);
		OMSetColorMaskState(config.alpha_second_pass.colormask);
		SetupOM(config.alpha_second_pass.depth);
		OMSetBlendState();
		SendHWDraw(config, psel.ps.IsFeedbackLoop());

		// restore blend state if we're doing a second pass
		if (config.alpha_second_pass.enable)
		{
			OMSetBlendState(config.blend.enable, s_gl_blend_factors[config.blend.src_factor],
				s_gl_blend_factors[config.blend.dst_factor], s_gl_blend_ops[config.blend.op],
				config.blend.constant_enable, config.blend.constant);
		}
	}

	if (config.alpha_second_pass.enable)
	{
		// cbuffer will definitely be dirty if aref changes, no need to check it
		if (config.cb_ps.FogColor_AREF.a != config.alpha_second_pass.ps_aref)
		{
			config.cb_ps.FogColor_AREF.a = config.alpha_second_pass.ps_aref;
			WriteToStreamBuffer(m_fragment_uniform_stream_buffer.get(), g_ps_cb_index,
				m_uniform_buffer_alignment, &config.cb_ps, sizeof(config.cb_ps));
		}

		psel.ps = config.alpha_second_pass.ps;
		SetupPipeline(psel);
		OMSetColorMaskState(config.alpha_second_pass.colormask);
		SetupOM(config.alpha_second_pass.depth);
		SendHWDraw(config, psel.ps.IsFeedbackLoop());

		if (config.second_separate_alpha_pass)
		{
			GSHWDrawConfig::BlendState dummy_bs;
			SetHWDrawConfigForAlphaPass(&psel.ps, &config.colormask, &dummy_bs, &config.depth);
			SetupPipeline(psel);
			OMSetColorMaskState(config.alpha_second_pass.colormask);
			SetupOM(config.alpha_second_pass.depth);
			OMSetBlendState();
			SendHWDraw(config, psel.ps.IsFeedbackLoop());
		}
	}

	if (config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking)
		RecycleDateTexture();

	EndScene();

	// Warning: EndScene must be called before StretchRect otherwise
	// vertices will be overwritten. Trust me you don't want to do that.
	if (hdr_rt)
	{
		GSVector2i size = config.rt->GetSize();
		GSVector4 dRect(config.drawarea);
		const GSVector4 sRect = dRect / GSVector4(size.x, size.y).xyxy();
		StretchRect(hdr_rt, sRect, config.rt, dRect, ShaderConvert::MOD_256, false);

		Recycle(hdr_rt);
	}
}

void GSDeviceOGL::SendHWDraw(const GSHWDrawConfig& config, bool needs_barrier)
{
	if (config.drawlist)
	{
		GL_PUSH("Split the draw (SPRITE)");
#if defined(_DEBUG)
		// Check how draw call is split.
		std::map<size_t, size_t> frequency;
		for (const auto& it : *config.drawlist)
			++frequency[it];

		std::string message;
		for (const auto& it : frequency)
			message += " " + std::to_string(it.first) + "(" + std::to_string(it.second) + ")";

		GL_PERF("Split single draw (%d sprites) into %zu draws: consecutive draws(frequency):%s",
		        config.nindices / config.indices_per_prim, config.drawlist->size(), message.c_str());
#endif

		g_perfmon.Put(GSPerfMon::Barriers, static_cast<u32>(config.drawlist->size()));

		for (size_t count = 0, p = 0, n = 0; n < config.drawlist->size(); p += count, ++n)
		{
			count = (*config.drawlist)[n] * config.indices_per_prim;
			glTextureBarrier();
			DrawIndexedPrimitive(p, count);
		}

		return;
	}

	const bool tex_is_ds = config.tex && config.tex == config.ds;
	if (needs_barrier || tex_is_ds)
	{
		if (config.require_full_barrier)
		{
			GL_PUSH("Split the draw");

			GL_PERF("Split single draw in %d draw", config.nindices / config.indices_per_prim);
			g_perfmon.Put(GSPerfMon::Barriers, config.nindices / config.indices_per_prim);

			for (size_t p = 0; p < config.nindices; p += config.indices_per_prim)
			{
				glTextureBarrier();
				DrawIndexedPrimitive(p, config.indices_per_prim);
			}

			return;
		}

		if (config.require_one_barrier || tex_is_ds)
		{
			// The common renderer code doesn't put a barrier here because D3D/VK need to copy the DS, so we need to check it.
			// One barrier needed for non-overlapping draw.
			g_perfmon.Put(GSPerfMon::Barriers, 1);
			glTextureBarrier();
			DrawIndexedPrimitive();
			return;
		}
	}

	// No barriers needed
	DrawIndexedPrimitive();
}

// Note: used as a callback of DebugMessageCallback. Don't change the signature
void GSDeviceOGL::DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar* gl_message, const void* userParam)
{
	std::string message(gl_message, gl_length >= 0 ? gl_length : strlen(gl_message));
	std::string type, severity, source;
	static int sev_counter = 0;
	switch (gl_type)
	{
		case GL_DEBUG_TYPE_ERROR_ARB               : type = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB : type = "Deprecated bhv"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB  : type = "Undefined bhv"; break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB         : type = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB         : type = "Perf"; break;
		case GL_DEBUG_TYPE_OTHER_ARB               : type = "Oth"; break;
		case GL_DEBUG_TYPE_PUSH_GROUP              : return; // Don't print message injected by myself
		case GL_DEBUG_TYPE_POP_GROUP               : return; // Don't print message injected by myself
		default                                    : type = "TTT"; break;
	}
	switch (gl_severity)
	{
		case GL_DEBUG_SEVERITY_HIGH_ARB   : severity = "High"; sev_counter++; break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB : severity = "Mid"; break;
		case GL_DEBUG_SEVERITY_LOW_ARB    : severity = "Low"; break;
		default:
			if (id == 0xFEAD)
				severity = "Cache";
			else if (id == 0xB0B0)
				severity = "REG";
			else if (id == 0xD0D0)
				severity = "EXTRA";
			break;
	}
	switch (gl_source)
	{
		case GL_DEBUG_SOURCE_API_ARB             : source = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB   : source = "WINDOW"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB : source = "COMPILER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB     : source = "3rdparty"; break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB     : source = "Application"; break;
		case GL_DEBUG_SOURCE_OTHER_ARB           : source = "Others"; break;
		default                                  : source = "???"; break;
	}

#ifdef _DEBUG
	// Don't spam noisy information on the terminal
	if (gl_severity != GL_DEBUG_SEVERITY_NOTIFICATION)
	{
		Console.Error("T:%s\tID:%d\tS:%s\t=> %s", type.c_str(), GSState::s_n, severity.c_str(), message.c_str());
	}
#else
	// Print nouveau shader compiler info
	if (GSState::s_n == 0)
	{
		int t, local, gpr, inst, byte;
		const int status = sscanf(message.c_str(), "type: %d, local: %d, gpr: %d, inst: %d, bytes: %d",
			&t, &local, &gpr, &inst, &byte);
		if (status == 5)
		{
			m_shader_inst += inst;
			m_shader_reg += gpr;
			fprintf(stderr, "T:%s\t\tS:%s\t=> %s\n", type.c_str(), severity.c_str(), message.c_str());
		}
	}
#endif

#ifdef ENABLE_OGL_DEBUG
	if (m_debug_gl_file)
		fprintf(m_debug_gl_file, "T:%s\tID:%d\tS:%s\t=> %s\n", type.c_str(), GSState::s_n, severity.c_str(), message.c_str());

	if (sev_counter >= 5)
	{
		// Close the file to flush the content on disk before exiting.
		if (m_debug_gl_file)
		{
			fclose(m_debug_gl_file);
			m_debug_gl_file = NULL;
		}
		ASSERT(0);
	}
#endif
}

void GSDeviceOGL::PushDebugGroup(const char* fmt, ...)
{
#ifdef ENABLE_OGL_DEBUG
	if (!glPushDebugGroup)
		return;

	std::va_list ap;
	va_start(ap, fmt);
	const std::string buf(StringUtil::StdStringFromFormatV(fmt, ap));
	va_end(ap);
	if (!buf.empty())
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0xBAD, -1, buf.c_str());
#endif
}

void GSDeviceOGL::PopDebugGroup()
{
#ifdef ENABLE_OGL_DEBUG
	if (!glPopDebugGroup)
		return;
	
	glPopDebugGroup();
#endif
}

void GSDeviceOGL::InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...)
{
#ifdef ENABLE_OGL_DEBUG
	if (!glDebugMessageInsert)
		return;

	GLenum type, id, severity;
	switch (category)
	{
	case GSDevice::DebugMessageCategory::Cache:
			type = GL_DEBUG_TYPE_OTHER;
			id = 0xFEAD;
			severity = GL_DEBUG_SEVERITY_NOTIFICATION;
		break;
	case GSDevice::DebugMessageCategory::Reg:
		type = GL_DEBUG_TYPE_OTHER;
		id = 0xB0B0;
		severity = GL_DEBUG_SEVERITY_NOTIFICATION;
		break;
	case GSDevice::DebugMessageCategory::Debug:
		type = GL_DEBUG_TYPE_OTHER;
		id = 0xD0D0;
		severity = GL_DEBUG_SEVERITY_NOTIFICATION;
		break;
	case GSDevice::DebugMessageCategory::Message:
		type = GL_DEBUG_TYPE_ERROR;
		id = 0xDEAD;
		severity = GL_DEBUG_SEVERITY_MEDIUM;
		break;
	case GSDevice::DebugMessageCategory::Performance:		
	default:
		type = GL_DEBUG_TYPE_PERFORMANCE;
		id = 0xFEE1;
		severity = GL_DEBUG_SEVERITY_NOTIFICATION;
		break;
	}

	std::va_list ap;
	va_start(ap, fmt);
	const std::string buf(StringUtil::StdStringFromFormatV(fmt, ap));
	va_end(ap);
	if (!buf.empty())
		glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, type, id, severity, buf.size(), buf.c_str());
#endif
}
