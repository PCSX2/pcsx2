/**********************************************************************************
 * Copyright (c) 2008-2013 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 **********************************************************************************/

/* $Revision: 11708 $ on $Date: 2010-06-13 23:36:24 -0700 (Sun, 13 Jun 2010) $ */

#ifndef __OPENCL_CL_D3D9_H
#define __OPENCL_CL_D3D9_H

#include <CL/cl.h>
#include <CL/cl_platform.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/* cl_khr_dx9_media_sharing                                                   */
#define cl_khr_dx9_media_sharing 1

typedef cl_uint             cl_dx9_media_adapter_type_khr;
typedef cl_uint             cl_dx9_media_adapter_set_khr;
    
#if defined(_WIN32)
#include <d3d9.h>
typedef struct _cl_dx9_surface_info_khr
{
    IDirect3DSurface9 *resource;
    HANDLE shared_handle;
} cl_dx9_surface_info_khr;
#endif


/******************************************************************************/

/* Error Codes */
#define CL_INVALID_DX9_MEDIA_ADAPTER_KHR                -1010
#define CL_INVALID_DX9_MEDIA_SURFACE_KHR                -1011
#define CL_DX9_MEDIA_SURFACE_ALREADY_ACQUIRED_KHR       -1012
#define CL_DX9_MEDIA_SURFACE_NOT_ACQUIRED_KHR           -1013

/* cl_media_adapter_type_khr */
#define CL_ADAPTER_D3D9_KHR                              0x2020
#define CL_ADAPTER_D3D9EX_KHR                            0x2021
#define CL_ADAPTER_DXVA_KHR                              0x2022

/* cl_media_adapter_set_khr */
#define CL_PREFERRED_DEVICES_FOR_DX9_MEDIA_ADAPTER_KHR   0x2023
#define CL_ALL_DEVICES_FOR_DX9_MEDIA_ADAPTER_KHR         0x2024

/* cl_context_info */
#define CL_CONTEXT_ADAPTER_D3D9_KHR                      0x2025
#define CL_CONTEXT_ADAPTER_D3D9EX_KHR                    0x2026
#define CL_CONTEXT_ADAPTER_DXVA_KHR                      0x2027

/* cl_mem_info */
#define CL_MEM_DX9_MEDIA_ADAPTER_TYPE_KHR                0x2028
#define CL_MEM_DX9_MEDIA_SURFACE_INFO_KHR                0x2029

/* cl_image_info */
#define CL_IMAGE_DX9_MEDIA_PLANE_KHR                     0x202A

/* cl_command_type */
#define CL_COMMAND_ACQUIRE_DX9_MEDIA_SURFACES_KHR        0x202B
#define CL_COMMAND_RELEASE_DX9_MEDIA_SURFACES_KHR        0x202C

/******************************************************************************/

typedef CL_API_ENTRY cl_int (CL_API_CALL *clGetDeviceIDsFromDX9MediaAdapterKHR_fn)(
    cl_platform_id                   platform,
    cl_uint                          num_media_adapters,
    cl_dx9_media_adapter_type_khr *  media_adapter_type,
    void *                           media_adapters[],
    cl_dx9_media_adapter_set_khr     media_adapter_set,
    cl_uint                          num_entries,
    cl_device_id *                   devices,
    cl_uint *                        num_devices) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromDX9MediaSurfaceKHR_fn)(
    cl_context                    context,
    cl_mem_flags                  flags,
    cl_dx9_media_adapter_type_khr adapter_type,
    void *                        surface_info,
    cl_uint                       plane,                                                                          
    cl_int *                      errcode_ret) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int (CL_API_CALL *clEnqueueAcquireDX9MediaSurfacesKHR_fn)(
    cl_command_queue command_queue,
    cl_uint          num_objects,
    const cl_mem *   mem_objects,
    cl_uint          num_events_in_wait_list,
    const cl_event * event_wait_list,
    cl_event *       event) CL_API_SUFFIX__VERSION_1_2;

typedef CL_API_ENTRY cl_int (CL_API_CALL *clEnqueueReleaseDX9MediaSurfacesKHR_fn)(
    cl_command_queue command_queue,
    cl_uint          num_objects,
    const cl_mem *   mem_objects,
    cl_uint          num_events_in_wait_list,
    const cl_event * event_wait_list,
    cl_event *       event) CL_API_SUFFIX__VERSION_1_2;

