#include "CL/cl.h"

typedef cl_int (CL_API_CALL * clGetPlatformIDsPtr)(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetPlatformInfoPtr)(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetDeviceIDsPtr)(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id* devices, cl_uint* num_devices) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetDeviceInfoPtr)(cl_device_id device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clCreateSubDevicesPtr)(cl_device_id in_device, const cl_device_partition_property* properties, cl_uint num_devices, cl_device_id* out_devices, cl_uint* num_devices_ret) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clRetainDevicePtr)(cl_device_id device) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clReleaseDevicePtr)(cl_device_id device) CL_API_SUFFIX__VERSION_1_2;
typedef cl_context (CL_API_CALL * clCreateContextPtr)(const cl_context_properties* properties, cl_uint num_devices, const cl_device_id* devices, void (CL_CALLBACK* pfn_notify)(const char*, const void*, size_t, void*), void* user_data, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_context (CL_API_CALL * clCreateContextFromTypePtr)(const cl_context_properties* properties, cl_device_type device_type, void (CL_CALLBACK* pfn_notify)(const char*, const void*, size_t, void*), void* user_data, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clRetainContextPtr)(cl_context context) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clReleaseContextPtr)(cl_context context) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetContextInfoPtr)(cl_context context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_command_queue (CL_API_CALL * clCreateCommandQueueWithPropertiesPtr)(cl_context context, cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clRetainCommandQueuePtr)(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clReleaseCommandQueuePtr)(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetCommandQueueInfoPtr)(cl_command_queue command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_mem (CL_API_CALL * clCreateBufferPtr)(cl_context context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_mem (CL_API_CALL * clCreateSubBufferPtr)(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const void* buffer_create_info, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_1;
typedef cl_mem (CL_API_CALL * clCreateImagePtr)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2;
typedef cl_mem (CL_API_CALL * clCreatePipePtr)(cl_context context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const cl_pipe_properties* properties, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clRetainMemObjectPtr)(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clReleaseMemObjectPtr)(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetSupportedImageFormatsPtr)(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetMemObjectInfoPtr)(cl_mem memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetImageInfoPtr)(cl_mem image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetPipeInfoPtr)(cl_mem pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clSetMemObjectDestructorCallbackPtr)(cl_mem memobj, void (CL_CALLBACK* pfn_notify)(cl_mem memobj, void* user_data), void* user_data) CL_API_SUFFIX__VERSION_1_1;
typedef void* (CL_API_CALL * clSVMAllocPtr)(cl_context context, cl_svm_mem_flags flags, size_t size, cl_uint alignment) CL_API_SUFFIX__VERSION_2_0;
typedef void (CL_API_CALL * clSVMFreePtr)(cl_context context, void* svm_pointer) CL_API_SUFFIX__VERSION_2_0;
typedef cl_sampler (CL_API_CALL * clCreateSamplerWithPropertiesPtr)(cl_context context, const cl_sampler_properties* normalized_coords, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clRetainSamplerPtr)(cl_sampler sampler) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clReleaseSamplerPtr)(cl_sampler sampler) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetSamplerInfoPtr)(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_program (CL_API_CALL * clCreateProgramWithSourcePtr)(cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_program (CL_API_CALL * clCreateProgramWithBinaryPtr)(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_program (CL_API_CALL * clCreateProgramWithBuiltInKernelsPtr)(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clRetainProgramPtr)(cl_program program) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clReleaseProgramPtr)(cl_program program) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clBuildProgramPtr)(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clCompileProgramPtr)(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_headers, const cl_program* input_headers, const char** header_include_names, void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data) CL_API_SUFFIX__VERSION_1_2;
typedef cl_program (CL_API_CALL * clLinkProgramPtr)(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_programs, const cl_program* input_programs, void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clUnloadPlatformCompilerPtr)(cl_platform_id platform) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clGetProgramInfoPtr)(cl_program program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetProgramBuildInfoPtr)(cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_kernel (CL_API_CALL * clCreateKernelPtr)(cl_program program, const char* kernel_name, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clCreateKernelsInProgramPtr)(cl_program program, cl_uint num_kernels, cl_kernel* kernels, cl_uint* num_kernels_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clRetainKernelPtr)(cl_kernel kernel) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clReleaseKernelPtr)(cl_kernel kernel) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clSetKernelArgPtr)(cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clSetKernelArgSVMPointerPtr)(cl_kernel kernel, cl_uint arg_index, const void* arg_value) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clSetKernelExecInfoPtr)(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clGetKernelInfoPtr)(cl_kernel kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetKernelArgInfoPtr)(cl_kernel kernel, cl_uint arg_indx, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clGetKernelWorkGroupInfoPtr)(cl_kernel kernel, cl_device_id device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clWaitForEventsPtr)(cl_uint num_events, const cl_event* event_list) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clGetEventInfoPtr)(cl_event event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_event (CL_API_CALL * clCreateUserEventPtr)(cl_context context, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_1;
typedef cl_int (CL_API_CALL * clRetainEventPtr)(cl_event event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clReleaseEventPtr)(cl_event event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clSetUserEventStatusPtr)(cl_event event, cl_int execution_status) CL_API_SUFFIX__VERSION_1_1;
typedef cl_int (CL_API_CALL * clSetEventCallbackPtr)(cl_event event, cl_int command_exec_callback_type, void (CL_CALLBACK* pfn_notify)(cl_event, cl_int, void*), void* user_data) CL_API_SUFFIX__VERSION_1_1;
typedef cl_int (CL_API_CALL * clGetEventProfilingInfoPtr)(cl_event event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clFlushPtr)(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clFinishPtr)(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueReadBufferPtr)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueReadBufferRectPtr)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, const size_t* buffer_offset, const size_t* host_offset, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_1;
typedef cl_int (CL_API_CALL * clEnqueueWriteBufferPtr)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueWriteBufferRectPtr)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, const size_t* buffer_offset, const size_t* host_offset, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_1;
typedef cl_int (CL_API_CALL * clEnqueueFillBufferPtr)(cl_command_queue command_queue, cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clEnqueueCopyBufferPtr)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueCopyBufferRectPtr)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_1;
typedef cl_int (CL_API_CALL * clEnqueueReadImagePtr)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueWriteImagePtr)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueFillImagePtr)(cl_command_queue command_queue, cl_mem image, const void* fill_color, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clEnqueueCopyImagePtr)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueCopyImageToBufferPtr)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueCopyBufferToImagePtr)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef void* (CL_API_CALL * clEnqueueMapBufferPtr)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map, cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef void* (CL_API_CALL * clEnqueueMapImagePtr)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_map, cl_map_flags map_flags, const size_t* origin, const size_t* region, size_t* image_row_pitch, size_t* image_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueUnmapMemObjectPtr)(cl_command_queue command_queue, cl_mem memobj, void* mapped_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueMigrateMemObjectsPtr)(cl_command_queue command_queue, cl_uint num_mem_objects, const cl_mem* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clEnqueueNDRangeKernelPtr)(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueNativeKernelPtr)(cl_command_queue command_queue, void (CL_CALLBACK* /*user_func*/)(void*), void* args, size_t cb_args, cl_uint num_mem_objects, const cl_mem* mem_list, const void** args_mem_loc, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0;
typedef cl_int (CL_API_CALL * clEnqueueMarkerWithWaitListPtr)(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clEnqueueBarrierWithWaitListPtr)(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2;
typedef cl_int (CL_API_CALL * clEnqueueSVMFreePtr)(cl_command_queue command_queue, cl_uint num_svm_pointers, void* svm_pointers[], void (CL_CALLBACK* /*pfn_free_func*/)(cl_command_queue queue, cl_uint num_svm_pointers, void* svm_pointers[], void* user_data), void* user_data, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clEnqueueSVMMemcpyPtr)(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr, const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clEnqueueSVMMemFillPtr)(cl_command_queue command_queue, void* svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clEnqueueSVMMapPtr)(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags flags, void* svm_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0;
typedef cl_int (CL_API_CALL * clEnqueueSVMUnmapPtr)(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0;
typedef void* (CL_API_CALL * clGetExtensionFunctionAddressForPlatformPtr)(cl_platform_id platform, const char* func_name) CL_API_SUFFIX__VERSION_1_2;
typedef cl_mem (CL_API_CALL * clCreateImage2DPtr)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, cl_int* errcode_ret);
typedef cl_mem (CL_API_CALL * clCreateImage3DPtr)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, cl_int* errcode_ret);
typedef cl_int (CL_API_CALL * clEnqueueMarkerPtr)(cl_command_queue command_queue, cl_event* event);
typedef cl_int (CL_API_CALL * clEnqueueWaitForEventsPtr)(cl_command_queue command_queue, cl_uint num_events, const cl_event* event_list);
typedef cl_int (CL_API_CALL * clEnqueueBarrierPtr)(cl_command_queue command_queue);
typedef cl_int (CL_API_CALL * clUnloadCompilerPtr)(void);
typedef void* (CL_API_CALL * clGetExtensionFunctionAddressPtr)(const char* func_name);
typedef cl_command_queue (CL_API_CALL * clCreateCommandQueuePtr)(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int* errcode_ret);
typedef cl_sampler (CL_API_CALL * clCreateSamplerPtr)(cl_context context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret);
typedef cl_int (CL_API_CALL * clEnqueueTaskPtr)(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);

static clGetPlatformIDsPtr cl_GetPlatformIDs = NULL;
static clGetPlatformInfoPtr cl_GetPlatformInfo = NULL;
static clGetDeviceIDsPtr cl_GetDeviceIDs = NULL;
static clGetDeviceInfoPtr cl_GetDeviceInfo = NULL;
static clCreateSubDevicesPtr cl_CreateSubDevices = NULL;
static clRetainDevicePtr cl_RetainDevice = NULL;
static clReleaseDevicePtr cl_ReleaseDevice = NULL;
static clCreateContextPtr cl_CreateContext = NULL;
static clCreateContextFromTypePtr cl_CreateContextFromType = NULL;
static clRetainContextPtr cl_RetainContext = NULL;
static clReleaseContextPtr cl_ReleaseContext = NULL;
static clGetContextInfoPtr cl_GetContextInfo = NULL;
static clCreateCommandQueueWithPropertiesPtr cl_CreateCommandQueueWithProperties = NULL;
static clRetainCommandQueuePtr cl_RetainCommandQueue = NULL;
static clReleaseCommandQueuePtr cl_ReleaseCommandQueue = NULL;
static clGetCommandQueueInfoPtr cl_GetCommandQueueInfo = NULL;
static clCreateBufferPtr cl_CreateBuffer = NULL;
static clCreateSubBufferPtr cl_CreateSubBuffer = NULL;
static clCreateImagePtr cl_CreateImage = NULL;
static clCreatePipePtr cl_CreatePipe = NULL;
static clRetainMemObjectPtr cl_RetainMemObject = NULL;
static clReleaseMemObjectPtr cl_ReleaseMemObject = NULL;
static clGetSupportedImageFormatsPtr cl_GetSupportedImageFormats = NULL;
static clGetMemObjectInfoPtr cl_GetMemObjectInfo = NULL;
static clGetImageInfoPtr cl_GetImageInfo = NULL;
static clGetPipeInfoPtr cl_GetPipeInfo = NULL;
static clSetMemObjectDestructorCallbackPtr cl_SetMemObjectDestructorCallback = NULL;
static clSVMAllocPtr cl_SVMAlloc = NULL;
static clSVMFreePtr cl_SVMFree = NULL;
static clCreateSamplerWithPropertiesPtr cl_CreateSamplerWithProperties = NULL;
static clRetainSamplerPtr cl_RetainSampler = NULL;
static clReleaseSamplerPtr cl_ReleaseSampler = NULL;
static clGetSamplerInfoPtr cl_GetSamplerInfo = NULL;
static clCreateProgramWithSourcePtr cl_CreateProgramWithSource = NULL;
static clCreateProgramWithBinaryPtr cl_CreateProgramWithBinary = NULL;
static clCreateProgramWithBuiltInKernelsPtr cl_CreateProgramWithBuiltInKernels = NULL;
static clRetainProgramPtr cl_RetainProgram = NULL;
static clReleaseProgramPtr cl_ReleaseProgram = NULL;
static clBuildProgramPtr cl_BuildProgram = NULL;
static clCompileProgramPtr cl_CompileProgram = NULL;
static clLinkProgramPtr cl_LinkProgram = NULL;
static clUnloadPlatformCompilerPtr cl_UnloadPlatformCompiler = NULL;
static clGetProgramInfoPtr cl_GetProgramInfo = NULL;
static clGetProgramBuildInfoPtr cl_GetProgramBuildInfo = NULL;
static clCreateKernelPtr cl_CreateKernel = NULL;
static clCreateKernelsInProgramPtr cl_CreateKernelsInProgram = NULL;
static clRetainKernelPtr cl_RetainKernel = NULL;
static clReleaseKernelPtr cl_ReleaseKernel = NULL;
static clSetKernelArgPtr cl_SetKernelArg = NULL;
static clSetKernelArgSVMPointerPtr cl_SetKernelArgSVMPointer = NULL;
static clSetKernelExecInfoPtr cl_SetKernelExecInfo = NULL;
static clGetKernelInfoPtr cl_GetKernelInfo = NULL;
static clGetKernelArgInfoPtr cl_GetKernelArgInfo = NULL;
static clGetKernelWorkGroupInfoPtr cl_GetKernelWorkGroupInfo = NULL;
static clWaitForEventsPtr cl_WaitForEvents = NULL;
static clGetEventInfoPtr cl_GetEventInfo = NULL;
static clCreateUserEventPtr cl_CreateUserEvent = NULL;
static clRetainEventPtr cl_RetainEvent = NULL;
static clReleaseEventPtr cl_ReleaseEvent = NULL;
static clSetUserEventStatusPtr cl_SetUserEventStatus = NULL;
static clSetEventCallbackPtr cl_SetEventCallback = NULL;
static clGetEventProfilingInfoPtr cl_GetEventProfilingInfo = NULL;
static clFlushPtr cl_Flush = NULL;
static clFinishPtr cl_Finish = NULL;
static clEnqueueReadBufferPtr cl_EnqueueReadBuffer = NULL;
static clEnqueueReadBufferRectPtr cl_EnqueueReadBufferRect = NULL;
static clEnqueueWriteBufferPtr cl_EnqueueWriteBuffer = NULL;
static clEnqueueWriteBufferRectPtr cl_EnqueueWriteBufferRect = NULL;
static clEnqueueFillBufferPtr cl_EnqueueFillBuffer = NULL;
static clEnqueueCopyBufferPtr cl_EnqueueCopyBuffer = NULL;
static clEnqueueCopyBufferRectPtr cl_EnqueueCopyBufferRect = NULL;
static clEnqueueReadImagePtr cl_EnqueueReadImage = NULL;
static clEnqueueWriteImagePtr cl_EnqueueWriteImage = NULL;
static clEnqueueFillImagePtr cl_EnqueueFillImage = NULL;
static clEnqueueCopyImagePtr cl_EnqueueCopyImage = NULL;
static clEnqueueCopyImageToBufferPtr cl_EnqueueCopyImageToBuffer = NULL;
static clEnqueueCopyBufferToImagePtr cl_EnqueueCopyBufferToImage = NULL;
static clEnqueueMapBufferPtr cl_EnqueueMapBuffer = NULL;
static clEnqueueMapImagePtr cl_EnqueueMapImage = NULL;
static clEnqueueUnmapMemObjectPtr cl_EnqueueUnmapMemObject = NULL;
static clEnqueueMigrateMemObjectsPtr cl_EnqueueMigrateMemObjects = NULL;
static clEnqueueNDRangeKernelPtr cl_EnqueueNDRangeKernel = NULL;
static clEnqueueNativeKernelPtr cl_EnqueueNativeKernel = NULL;
static clEnqueueMarkerWithWaitListPtr cl_EnqueueMarkerWithWaitList = NULL;
static clEnqueueBarrierWithWaitListPtr cl_EnqueueBarrierWithWaitList = NULL;
static clEnqueueSVMFreePtr cl_EnqueueSVMFree = NULL;
static clEnqueueSVMMemcpyPtr cl_EnqueueSVMMemcpy = NULL;
static clEnqueueSVMMemFillPtr cl_EnqueueSVMMemFill = NULL;
static clEnqueueSVMMapPtr cl_EnqueueSVMMap = NULL;
static clEnqueueSVMUnmapPtr cl_EnqueueSVMUnmap = NULL;
static clGetExtensionFunctionAddressForPlatformPtr cl_GetExtensionFunctionAddressForPlatform = NULL;
static clCreateImage2DPtr cl_CreateImage2D = NULL;
static clCreateImage3DPtr cl_CreateImage3D = NULL;
static clEnqueueMarkerPtr cl_EnqueueMarker = NULL;
static clEnqueueWaitForEventsPtr cl_EnqueueWaitForEvents = NULL;
static clEnqueueBarrierPtr cl_EnqueueBarrier = NULL;
static clUnloadCompilerPtr cl_UnloadCompiler = NULL;
static clGetExtensionFunctionAddressPtr cl_GetExtensionFunctionAddress = NULL;
static clCreateCommandQueuePtr cl_CreateCommandQueue = NULL;
static clCreateSamplerPtr cl_CreateSampler = NULL;
static clEnqueueTaskPtr cl_EnqueueTask = NULL;

#include <Windows.h>

static struct Loader
{
	struct Loader()
	{
		HMODULE hModule = LoadLibrary("OpenCL.dll");

		if(hModule == NULL) return;

		*(void**)&cl_GetPlatformIDs = GetProcAddress(hModule, "clGetPlatformIDs");
		*(void**)&cl_GetPlatformInfo = GetProcAddress(hModule, "clGetPlatformInfo");
		*(void**)&cl_GetDeviceIDs = GetProcAddress(hModule, "clGetDeviceIDs");
		*(void**)&cl_GetDeviceInfo = GetProcAddress(hModule, "clGetDeviceInfo");
		*(void**)&cl_CreateSubDevices = GetProcAddress(hModule, "clCreateSubDevices");
		*(void**)&cl_RetainDevice = GetProcAddress(hModule, "clRetainDevice");
		*(void**)&cl_ReleaseDevice = GetProcAddress(hModule, "clReleaseDevice");
		*(void**)&cl_CreateContext = GetProcAddress(hModule, "clCreateContext");
		*(void**)&cl_CreateContextFromType = GetProcAddress(hModule, "clCreateContextFromType");
		*(void**)&cl_RetainContext = GetProcAddress(hModule, "clRetainContext");
		*(void**)&cl_ReleaseContext = GetProcAddress(hModule, "clReleaseContext");
		*(void**)&cl_GetContextInfo = GetProcAddress(hModule, "clGetContextInfo");
		*(void**)&cl_CreateCommandQueueWithProperties = GetProcAddress(hModule, "clCreateCommandQueueWithProperties");
		*(void**)&cl_RetainCommandQueue = GetProcAddress(hModule, "clRetainCommandQueue");
		*(void**)&cl_ReleaseCommandQueue = GetProcAddress(hModule, "clReleaseCommandQueue");
		*(void**)&cl_GetCommandQueueInfo = GetProcAddress(hModule, "clGetCommandQueueInfo");
		*(void**)&cl_CreateBuffer = GetProcAddress(hModule, "clCreateBuffer");
		*(void**)&cl_CreateSubBuffer = GetProcAddress(hModule, "clCreateSubBuffer");
		*(void**)&cl_CreateImage = GetProcAddress(hModule, "clCreateImage");
		*(void**)&cl_CreatePipe = GetProcAddress(hModule, "clCreatePipe");
		*(void**)&cl_RetainMemObject = GetProcAddress(hModule, "clRetainMemObject");
		*(void**)&cl_ReleaseMemObject = GetProcAddress(hModule, "clReleaseMemObject");
		*(void**)&cl_GetSupportedImageFormats = GetProcAddress(hModule, "clGetSupportedImageFormats");
		*(void**)&cl_GetMemObjectInfo = GetProcAddress(hModule, "clGetMemObjectInfo");
		*(void**)&cl_GetImageInfo = GetProcAddress(hModule, "clGetImageInfo");
		*(void**)&cl_GetPipeInfo = GetProcAddress(hModule, "clGetPipeInfo");
		*(void**)&cl_SetMemObjectDestructorCallback = GetProcAddress(hModule, "clSetMemObjectDestructorCallback");
		*(void**)&cl_SVMAlloc = GetProcAddress(hModule, "clSVMAlloc");
		*(void**)&cl_SVMFree = GetProcAddress(hModule, "clSVMFree");
		*(void**)&cl_CreateSamplerWithProperties = GetProcAddress(hModule, "clCreateSamplerWithProperties");
		*(void**)&cl_RetainSampler = GetProcAddress(hModule, "clRetainSampler");
		*(void**)&cl_ReleaseSampler = GetProcAddress(hModule, "clReleaseSampler");
		*(void**)&cl_GetSamplerInfo = GetProcAddress(hModule, "clGetSamplerInfo");
		*(void**)&cl_CreateProgramWithSource = GetProcAddress(hModule, "clCreateProgramWithSource");
		*(void**)&cl_CreateProgramWithBinary = GetProcAddress(hModule, "clCreateProgramWithBinary");
		*(void**)&cl_CreateProgramWithBuiltInKernels = GetProcAddress(hModule, "clCreateProgramWithBuiltInKernels");
		*(void**)&cl_RetainProgram = GetProcAddress(hModule, "clRetainProgram");
		*(void**)&cl_ReleaseProgram = GetProcAddress(hModule, "clReleaseProgram");
		*(void**)&cl_BuildProgram = GetProcAddress(hModule, "clBuildProgram");
		*(void**)&cl_CompileProgram = GetProcAddress(hModule, "clCompileProgram");
		*(void**)&cl_LinkProgram = GetProcAddress(hModule, "clLinkProgram");
		*(void**)&cl_UnloadPlatformCompiler = GetProcAddress(hModule, "clUnloadPlatformCompiler");
		*(void**)&cl_GetProgramInfo = GetProcAddress(hModule, "clGetProgramInfo");
		*(void**)&cl_GetProgramBuildInfo = GetProcAddress(hModule, "clGetProgramBuildInfo");
		*(void**)&cl_CreateKernel = GetProcAddress(hModule, "clCreateKernel");
		*(void**)&cl_CreateKernelsInProgram = GetProcAddress(hModule, "clCreateKernelsInProgram");
		*(void**)&cl_RetainKernel = GetProcAddress(hModule, "clRetainKernel");
		*(void**)&cl_ReleaseKernel = GetProcAddress(hModule, "clReleaseKernel");
		*(void**)&cl_SetKernelArg = GetProcAddress(hModule, "clSetKernelArg");
		*(void**)&cl_SetKernelArgSVMPointer = GetProcAddress(hModule, "clSetKernelArgSVMPointer");
		*(void**)&cl_SetKernelExecInfo = GetProcAddress(hModule, "clSetKernelExecInfo");
		*(void**)&cl_GetKernelInfo = GetProcAddress(hModule, "clGetKernelInfo");
		*(void**)&cl_GetKernelArgInfo = GetProcAddress(hModule, "clGetKernelArgInfo");
		*(void**)&cl_GetKernelWorkGroupInfo = GetProcAddress(hModule, "clGetKernelWorkGroupInfo");
		*(void**)&cl_WaitForEvents = GetProcAddress(hModule, "clWaitForEvents");
		*(void**)&cl_GetEventInfo = GetProcAddress(hModule, "clGetEventInfo");
		*(void**)&cl_CreateUserEvent = GetProcAddress(hModule, "clCreateUserEvent");
		*(void**)&cl_RetainEvent = GetProcAddress(hModule, "clRetainEvent");
		*(void**)&cl_ReleaseEvent = GetProcAddress(hModule, "clReleaseEvent");
		*(void**)&cl_SetUserEventStatus = GetProcAddress(hModule, "clSetUserEventStatus");
		*(void**)&cl_SetEventCallback = GetProcAddress(hModule, "clSetEventCallback");
		*(void**)&cl_GetEventProfilingInfo = GetProcAddress(hModule, "clGetEventProfilingInfo");
		*(void**)&cl_Flush = GetProcAddress(hModule, "clFlush");
		*(void**)&cl_Finish = GetProcAddress(hModule, "clFinish");
		*(void**)&cl_EnqueueReadBuffer = GetProcAddress(hModule, "clEnqueueReadBuffer");
		*(void**)&cl_EnqueueReadBufferRect = GetProcAddress(hModule, "clEnqueueReadBufferRect");
		*(void**)&cl_EnqueueWriteBuffer = GetProcAddress(hModule, "clEnqueueWriteBuffer");
		*(void**)&cl_EnqueueWriteBufferRect = GetProcAddress(hModule, "clEnqueueWriteBufferRect");
		*(void**)&cl_EnqueueFillBuffer = GetProcAddress(hModule, "clEnqueueFillBuffer");
		*(void**)&cl_EnqueueCopyBuffer = GetProcAddress(hModule, "clEnqueueCopyBuffer");
		*(void**)&cl_EnqueueCopyBufferRect = GetProcAddress(hModule, "clEnqueueCopyBufferRect");
		*(void**)&cl_EnqueueReadImage = GetProcAddress(hModule, "clEnqueueReadImage");
		*(void**)&cl_EnqueueWriteImage = GetProcAddress(hModule, "clEnqueueWriteImage");
		*(void**)&cl_EnqueueFillImage = GetProcAddress(hModule, "clEnqueueFillImage");
		*(void**)&cl_EnqueueCopyImage = GetProcAddress(hModule, "clEnqueueCopyImage");
		*(void**)&cl_EnqueueCopyImageToBuffer = GetProcAddress(hModule, "clEnqueueCopyImageToBuffer");
		*(void**)&cl_EnqueueCopyBufferToImage = GetProcAddress(hModule, "clEnqueueCopyBufferToImage");
		*(void**)&cl_EnqueueMapBuffer = GetProcAddress(hModule, "clEnqueueMapBuffer");
		*(void**)&cl_EnqueueMapImage = GetProcAddress(hModule, "clEnqueueMapImage");
		*(void**)&cl_EnqueueUnmapMemObject = GetProcAddress(hModule, "clEnqueueUnmapMemObject");
		*(void**)&cl_EnqueueMigrateMemObjects = GetProcAddress(hModule, "clEnqueueMigrateMemObjects");
		*(void**)&cl_EnqueueNDRangeKernel = GetProcAddress(hModule, "clEnqueueNDRangeKernel");
		*(void**)&cl_EnqueueNativeKernel = GetProcAddress(hModule, "clEnqueueNativeKernel");
		*(void**)&cl_EnqueueMarkerWithWaitList = GetProcAddress(hModule, "clEnqueueMarkerWithWaitList");
		*(void**)&cl_EnqueueBarrierWithWaitList = GetProcAddress(hModule, "clEnqueueBarrierWithWaitList");
		*(void**)&cl_EnqueueSVMFree = GetProcAddress(hModule, "clEnqueueSVMFree");
		*(void**)&cl_EnqueueSVMMemcpy = GetProcAddress(hModule, "clEnqueueSVMMemcpy");
		*(void**)&cl_EnqueueSVMMemFill = GetProcAddress(hModule, "clEnqueueSVMMemFill");
		*(void**)&cl_EnqueueSVMMap = GetProcAddress(hModule, "clEnqueueSVMMap");
		*(void**)&cl_EnqueueSVMUnmap = GetProcAddress(hModule, "clEnqueueSVMUnmap");
		*(void**)&cl_GetExtensionFunctionAddressForPlatform = GetProcAddress(hModule, "clGetExtensionFunctionAddressForPlatform");
		*(void**)&cl_CreateImage2D = GetProcAddress(hModule, "clCreateImage2D");
		*(void**)&cl_CreateImage3D = GetProcAddress(hModule, "clCreateImage3D");
		*(void**)&cl_EnqueueMarker = GetProcAddress(hModule, "clEnqueueMarker");
		*(void**)&cl_EnqueueWaitForEvents = GetProcAddress(hModule, "clEnqueueWaitForEvents");
		*(void**)&cl_EnqueueBarrier = GetProcAddress(hModule, "clEnqueueBarrier");
		*(void**)&cl_UnloadCompiler = GetProcAddress(hModule, "clUnloadCompiler");
		*(void**)&cl_GetExtensionFunctionAddress = GetProcAddress(hModule, "clGetExtensionFunctionAddress");
		*(void**)&cl_CreateCommandQueue = GetProcAddress(hModule, "clCreateCommandQueue");
		*(void**)&cl_CreateSampler = GetProcAddress(hModule, "clCreateSampler");
		*(void**)&cl_EnqueueTask = GetProcAddress(hModule, "clEnqueueTask");
	}
} s_loader;

cl_int CL_API_CALL clGetPlatformIDs(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetPlatformIDs(num_entries, platforms, num_platforms);
}

cl_int CL_API_CALL clGetPlatformInfo(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetPlatformInfo(platform, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id* devices, cl_uint* num_devices) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetDeviceIDs(platform, device_type, num_entries, devices, num_devices);
}

cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetDeviceInfo(device, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clCreateSubDevices(cl_device_id in_device, const cl_device_partition_property* properties, cl_uint num_devices, cl_device_id* out_devices, cl_uint* num_devices_ret) CL_API_SUFFIX__VERSION_1_2
{
	return cl_CreateSubDevices(in_device, properties, num_devices, out_devices, num_devices_ret);
}

cl_int CL_API_CALL clRetainDevice(cl_device_id device) CL_API_SUFFIX__VERSION_1_2
{
	return cl_RetainDevice(device);
}

cl_int CL_API_CALL clReleaseDevice(cl_device_id device) CL_API_SUFFIX__VERSION_1_2
{
	return cl_ReleaseDevice(device);
}

cl_context CL_API_CALL clCreateContext(const cl_context_properties* properties, cl_uint num_devices, const cl_device_id* devices, void (CL_CALLBACK* pfn_notify)(const char*, const void*, size_t, void*), void* user_data, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_CreateContext(properties, num_devices, devices, pfn_notify, user_data, errcode_ret);
}

cl_context CL_API_CALL clCreateContextFromType(const cl_context_properties* properties, cl_device_type device_type, void (CL_CALLBACK* pfn_notify)(const char*, const void*, size_t, void*), void* user_data, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_CreateContextFromType(properties, device_type, pfn_notify, user_data, errcode_ret);
}

cl_int CL_API_CALL clRetainContext(cl_context context) CL_API_SUFFIX__VERSION_1_0
{
	return cl_RetainContext(context);
}

cl_int CL_API_CALL clReleaseContext(cl_context context) CL_API_SUFFIX__VERSION_1_0
{
	return cl_ReleaseContext(context);
}

cl_int CL_API_CALL clGetContextInfo(cl_context context, cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetContextInfo(context, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_command_queue CL_API_CALL clCreateCommandQueueWithProperties(cl_context context, cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0
{
	return cl_CreateCommandQueueWithProperties(context, device, properties, errcode_ret);
}

cl_int CL_API_CALL clRetainCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	return cl_RetainCommandQueue(command_queue);
}

cl_int CL_API_CALL clReleaseCommandQueue(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	return cl_ReleaseCommandQueue(command_queue);
}

cl_int CL_API_CALL clGetCommandQueueInfo(cl_command_queue command_queue, cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetCommandQueueInfo(command_queue, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_mem CL_API_CALL clCreateBuffer(cl_context context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_CreateBuffer(context, flags, size, host_ptr, errcode_ret);
}

cl_mem CL_API_CALL clCreateSubBuffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const void* buffer_create_info, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_1
{
	return cl_CreateSubBuffer(buffer, flags, buffer_create_type, buffer_create_info, errcode_ret);
}

cl_mem CL_API_CALL clCreateImage(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2
{
	return cl_CreateImage(context, flags, image_format, image_desc, host_ptr, errcode_ret);
}

cl_mem CL_API_CALL clCreatePipe(cl_context context, cl_mem_flags flags, cl_uint pipe_packet_size, cl_uint pipe_max_packets, const cl_pipe_properties* properties, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0
{
	return cl_CreatePipe(context, flags, pipe_packet_size, pipe_max_packets, properties, errcode_ret);
}

cl_int CL_API_CALL clRetainMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
	return cl_RetainMemObject(memobj);
}

cl_int CL_API_CALL clReleaseMemObject(cl_mem memobj) CL_API_SUFFIX__VERSION_1_0
{
	return cl_ReleaseMemObject(memobj);
}

cl_int CL_API_CALL clGetSupportedImageFormats(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetSupportedImageFormats(context, flags, image_type, num_entries, image_formats, num_image_formats);
}

cl_int CL_API_CALL clGetMemObjectInfo(cl_mem memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetMemObjectInfo(memobj, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clGetImageInfo(cl_mem image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetImageInfo(image, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clGetPipeInfo(cl_mem pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_2_0
{
	return cl_GetPipeInfo(pipe, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clSetMemObjectDestructorCallback(cl_mem memobj, void (CL_CALLBACK* pfn_notify)(cl_mem memobj, void* user_data), void* user_data) CL_API_SUFFIX__VERSION_1_1
{
	return cl_SetMemObjectDestructorCallback(memobj, pfn_notify, user_data);
}

void* CL_API_CALL clSVMAlloc(cl_context context, cl_svm_mem_flags flags, size_t size, cl_uint alignment) CL_API_SUFFIX__VERSION_2_0
{
	return cl_SVMAlloc(context, flags, size, alignment);
}

void CL_API_CALL clSVMFree(cl_context context, void* svm_pointer) CL_API_SUFFIX__VERSION_2_0
{
	cl_SVMFree(context, svm_pointer);
}

cl_sampler CL_API_CALL clCreateSamplerWithProperties(cl_context context, const cl_sampler_properties* normalized_coords, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0
{
	return cl_CreateSamplerWithProperties(context, normalized_coords, errcode_ret);
}

cl_int CL_API_CALL clRetainSampler(cl_sampler sampler) CL_API_SUFFIX__VERSION_1_0
{
	return cl_RetainSampler(sampler);
}

cl_int CL_API_CALL clReleaseSampler(cl_sampler sampler) CL_API_SUFFIX__VERSION_1_0
{
	return cl_ReleaseSampler(sampler);
}

cl_int CL_API_CALL clGetSamplerInfo(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetSamplerInfo(sampler, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_program CL_API_CALL clCreateProgramWithSource(cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_CreateProgramWithSource(context, count, strings, lengths, errcode_ret);
}

cl_program CL_API_CALL clCreateProgramWithBinary(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_CreateProgramWithBinary(context, num_devices, device_list, lengths, binaries, binary_status, errcode_ret);
}

cl_program CL_API_CALL clCreateProgramWithBuiltInKernels(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2
{
	return cl_CreateProgramWithBuiltInKernels(context, num_devices, device_list, kernel_names, errcode_ret);
}

cl_int CL_API_CALL clRetainProgram(cl_program program) CL_API_SUFFIX__VERSION_1_0
{
	return cl_RetainProgram(program);
}

cl_int CL_API_CALL clReleaseProgram(cl_program program) CL_API_SUFFIX__VERSION_1_0
{
	return cl_ReleaseProgram(program);
}

cl_int CL_API_CALL clBuildProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data) CL_API_SUFFIX__VERSION_1_0
{
	return cl_BuildProgram(program, num_devices, device_list, options, pfn_notify, user_data);
}

cl_int CL_API_CALL clCompileProgram(cl_program program, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_headers, const cl_program* input_headers, const char** header_include_names, void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data) CL_API_SUFFIX__VERSION_1_2
{
	return cl_CompileProgram(program, num_devices, device_list, options, num_input_headers, input_headers, header_include_names, pfn_notify, user_data);
}

cl_program CL_API_CALL clLinkProgram(cl_context context, cl_uint num_devices, const cl_device_id* device_list, const char* options, cl_uint num_input_programs, const cl_program* input_programs, void (CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2
{
	return cl_LinkProgram(context, num_devices, device_list, options, num_input_programs, input_programs, pfn_notify, user_data, errcode_ret);
}

cl_int CL_API_CALL clUnloadPlatformCompiler(cl_platform_id platform) CL_API_SUFFIX__VERSION_1_2
{
	return cl_UnloadPlatformCompiler(platform);
}

cl_int CL_API_CALL clGetProgramInfo(cl_program program, cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetProgramInfo(program, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clGetProgramBuildInfo(cl_program program, cl_device_id device, cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetProgramBuildInfo(program, device, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_kernel CL_API_CALL clCreateKernel(cl_program program, const char* kernel_name, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_CreateKernel(program, kernel_name, errcode_ret);
}

cl_int CL_API_CALL clCreateKernelsInProgram(cl_program program, cl_uint num_kernels, cl_kernel* kernels, cl_uint* num_kernels_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_CreateKernelsInProgram(program, num_kernels, kernels, num_kernels_ret);
}

cl_int CL_API_CALL clRetainKernel(cl_kernel kernel) CL_API_SUFFIX__VERSION_1_0
{
	return cl_RetainKernel(kernel);
}

cl_int CL_API_CALL clReleaseKernel(cl_kernel kernel) CL_API_SUFFIX__VERSION_1_0
{
	return cl_ReleaseKernel(kernel);
}

cl_int CL_API_CALL clSetKernelArg(cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void* arg_value) CL_API_SUFFIX__VERSION_1_0
{
	return cl_SetKernelArg(kernel, arg_index, arg_size, arg_value);
}

cl_int CL_API_CALL clSetKernelArgSVMPointer(cl_kernel kernel, cl_uint arg_index, const void* arg_value) CL_API_SUFFIX__VERSION_2_0
{
	return cl_SetKernelArgSVMPointer(kernel, arg_index, arg_value);
}

cl_int CL_API_CALL clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value) CL_API_SUFFIX__VERSION_2_0
{
	return cl_SetKernelExecInfo(kernel, param_name, param_value_size, param_value);
}

cl_int CL_API_CALL clGetKernelInfo(cl_kernel kernel, cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetKernelInfo(kernel, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clGetKernelArgInfo(cl_kernel kernel, cl_uint arg_indx, cl_kernel_arg_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_2
{
	return cl_GetKernelArgInfo(kernel, arg_indx, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clGetKernelWorkGroupInfo(cl_kernel kernel, cl_device_id device, cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetKernelWorkGroupInfo(kernel, device, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clWaitForEvents(cl_uint num_events, const cl_event* event_list) CL_API_SUFFIX__VERSION_1_0
{
	return cl_WaitForEvents(num_events, event_list);
}

cl_int CL_API_CALL clGetEventInfo(cl_event event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetEventInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_event CL_API_CALL clCreateUserEvent(cl_context context, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_1
{
	return cl_CreateUserEvent(context, errcode_ret);
}

cl_int CL_API_CALL clRetainEvent(cl_event event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_RetainEvent(event);
}

cl_int CL_API_CALL clReleaseEvent(cl_event event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_ReleaseEvent(event);
}

cl_int CL_API_CALL clSetUserEventStatus(cl_event event, cl_int execution_status) CL_API_SUFFIX__VERSION_1_1
{
	return cl_SetUserEventStatus(event, execution_status);
}

cl_int CL_API_CALL clSetEventCallback(cl_event event, cl_int command_exec_callback_type, void (CL_CALLBACK* pfn_notify)(cl_event, cl_int, void*), void* user_data) CL_API_SUFFIX__VERSION_1_1
{
	return cl_SetEventCallback(event, command_exec_callback_type, pfn_notify, user_data);
}

cl_int CL_API_CALL clGetEventProfilingInfo(cl_event event, cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_GetEventProfilingInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int CL_API_CALL clFlush(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	return cl_Flush(command_queue);
}

cl_int CL_API_CALL clFinish(cl_command_queue command_queue) CL_API_SUFFIX__VERSION_1_0
{
	return cl_Finish(command_queue);
}

cl_int CL_API_CALL clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueReadBuffer(command_queue, buffer, blocking_read, offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueReadBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, const size_t* buffer_offset, const size_t* host_offset, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_1
{
	return cl_EnqueueReadBufferRect(command_queue, buffer, blocking_read, buffer_offset, host_offset, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueWriteBuffer(command_queue, buffer, blocking_write, offset, size, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueWriteBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, const size_t* buffer_offset, const size_t* host_offset, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_1
{
	return cl_EnqueueWriteBufferRect(command_queue, buffer, blocking_write, buffer_offset, host_offset, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueFillBuffer(cl_command_queue command_queue, cl_mem buffer, const void* pattern, size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2
{
	return cl_EnqueueFillBuffer(command_queue, buffer, pattern, pattern_size, offset, size, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueCopyBuffer(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueCopyBuffer(command_queue, src_buffer, dst_buffer, src_offset, dst_offset, size, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueCopyBufferRect(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer, const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_1
{
	return cl_EnqueueCopyBufferRect(command_queue, src_buffer, dst_buffer, src_origin, dst_origin, region, src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueReadImage(command_queue, image, blocking_read, origin, region, row_pitch, slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueWriteImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueWriteImage(command_queue, image, blocking_write, origin, region, input_row_pitch, input_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueFillImage(cl_command_queue command_queue, cl_mem image, const void* fill_color, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2
{
	return cl_EnqueueFillImage(command_queue, image, fill_color, origin, region, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueCopyImage(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueCopyImage(command_queue, src_image, dst_image, src_origin, dst_origin, region, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueCopyImageToBuffer(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueCopyImageToBuffer(command_queue, src_image, dst_buffer, src_origin, region, dst_offset, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueCopyBufferToImage(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueCopyBufferToImage(command_queue, src_buffer, dst_image, src_offset, dst_origin, region, num_events_in_wait_list, event_wait_list, event);
}

void* CL_API_CALL clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map, cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueMapBuffer(command_queue, buffer, blocking_map, map_flags, offset, size, num_events_in_wait_list, event_wait_list, event, errcode_ret);
}

void* CL_API_CALL clEnqueueMapImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_map, cl_map_flags map_flags, const size_t* origin, const size_t* region, size_t* image_row_pitch, size_t* image_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueMapImage(command_queue, image, blocking_map, map_flags, origin, region, image_row_pitch, image_slice_pitch, num_events_in_wait_list, event_wait_list, event, errcode_ret);
}

cl_int CL_API_CALL clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void* mapped_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueUnmapMemObject(command_queue, memobj, mapped_ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueMigrateMemObjects(cl_command_queue command_queue, cl_uint num_mem_objects, const cl_mem* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2
{
	return cl_EnqueueMigrateMemObjects(command_queue, num_mem_objects, mem_objects, flags, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim, const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueNDRangeKernel(command_queue, kernel, work_dim, global_work_offset, global_work_size, local_work_size, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueNativeKernel(cl_command_queue command_queue, void (CL_CALLBACK* user_func)(void*), void* args, size_t cb_args, cl_uint num_mem_objects, const cl_mem* mem_list, const void** args_mem_loc, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_0
{
	return cl_EnqueueNativeKernel(command_queue, user_func, args, cb_args, num_mem_objects, mem_list, args_mem_loc, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueMarkerWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2
{
	return cl_EnqueueMarkerWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueBarrierWithWaitList(cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_1_2
{
	return cl_EnqueueBarrierWithWaitList(command_queue, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueSVMFree(cl_command_queue command_queue, cl_uint num_svm_pointers, void* svm_pointers[], void (CL_CALLBACK* pfn_free_func)(cl_command_queue queue, cl_uint num_svm_pointers, void* svm_pointers[], void* user_data), void* user_data, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0
{
	return cl_EnqueueSVMFree(command_queue, num_svm_pointers, svm_pointers, pfn_free_func, user_data, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueSVMMemcpy(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr, const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0
{
	return cl_EnqueueSVMMemcpy(command_queue, blocking_copy, dst_ptr, src_ptr, size, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueSVMMemFill(cl_command_queue command_queue, void* svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0
{
	return cl_EnqueueSVMMemFill(command_queue, svm_ptr, pattern, pattern_size, size, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueSVMMap(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags flags, void* svm_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0
{
	return cl_EnqueueSVMMap(command_queue, blocking_map, flags, svm_ptr, size, num_events_in_wait_list, event_wait_list, event);
}

cl_int CL_API_CALL clEnqueueSVMUnmap(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event) CL_API_SUFFIX__VERSION_2_0
{
	return cl_EnqueueSVMUnmap(command_queue, svm_ptr, num_events_in_wait_list, event_wait_list, event);
}

void* CL_API_CALL clGetExtensionFunctionAddressForPlatform(cl_platform_id platform, const char* func_name) CL_API_SUFFIX__VERSION_1_2
{
	return cl_GetExtensionFunctionAddressForPlatform(platform, func_name);
}

cl_mem CL_API_CALL clCreateImage2D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, cl_int* errcode_ret)
{
	return cl_CreateImage2D(context, flags, image_format, image_width, image_height, image_row_pitch, host_ptr, errcode_ret);
}

cl_mem CL_API_CALL clCreateImage3D(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void* host_ptr, cl_int* errcode_ret)
{
	return cl_CreateImage3D(context, flags, image_format, image_width, image_height, image_depth, image_row_pitch, image_slice_pitch, host_ptr, errcode_ret);
}

cl_int CL_API_CALL clEnqueueMarker(cl_command_queue command_queue, cl_event* event)
{
	return cl_EnqueueMarker(command_queue, event);
}

cl_int CL_API_CALL clEnqueueWaitForEvents(cl_command_queue command_queue, cl_uint num_events, const cl_event* event_list)
{
	return cl_EnqueueWaitForEvents(command_queue, num_events, event_list);
}

cl_int CL_API_CALL clEnqueueBarrier(cl_command_queue command_queue)
{
	return cl_EnqueueBarrier(command_queue);
}

cl_int CL_API_CALL clUnloadCompiler(void)
{
	return cl_UnloadCompiler();
}

void* CL_API_CALL clGetExtensionFunctionAddress(const char* func_name)
{
	return cl_GetExtensionFunctionAddress(func_name);
}

cl_command_queue CL_API_CALL clCreateCommandQueue(cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int* errcode_ret)
{
	return cl_CreateCommandQueue(context, device, properties, errcode_ret);
}

cl_sampler CL_API_CALL clCreateSampler(cl_context context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret)
{
	return cl_CreateSampler(context, normalized_coords, addressing_mode, filter_mode, errcode_ret);
}

cl_int CL_API_CALL clEnqueueTask(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	return cl_EnqueueTask(command_queue, kernel, num_events_in_wait_list, event_wait_list, event);
}
