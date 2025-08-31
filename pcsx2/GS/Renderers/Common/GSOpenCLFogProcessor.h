// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#ifdef USE_OPENCL_FOG

#include "GS/GSVector.h"
#include <CL/opencl.h>
#include <memory>

/// OpenCL-based fog processor for PS2-accurate fog rendering
/// Implements post-processing fog calculation with high precision floats
class GSOpenCLFogProcessor
{
private:
	cl_context m_context = nullptr;
	cl_command_queue m_queue = nullptr;
	cl_program m_program = nullptr;
	cl_kernel m_fog_kernel = nullptr;
	cl_mem m_input_buffer = nullptr;
	cl_mem m_output_buffer = nullptr;
	cl_mem m_fog_params_buffer = nullptr;
	
	bool m_initialized = false;
	size_t m_buffer_size = 0;
	
	struct FogParams
	{
		alignas(16) GSVector4 fog_color;
		float fog_factor;
		float inv_fog_factor;
		float _pad1, _pad2;
	};

	bool InitializeOpenCL();
	bool CreateKernel();
	
public:
	GSOpenCLFogProcessor();
	~GSOpenCLFogProcessor();
	
	/// Initialize the OpenCL fog processor
	bool Initialize();
	
	/// Process fog on rendered primitives
	/// @param input_texture Rendered texture without fog
	/// @param output_texture Output texture with fog applied  
	/// @param fog_color RGB fog color from PS2 FOGCOL register
	/// @param fog_factor Fog intensity from primitive (0-255 range)
	/// @param width Texture width
	/// @param height Texture height
	bool ProcessFog(void* input_texture, void* output_texture, 
	               const GSVector4& fog_color, float fog_factor,
	               int width, int height);
	
	/// Check if OpenCL fog processing is available
	bool IsAvailable() const { return m_initialized; }
	
	/// Get global OpenCL fog enable state
	static bool IsGloballyEnabled();
	
	/// Set global OpenCL fog enable state
	static void SetGloballyEnabled(bool enabled);
	
	/// Cleanup OpenCL resources
	void Destroy();
};

#endif // USE_OPENCL_FOG