#if defined CL_DX9_MEDIA_SHARING_INTEL_EXT

#ifndef _WIN32
#include <d3d9.h>
#endif
#include <d3d9.h>
#include <dxvahd.h>
#include <wtypes.h>
#include <d3d9types.h>
 
/******************************************************************************/    
/* cl_intel_dx9_media_sharing extension                                       */
#define cl_intel_dx9_media_sharing 1

typedef cl_uint cl_dx9_device_source_intel;
typedef cl_uint cl_dx9_device_set_intel;

/******************************************************************************/

// Error Codes
#define CL_INVALID_DX9_DEVICE_INTEL                   -1010
#define CL_INVALID_DX9_RESOURCE_INTEL                 -1011
#define CL_DX9_RESOURCE_ALREADY_ACQUIRED_INTEL        -1012
#define CL_DX9_RESOURCE_NOT_ACQUIRED_INTEL            -1013

// cl_dx9_device_source_intel
#define CL_D3D9_DEVICE_INTEL                          0x4022
#define CL_D3D9EX_DEVICE_INTEL                        0x4070
#define CL_DXVA_DEVICE_INTEL                          0x4071

// cl_dx9_device_set_intel
#define CL_PREFERRED_DEVICES_FOR_DX9_INTEL            0x4024
#define CL_ALL_DEVICES_FOR_DX9_INTEL                  0x4025

// cl_context_info
#define CL_CONTEXT_D3D9_DEVICE_INTEL                  0x4026
#define CL_CONTEXT_D3D9EX_DEVICE_INTEL                0x4072
#define CL_CONTEXT_DXVA_DEVICE_INTEL                  0x4073

// cl_mem_info
#define CL_MEM_DX9_RESOURCE_INTEL                     0x4027
#define CL_MEM_DX9_SHARED_HANDLE_INTEL                0x4074

// cl_image_info
#define CL_IMAGE_DX9_PLANE_INTEL                      0x4075

// cl_command_type
#define CL_COMMAND_ACQUIRE_DX9_OBJECTS_INTEL          0x402A
#define CL_COMMAND_RELEASE_DX9_OBJECTS_INTEL          0x402B

//packed YUV channel order
#define CL_YUYV_INTEL                                 0x4076
#define CL_UYVY_INTEL                                 0x4077
#define CL_YVYU_INTEL                                 0x4078
#define CL_VYUY_INTEL                                 0x4079

/******************************************************************************/

typedef CL_API_ENTRY cl_int (CL_API_CALL* clGetDeviceIDsFromDX9INTEL_fn)(
    cl_platform_id              /*platform*/,
    cl_dx9_device_source_intel  /*dx9_device_source*/,
    void*                       /*dx9_object*/,
    cl_dx9_device_set_intel     /*dx9_device_set*/,
    cl_uint                     /*num_entries*/, 
    cl_device_id*               /*devices*/, 
    cl_uint*                    /*num_devices*/);
    
typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromDX9MediaSurfaceINTEL_fn)(
    cl_context                  /*context*/,
    cl_mem_flags                /*flags*/,
    IDirect3DSurface9 *         /*resource*/,
    HANDLE                      /*sharedHandle*/,
    UINT                        /*plane*/,
    cl_int *                    /*errcode_ret*/);
    
typedef CL_API_ENTRY cl_int (CL_API_CALL *clEnqueueAcquireDX9ObjectsINTEL_fn)(
    cl_command_queue            /*command_queue*/,
    cl_uint                     /*num_objects*/,
    const cl_mem *              /*mem_objects*/,
    cl_uint                     /*num_events_in_wait_list*/,
    const cl_event *            /*event_wait_list*/,
    cl_event *                  /*event*/);
    
typedef CL_API_ENTRY cl_int (CL_API_CALL *clEnqueueReleaseDX9ObjectsINTEL_fn)(
    cl_command_queue            /*command_queue*/,
    cl_uint                     /*num_objects*/,
    cl_mem *                    /*mem_objects*/,
    cl_uint                     /*num_events_in_wait_list*/,
    const cl_event *            /*event_wait_list*/,
    cl_event *                  /*event*/);

#endif // CL_DX9_MEDIA_SHARING_INTEL_EXT

