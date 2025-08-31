// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSOpenCLFogProcessor.h"
#include "common/Console.h"
#include <cstring>
#include <vector>

// OpenCL kernel source for PS2-accurate fog calculation
static const char* FOG_KERNEL_SOURCE = R"(
__kernel void apply_ps2_fog(__global uchar4* input,
                           __global uchar4* output,
                           __constant float4* fog_params,
                           int width, int height)
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    
    if (x >= width || y >= height)
        return;
        
    int idx = y * width + x;
    uchar4 pixel = input[idx];
    
    // Extract fog parameters
    float3 fog_color = fog_params[0].xyz;
    float fog_factor = fog_params[1].x;
    float inv_fog_factor = fog_params[1].y;
    
    // Convert pixel to float for calculation
    float3 color = convert_float3(pixel.xyz);
    
    // Apply PS2 hardware-accurate fog calculation: (Color * Fog + FogColor * (256 - Fog)) >> 8
    // Using precise float calculation as per CEO's instructions for better accuracy
    float3 result = trunc((color * fog_factor + fog_color * inv_fog_factor) / 256.0f);
    
    // Convert back to uchar and write output
    output[idx] = (uchar4)(convert_uchar3_sat(result), pixel.w);
}
)";

GSOpenCLFogProcessor::GSOpenCLFogProcessor() = default;

GSOpenCLFogProcessor::~GSOpenCLFogProcessor()
{
	Destroy();
}

bool GSOpenCLFogProcessor::Initialize()
{
	if (m_initialized)
		return true;
		
	Console.WriteLn("GSOpenCLFogProcessor: Initializing OpenCL fog processing pipeline...");
	
	if (!InitializeOpenCL())
	{
		Console.Error("GSOpenCLFogProcessor: Failed to initialize OpenCL context");
		return false;
	}
	
	if (!CreateKernel())
	{
		Console.Error("GSOpenCLFogProcessor: Failed to create fog kernel");
		return false;
	}
	
	m_initialized = true;
	Console.WriteLn("GSOpenCLFogProcessor: OpenCL fog pipeline initialized successfully! 📈📈📈");
	return true;
}

bool GSOpenCLFogProcessor::InitializeOpenCL()
{
	cl_int err;
	
	// Get platform
	cl_platform_id platform;
	cl_uint num_platforms;
	err = clGetPlatformIDs(1, &platform, &num_platforms);
	if (err != CL_SUCCESS || num_platforms == 0)
	{
		Console.Error("GSOpenCLFogProcessor: No OpenCL platforms found");
		return false;
	}
	
	// Get device (prefer GPU)
	cl_device_id device;
	cl_uint num_devices;
	err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, &num_devices);
	if (err != CL_SUCCESS || num_devices == 0)
	{
		// Fallback to CPU
		err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, &num_devices);
		if (err != CL_SUCCESS || num_devices == 0)
		{
			Console.Error("GSOpenCLFogProcessor: No OpenCL devices found");
			return false;
		}
	}
	
	// Create context
	m_context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
	if (err != CL_SUCCESS)
	{
		Console.Error("GSOpenCLFogProcessor: Failed to create OpenCL context (%d)", err);
		return false;
	}
	
	// Create command queue  
	m_queue = clCreateCommandQueue(m_context, device, 0, &err);
	if (err != CL_SUCCESS)
	{
		Console.Error("GSOpenCLFogProcessor: Failed to create command queue (%d)", err);
		return false;
	}
	
	Console.WriteLn("GSOpenCLFogProcessor: OpenCL context created successfully");
	return true;
}

bool GSOpenCLFogProcessor::CreateKernel()
{
	cl_int err;
	
	// Create program from source
	size_t source_length = strlen(FOG_KERNEL_SOURCE);
	m_program = clCreateProgramWithSource(m_context, 1, &FOG_KERNEL_SOURCE, &source_length, &err);
	if (err != CL_SUCCESS)
	{
		Console.Error("GSOpenCLFogProcessor: Failed to create program (%d)", err);
		return false;
	}
	
	// Build program
	cl_device_id device;
	err = clGetContextInfo(m_context, CL_CONTEXT_DEVICES, sizeof(device), &device, nullptr);
	if (err != CL_SUCCESS)
	{
		Console.Error("GSOpenCLFogProcessor: Failed to get device (%d)", err);
		return false;
	}
	
	err = clBuildProgram(m_program, 1, &device, nullptr, nullptr, nullptr);
	if (err != CL_SUCCESS)
	{
		size_t log_size;
		clGetProgramBuildInfo(m_program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
		
		std::vector<char> build_log(log_size);
		clGetProgramBuildInfo(m_program, device, CL_PROGRAM_BUILD_LOG, log_size, build_log.data(), nullptr);
		
		Console.Error("GSOpenCLFogProcessor: Build failed:\n%s", build_log.data());
		return false;
	}
	
	// Create kernel
	m_fog_kernel = clCreateKernel(m_program, "apply_ps2_fog", &err);
	if (err != CL_SUCCESS)
	{
		Console.Error("GSOpenCLFogProcessor: Failed to create kernel (%d)", err);
		return false;
	}
	
	Console.WriteLn("GSOpenCLFogProcessor: Fog kernel compiled successfully");
	return true;
}

bool GSOpenCLFogProcessor::ProcessFog(void* input_texture, void* output_texture,
                                     const GSVector4& fog_color, float fog_factor,
                                     int width, int height)
{
	if (!m_initialized)
		return false;
		
	cl_int err;
	size_t texture_size = width * height * 4; // RGBA pixels
	
	// Create buffers if needed
	if (texture_size != m_buffer_size)
	{
		if (m_input_buffer) clReleaseMemObject(m_input_buffer);
		if (m_output_buffer) clReleaseMemObject(m_output_buffer);
		if (m_fog_params_buffer) clReleaseMemObject(m_fog_params_buffer);
		
		m_input_buffer = clCreateBuffer(m_context, CL_MEM_READ_ONLY, texture_size, nullptr, &err);
		if (err != CL_SUCCESS) return false;
		
		m_output_buffer = clCreateBuffer(m_context, CL_MEM_WRITE_ONLY, texture_size, nullptr, &err);
		if (err != CL_SUCCESS) return false;
		
		m_fog_params_buffer = clCreateBuffer(m_context, CL_MEM_READ_ONLY, sizeof(FogParams), nullptr, &err);
		if (err != CL_SUCCESS) return false;
		
		m_buffer_size = texture_size;
	}
	
	// Prepare fog parameters with PS2-accurate calculation
	FogParams params;
	params.fog_color = fog_color * 255.0f; // Convert to 0-255 range
	params.fog_factor = fog_factor;
	params.inv_fog_factor = 256.0f - fog_factor;
	
	// Upload data to OpenCL buffers
	err = clEnqueueWriteBuffer(m_queue, m_input_buffer, CL_TRUE, 0, texture_size, input_texture, 0, nullptr, nullptr);
	if (err != CL_SUCCESS) return false;
	
	err = clEnqueueWriteBuffer(m_queue, m_fog_params_buffer, CL_TRUE, 0, sizeof(FogParams), &params, 0, nullptr, nullptr);
	if (err != CL_SUCCESS) return false;
	
	// Set kernel arguments
	err = clSetKernelArg(m_fog_kernel, 0, sizeof(cl_mem), &m_input_buffer);
	if (err != CL_SUCCESS) return false;
	err = clSetKernelArg(m_fog_kernel, 1, sizeof(cl_mem), &m_output_buffer);
	if (err != CL_SUCCESS) return false;
	err = clSetKernelArg(m_fog_kernel, 2, sizeof(cl_mem), &m_fog_params_buffer);
	if (err != CL_SUCCESS) return false;
	err = clSetKernelArg(m_fog_kernel, 3, sizeof(int), &width);
	if (err != CL_SUCCESS) return false;
	err = clSetKernelArg(m_fog_kernel, 4, sizeof(int), &height);
	if (err != CL_SUCCESS) return false;
	
	// Execute kernel
	size_t global_size[2] = {static_cast<size_t>(width), static_cast<size_t>(height)};
	err = clEnqueueNDRangeKernel(m_queue, m_fog_kernel, 2, nullptr, global_size, nullptr, 0, nullptr, nullptr);
	if (err != CL_SUCCESS) return false;
	
	// Read back result
	err = clEnqueueReadBuffer(m_queue, m_output_buffer, CL_TRUE, 0, texture_size, output_texture, 0, nullptr, nullptr);
	if (err != CL_SUCCESS) return false;
	
	clFinish(m_queue);
	return true;
}

// Global OpenCL fog enable state
static bool s_opencl_fog_enabled = true; // Default enabled as per CEO instructions

bool GSOpenCLFogProcessor::IsGloballyEnabled()
{
	return s_opencl_fog_enabled;
}

void GSOpenCLFogProcessor::SetGloballyEnabled(bool enabled)
{
	s_opencl_fog_enabled = enabled;
}

void GSOpenCLFogProcessor::Destroy()
{
	if (m_initialized)
	{
		if (m_input_buffer) clReleaseMemObject(m_input_buffer);
		if (m_output_buffer) clReleaseMemObject(m_output_buffer);
		if (m_fog_params_buffer) clReleaseMemObject(m_fog_params_buffer);
		if (m_fog_kernel) clReleaseKernel(m_fog_kernel);
		if (m_program) clReleaseProgram(m_program);
		if (m_queue) clReleaseCommandQueue(m_queue);
		if (m_context) clReleaseContext(m_context);
		
		m_input_buffer = nullptr;
		m_output_buffer = nullptr;
		m_fog_params_buffer = nullptr;
		m_fog_kernel = nullptr;
		m_program = nullptr;
		m_queue = nullptr;
		m_context = nullptr;
		
		m_initialized = false;
		m_buffer_size = 0;
		Console.WriteLn("GSOpenCLFogProcessor: OpenCL fog pipeline destroyed");
	}
}