#if defined CL_DX9_MEDIA_SHARING_NV_EXT

#ifndef _WIN32
#include <d3d9.h>
#endif
    
/******************************************************************************
 * cl_nv_d3d9_sharing                                                         */

typedef cl_uint cl_d3d9_device_source_nv;
typedef cl_uint cl_d3d9_device_set_nv;

/******************************************************************************/

// Error Codes
#define CL_INVALID_D3D9_DEVICE_NV              -1010
#define CL_INVALID_D3D9_RESOURCE_NV            -1011
#define CL_D3D9_RESOURCE_ALREADY_ACQUIRED_NV   -1012
#define CL_D3D9_RESOURCE_NOT_ACQUIRED_NV       -1013

// cl_d3d9_device_source_nv
#define CL_D3D9_DEVICE_NV                      0x4022
#define CL_D3D9_ADAPTER_NAME_NV                0x4023

// cl_d3d9_device_set_nv
#define CL_PREFERRED_DEVICES_FOR_D3D9_NV       0x4024
#define CL_ALL_DEVICES_FOR_D3D9_NV             0x4025

// cl_context_info
#define CL_CONTEXT_D3D9_DEVICE_NV              0x4026

// cl_mem_info
#define CL_MEM_D3D9_RESOURCE_NV                0x4027

// cl_image_info
#define CL_IMAGE_D3D9_FACE_NV                  0x4028
#define CL_IMAGE_D3D9_LEVEL_NV                 0x4029

// cl_command_type
#define CL_COMMAND_ACQUIRE_D3D9_OBJECTS_NV     0x402A
#define CL_COMMAND_RELEASE_D3D9_OBJECTS_NV     0x402B

/******************************************************************************/

typedef CL_API_ENTRY cl_int (CL_API_CALL *clGetDeviceIDsFromD3D9NV_fn)(
    cl_platform_id            platform,
    cl_d3d9_device_source_nv  d3d_device_source,
    void *                    d3d_object,
    cl_d3d9_device_set_nv     d3d_device_set,
    cl_uint                   num_entries, 
    cl_device_id *            devices, 
    cl_uint *                 num_devices) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromD3D9VertexBufferNV_fn)(
    cl_context               context,
    cl_mem_flags             flags,
    IDirect3DVertexBuffer9 * resource,
    cl_int *                 errcode_ret) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromD3D9IndexBufferNV_fn)(
    cl_context              context,
    cl_mem_flags            flags,
    IDirect3DIndexBuffer9 * resource,
    cl_int *                errcode_ret) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromD3D9SurfaceNV_fn)(
    cl_context          context,
    cl_mem_flags        flags,
    IDirect3DSurface9 * resource,
    cl_int *            errcode_ret) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromD3D9TextureNV_fn)(
    cl_context         context,
    cl_mem_flags       flags,
    IDirect3DTexture9 *resource,
    UINT               miplevel,
    cl_int *           errcode_ret) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromD3D9CubeTextureNV_fn)(
    cl_context              context,
    cl_mem_flags            flags,
    IDirect3DCubeTexture9 * resource,
    D3DCUBEMAP_FACES        facetype,
    UINT                    miplevel,
    cl_int *                errcode_ret) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_mem (CL_API_CALL *clCreateFromD3D9VolumeTextureNV_fn)(
    cl_context                context,
    cl_mem_flags              flags,
    IDirect3DVolumeTexture9 * resource,
    UINT                      miplevel,
    cl_int *                  errcode_ret) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *clEnqueueAcquireD3D9ObjectsNV_fn)(
    cl_command_queue command_queue,
    cl_uint num_objects,
    const cl_mem *mem_objects,
    cl_uint num_events_in_wait_list,
    const cl_event *event_wait_list,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

typedef CL_API_ENTRY cl_int (CL_API_CALL *clEnqueueReleaseD3D9ObjectsNV_fn)(
    cl_command_queue command_queue,
    cl_uint num_objects,
    cl_mem *mem_objects,
    cl_uint num_events_in_wait_list,
    const cl_event *event_wait_list,
    cl_event *event) CL_API_SUFFIX__VERSION_1_0;

#endif // CL_DX9_MEDIA_SHARING_NV_EXT

#ifdef __cplusplus
}
#endif

#endif  /* __OPENCL_CL_DX9_MEDIA_SHARING_H */

