/**
 * SPDX-License-Identifier: (WTFPL OR CC0-1.0) AND Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/wgl.h>

#ifndef GLAD_IMPL_UTIL_C_
#define GLAD_IMPL_UTIL_C_

#ifdef _MSC_VER
#define GLAD_IMPL_UTIL_SSCANF sscanf_s
#else
#define GLAD_IMPL_UTIL_SSCANF sscanf
#endif

#endif /* GLAD_IMPL_UTIL_C_ */

#ifdef __cplusplus
extern "C" {
#endif



int GLAD_WGL_VERSION_1_0 = 0;
int GLAD_WGL_3DFX_multisample = 0;
int GLAD_WGL_3DL_stereo_control = 0;
int GLAD_WGL_AMD_gpu_association = 0;
int GLAD_WGL_ARB_buffer_region = 0;
int GLAD_WGL_ARB_context_flush_control = 0;
int GLAD_WGL_ARB_create_context = 0;
int GLAD_WGL_ARB_create_context_no_error = 0;
int GLAD_WGL_ARB_create_context_profile = 0;
int GLAD_WGL_ARB_create_context_robustness = 0;
int GLAD_WGL_ARB_extensions_string = 0;
int GLAD_WGL_ARB_framebuffer_sRGB = 0;
int GLAD_WGL_ARB_make_current_read = 0;
int GLAD_WGL_ARB_multisample = 0;
int GLAD_WGL_ARB_pbuffer = 0;
int GLAD_WGL_ARB_pixel_format = 0;
int GLAD_WGL_ARB_pixel_format_float = 0;
int GLAD_WGL_ARB_render_texture = 0;
int GLAD_WGL_ARB_robustness_application_isolation = 0;
int GLAD_WGL_ARB_robustness_share_group_isolation = 0;
int GLAD_WGL_ATI_pixel_format_float = 0;
int GLAD_WGL_ATI_render_texture_rectangle = 0;
int GLAD_WGL_EXT_colorspace = 0;
int GLAD_WGL_EXT_create_context_es2_profile = 0;
int GLAD_WGL_EXT_create_context_es_profile = 0;
int GLAD_WGL_EXT_depth_float = 0;
int GLAD_WGL_EXT_display_color_table = 0;
int GLAD_WGL_EXT_extensions_string = 0;
int GLAD_WGL_EXT_framebuffer_sRGB = 0;
int GLAD_WGL_EXT_make_current_read = 0;
int GLAD_WGL_EXT_multisample = 0;
int GLAD_WGL_EXT_pbuffer = 0;
int GLAD_WGL_EXT_pixel_format = 0;
int GLAD_WGL_EXT_pixel_format_packed_float = 0;
int GLAD_WGL_EXT_swap_control = 0;
int GLAD_WGL_EXT_swap_control_tear = 0;
int GLAD_WGL_I3D_digital_video_control = 0;
int GLAD_WGL_I3D_gamma = 0;
int GLAD_WGL_I3D_genlock = 0;
int GLAD_WGL_I3D_image_buffer = 0;
int GLAD_WGL_I3D_swap_frame_lock = 0;
int GLAD_WGL_I3D_swap_frame_usage = 0;
int GLAD_WGL_NV_DX_interop = 0;
int GLAD_WGL_NV_DX_interop2 = 0;
int GLAD_WGL_NV_copy_image = 0;
int GLAD_WGL_NV_delay_before_swap = 0;
int GLAD_WGL_NV_float_buffer = 0;
int GLAD_WGL_NV_gpu_affinity = 0;
int GLAD_WGL_NV_multigpu_context = 0;
int GLAD_WGL_NV_multisample_coverage = 0;
int GLAD_WGL_NV_present_video = 0;
int GLAD_WGL_NV_render_depth_texture = 0;
int GLAD_WGL_NV_render_texture_rectangle = 0;
int GLAD_WGL_NV_swap_group = 0;
int GLAD_WGL_NV_vertex_array_range = 0;
int GLAD_WGL_NV_video_capture = 0;
int GLAD_WGL_NV_video_output = 0;
int GLAD_WGL_OML_sync_control = 0;



PFNWGLALLOCATEMEMORYNVPROC glad_wglAllocateMemoryNV = NULL;
PFNWGLASSOCIATEIMAGEBUFFEREVENTSI3DPROC glad_wglAssociateImageBufferEventsI3D = NULL;
PFNWGLBEGINFRAMETRACKINGI3DPROC glad_wglBeginFrameTrackingI3D = NULL;
PFNWGLBINDDISPLAYCOLORTABLEEXTPROC glad_wglBindDisplayColorTableEXT = NULL;
PFNWGLBINDSWAPBARRIERNVPROC glad_wglBindSwapBarrierNV = NULL;
PFNWGLBINDTEXIMAGEARBPROC glad_wglBindTexImageARB = NULL;
PFNWGLBINDVIDEOCAPTUREDEVICENVPROC glad_wglBindVideoCaptureDeviceNV = NULL;
PFNWGLBINDVIDEODEVICENVPROC glad_wglBindVideoDeviceNV = NULL;
PFNWGLBINDVIDEOIMAGENVPROC glad_wglBindVideoImageNV = NULL;
PFNWGLBLITCONTEXTFRAMEBUFFERAMDPROC glad_wglBlitContextFramebufferAMD = NULL;
PFNWGLCHOOSEPIXELFORMATARBPROC glad_wglChoosePixelFormatARB = NULL;
PFNWGLCHOOSEPIXELFORMATEXTPROC glad_wglChoosePixelFormatEXT = NULL;
PFNWGLCOPYIMAGESUBDATANVPROC glad_wglCopyImageSubDataNV = NULL;
PFNWGLCREATEAFFINITYDCNVPROC glad_wglCreateAffinityDCNV = NULL;
PFNWGLCREATEASSOCIATEDCONTEXTAMDPROC glad_wglCreateAssociatedContextAMD = NULL;
PFNWGLCREATEASSOCIATEDCONTEXTATTRIBSAMDPROC glad_wglCreateAssociatedContextAttribsAMD = NULL;
PFNWGLCREATEBUFFERREGIONARBPROC glad_wglCreateBufferRegionARB = NULL;
PFNWGLCREATECONTEXTATTRIBSARBPROC glad_wglCreateContextAttribsARB = NULL;
PFNWGLCREATEDISPLAYCOLORTABLEEXTPROC glad_wglCreateDisplayColorTableEXT = NULL;
PFNWGLCREATEIMAGEBUFFERI3DPROC glad_wglCreateImageBufferI3D = NULL;
PFNWGLCREATEPBUFFERARBPROC glad_wglCreatePbufferARB = NULL;
PFNWGLCREATEPBUFFEREXTPROC glad_wglCreatePbufferEXT = NULL;
PFNWGLDXCLOSEDEVICENVPROC glad_wglDXCloseDeviceNV = NULL;
PFNWGLDXLOCKOBJECTSNVPROC glad_wglDXLockObjectsNV = NULL;
PFNWGLDXOBJECTACCESSNVPROC glad_wglDXObjectAccessNV = NULL;
PFNWGLDXOPENDEVICENVPROC glad_wglDXOpenDeviceNV = NULL;
PFNWGLDXREGISTEROBJECTNVPROC glad_wglDXRegisterObjectNV = NULL;
PFNWGLDXSETRESOURCESHAREHANDLENVPROC glad_wglDXSetResourceShareHandleNV = NULL;
PFNWGLDXUNLOCKOBJECTSNVPROC glad_wglDXUnlockObjectsNV = NULL;
PFNWGLDXUNREGISTEROBJECTNVPROC glad_wglDXUnregisterObjectNV = NULL;
PFNWGLDELAYBEFORESWAPNVPROC glad_wglDelayBeforeSwapNV = NULL;
PFNWGLDELETEASSOCIATEDCONTEXTAMDPROC glad_wglDeleteAssociatedContextAMD = NULL;
PFNWGLDELETEBUFFERREGIONARBPROC glad_wglDeleteBufferRegionARB = NULL;
PFNWGLDELETEDCNVPROC glad_wglDeleteDCNV = NULL;
PFNWGLDESTROYDISPLAYCOLORTABLEEXTPROC glad_wglDestroyDisplayColorTableEXT = NULL;
PFNWGLDESTROYIMAGEBUFFERI3DPROC glad_wglDestroyImageBufferI3D = NULL;
PFNWGLDESTROYPBUFFERARBPROC glad_wglDestroyPbufferARB = NULL;
PFNWGLDESTROYPBUFFEREXTPROC glad_wglDestroyPbufferEXT = NULL;
PFNWGLDISABLEFRAMELOCKI3DPROC glad_wglDisableFrameLockI3D = NULL;
PFNWGLDISABLEGENLOCKI3DPROC glad_wglDisableGenlockI3D = NULL;
PFNWGLENABLEFRAMELOCKI3DPROC glad_wglEnableFrameLockI3D = NULL;
PFNWGLENABLEGENLOCKI3DPROC glad_wglEnableGenlockI3D = NULL;
PFNWGLENDFRAMETRACKINGI3DPROC glad_wglEndFrameTrackingI3D = NULL;
PFNWGLENUMGPUDEVICESNVPROC glad_wglEnumGpuDevicesNV = NULL;
PFNWGLENUMGPUSFROMAFFINITYDCNVPROC glad_wglEnumGpusFromAffinityDCNV = NULL;
PFNWGLENUMGPUSNVPROC glad_wglEnumGpusNV = NULL;
PFNWGLENUMERATEVIDEOCAPTUREDEVICESNVPROC glad_wglEnumerateVideoCaptureDevicesNV = NULL;
PFNWGLENUMERATEVIDEODEVICESNVPROC glad_wglEnumerateVideoDevicesNV = NULL;
PFNWGLFREEMEMORYNVPROC glad_wglFreeMemoryNV = NULL;
PFNWGLGENLOCKSAMPLERATEI3DPROC glad_wglGenlockSampleRateI3D = NULL;
PFNWGLGENLOCKSOURCEDELAYI3DPROC glad_wglGenlockSourceDelayI3D = NULL;
PFNWGLGENLOCKSOURCEEDGEI3DPROC glad_wglGenlockSourceEdgeI3D = NULL;
PFNWGLGENLOCKSOURCEI3DPROC glad_wglGenlockSourceI3D = NULL;
PFNWGLGETCONTEXTGPUIDAMDPROC glad_wglGetContextGPUIDAMD = NULL;
PFNWGLGETCURRENTASSOCIATEDCONTEXTAMDPROC glad_wglGetCurrentAssociatedContextAMD = NULL;
PFNWGLGETCURRENTREADDCARBPROC glad_wglGetCurrentReadDCARB = NULL;
PFNWGLGETCURRENTREADDCEXTPROC glad_wglGetCurrentReadDCEXT = NULL;
PFNWGLGETDIGITALVIDEOPARAMETERSI3DPROC glad_wglGetDigitalVideoParametersI3D = NULL;
PFNWGLGETEXTENSIONSSTRINGARBPROC glad_wglGetExtensionsStringARB = NULL;
PFNWGLGETEXTENSIONSSTRINGEXTPROC glad_wglGetExtensionsStringEXT = NULL;
PFNWGLGETFRAMEUSAGEI3DPROC glad_wglGetFrameUsageI3D = NULL;
PFNWGLGETGPUIDSAMDPROC glad_wglGetGPUIDsAMD = NULL;
PFNWGLGETGPUINFOAMDPROC glad_wglGetGPUInfoAMD = NULL;
PFNWGLGETGAMMATABLEI3DPROC glad_wglGetGammaTableI3D = NULL;
PFNWGLGETGAMMATABLEPARAMETERSI3DPROC glad_wglGetGammaTableParametersI3D = NULL;
PFNWGLGETGENLOCKSAMPLERATEI3DPROC glad_wglGetGenlockSampleRateI3D = NULL;
PFNWGLGETGENLOCKSOURCEDELAYI3DPROC glad_wglGetGenlockSourceDelayI3D = NULL;
PFNWGLGETGENLOCKSOURCEEDGEI3DPROC glad_wglGetGenlockSourceEdgeI3D = NULL;
PFNWGLGETGENLOCKSOURCEI3DPROC glad_wglGetGenlockSourceI3D = NULL;
PFNWGLGETMSCRATEOMLPROC glad_wglGetMscRateOML = NULL;
PFNWGLGETPBUFFERDCARBPROC glad_wglGetPbufferDCARB = NULL;
PFNWGLGETPBUFFERDCEXTPROC glad_wglGetPbufferDCEXT = NULL;
PFNWGLGETPIXELFORMATATTRIBFVARBPROC glad_wglGetPixelFormatAttribfvARB = NULL;
PFNWGLGETPIXELFORMATATTRIBFVEXTPROC glad_wglGetPixelFormatAttribfvEXT = NULL;
PFNWGLGETPIXELFORMATATTRIBIVARBPROC glad_wglGetPixelFormatAttribivARB = NULL;
PFNWGLGETPIXELFORMATATTRIBIVEXTPROC glad_wglGetPixelFormatAttribivEXT = NULL;
PFNWGLGETSWAPINTERVALEXTPROC glad_wglGetSwapIntervalEXT = NULL;
PFNWGLGETSYNCVALUESOMLPROC glad_wglGetSyncValuesOML = NULL;
PFNWGLGETVIDEODEVICENVPROC glad_wglGetVideoDeviceNV = NULL;
PFNWGLGETVIDEOINFONVPROC glad_wglGetVideoInfoNV = NULL;
PFNWGLISENABLEDFRAMELOCKI3DPROC glad_wglIsEnabledFrameLockI3D = NULL;
PFNWGLISENABLEDGENLOCKI3DPROC glad_wglIsEnabledGenlockI3D = NULL;
PFNWGLJOINSWAPGROUPNVPROC glad_wglJoinSwapGroupNV = NULL;
PFNWGLLOADDISPLAYCOLORTABLEEXTPROC glad_wglLoadDisplayColorTableEXT = NULL;
PFNWGLLOCKVIDEOCAPTUREDEVICENVPROC glad_wglLockVideoCaptureDeviceNV = NULL;
PFNWGLMAKEASSOCIATEDCONTEXTCURRENTAMDPROC glad_wglMakeAssociatedContextCurrentAMD = NULL;
PFNWGLMAKECONTEXTCURRENTARBPROC glad_wglMakeContextCurrentARB = NULL;
PFNWGLMAKECONTEXTCURRENTEXTPROC glad_wglMakeContextCurrentEXT = NULL;
PFNWGLQUERYCURRENTCONTEXTNVPROC glad_wglQueryCurrentContextNV = NULL;
PFNWGLQUERYFRAMECOUNTNVPROC glad_wglQueryFrameCountNV = NULL;
PFNWGLQUERYFRAMELOCKMASTERI3DPROC glad_wglQueryFrameLockMasterI3D = NULL;
PFNWGLQUERYFRAMETRACKINGI3DPROC glad_wglQueryFrameTrackingI3D = NULL;
PFNWGLQUERYGENLOCKMAXSOURCEDELAYI3DPROC glad_wglQueryGenlockMaxSourceDelayI3D = NULL;
PFNWGLQUERYMAXSWAPGROUPSNVPROC glad_wglQueryMaxSwapGroupsNV = NULL;
PFNWGLQUERYPBUFFERARBPROC glad_wglQueryPbufferARB = NULL;
PFNWGLQUERYPBUFFEREXTPROC glad_wglQueryPbufferEXT = NULL;
PFNWGLQUERYSWAPGROUPNVPROC glad_wglQuerySwapGroupNV = NULL;
PFNWGLQUERYVIDEOCAPTUREDEVICENVPROC glad_wglQueryVideoCaptureDeviceNV = NULL;
PFNWGLRELEASEIMAGEBUFFEREVENTSI3DPROC glad_wglReleaseImageBufferEventsI3D = NULL;
PFNWGLRELEASEPBUFFERDCARBPROC glad_wglReleasePbufferDCARB = NULL;
PFNWGLRELEASEPBUFFERDCEXTPROC glad_wglReleasePbufferDCEXT = NULL;
PFNWGLRELEASETEXIMAGEARBPROC glad_wglReleaseTexImageARB = NULL;
PFNWGLRELEASEVIDEOCAPTUREDEVICENVPROC glad_wglReleaseVideoCaptureDeviceNV = NULL;
PFNWGLRELEASEVIDEODEVICENVPROC glad_wglReleaseVideoDeviceNV = NULL;
PFNWGLRELEASEVIDEOIMAGENVPROC glad_wglReleaseVideoImageNV = NULL;
PFNWGLRESETFRAMECOUNTNVPROC glad_wglResetFrameCountNV = NULL;
PFNWGLRESTOREBUFFERREGIONARBPROC glad_wglRestoreBufferRegionARB = NULL;
PFNWGLSAVEBUFFERREGIONARBPROC glad_wglSaveBufferRegionARB = NULL;
PFNWGLSENDPBUFFERTOVIDEONVPROC glad_wglSendPbufferToVideoNV = NULL;
PFNWGLSETDIGITALVIDEOPARAMETERSI3DPROC glad_wglSetDigitalVideoParametersI3D = NULL;
PFNWGLSETGAMMATABLEI3DPROC glad_wglSetGammaTableI3D = NULL;
PFNWGLSETGAMMATABLEPARAMETERSI3DPROC glad_wglSetGammaTableParametersI3D = NULL;
PFNWGLSETPBUFFERATTRIBARBPROC glad_wglSetPbufferAttribARB = NULL;
PFNWGLSETSTEREOEMITTERSTATE3DLPROC glad_wglSetStereoEmitterState3DL = NULL;
PFNWGLSWAPBUFFERSMSCOMLPROC glad_wglSwapBuffersMscOML = NULL;
PFNWGLSWAPINTERVALEXTPROC glad_wglSwapIntervalEXT = NULL;
PFNWGLSWAPLAYERBUFFERSMSCOMLPROC glad_wglSwapLayerBuffersMscOML = NULL;
PFNWGLWAITFORMSCOMLPROC glad_wglWaitForMscOML = NULL;
PFNWGLWAITFORSBCOMLPROC glad_wglWaitForSbcOML = NULL;


static void glad_wgl_load_WGL_3DL_stereo_control(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_3DL_stereo_control) return;
    glad_wglSetStereoEmitterState3DL = (PFNWGLSETSTEREOEMITTERSTATE3DLPROC) load(userptr, "wglSetStereoEmitterState3DL");
}
static void glad_wgl_load_WGL_AMD_gpu_association(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_AMD_gpu_association) return;
    glad_wglBlitContextFramebufferAMD = (PFNWGLBLITCONTEXTFRAMEBUFFERAMDPROC) load(userptr, "wglBlitContextFramebufferAMD");
    glad_wglCreateAssociatedContextAMD = (PFNWGLCREATEASSOCIATEDCONTEXTAMDPROC) load(userptr, "wglCreateAssociatedContextAMD");
    glad_wglCreateAssociatedContextAttribsAMD = (PFNWGLCREATEASSOCIATEDCONTEXTATTRIBSAMDPROC) load(userptr, "wglCreateAssociatedContextAttribsAMD");
    glad_wglDeleteAssociatedContextAMD = (PFNWGLDELETEASSOCIATEDCONTEXTAMDPROC) load(userptr, "wglDeleteAssociatedContextAMD");
    glad_wglGetContextGPUIDAMD = (PFNWGLGETCONTEXTGPUIDAMDPROC) load(userptr, "wglGetContextGPUIDAMD");
    glad_wglGetCurrentAssociatedContextAMD = (PFNWGLGETCURRENTASSOCIATEDCONTEXTAMDPROC) load(userptr, "wglGetCurrentAssociatedContextAMD");
    glad_wglGetGPUIDsAMD = (PFNWGLGETGPUIDSAMDPROC) load(userptr, "wglGetGPUIDsAMD");
    glad_wglGetGPUInfoAMD = (PFNWGLGETGPUINFOAMDPROC) load(userptr, "wglGetGPUInfoAMD");
    glad_wglMakeAssociatedContextCurrentAMD = (PFNWGLMAKEASSOCIATEDCONTEXTCURRENTAMDPROC) load(userptr, "wglMakeAssociatedContextCurrentAMD");
}
static void glad_wgl_load_WGL_ARB_buffer_region(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_ARB_buffer_region) return;
    glad_wglCreateBufferRegionARB = (PFNWGLCREATEBUFFERREGIONARBPROC) load(userptr, "wglCreateBufferRegionARB");
    glad_wglDeleteBufferRegionARB = (PFNWGLDELETEBUFFERREGIONARBPROC) load(userptr, "wglDeleteBufferRegionARB");
    glad_wglRestoreBufferRegionARB = (PFNWGLRESTOREBUFFERREGIONARBPROC) load(userptr, "wglRestoreBufferRegionARB");
    glad_wglSaveBufferRegionARB = (PFNWGLSAVEBUFFERREGIONARBPROC) load(userptr, "wglSaveBufferRegionARB");
}
static void glad_wgl_load_WGL_ARB_create_context(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_ARB_create_context) return;
    glad_wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) load(userptr, "wglCreateContextAttribsARB");
}
static void glad_wgl_load_WGL_ARB_extensions_string(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_ARB_extensions_string) return;
    glad_wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) load(userptr, "wglGetExtensionsStringARB");
}
static void glad_wgl_load_WGL_ARB_make_current_read(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_ARB_make_current_read) return;
    glad_wglGetCurrentReadDCARB = (PFNWGLGETCURRENTREADDCARBPROC) load(userptr, "wglGetCurrentReadDCARB");
    glad_wglMakeContextCurrentARB = (PFNWGLMAKECONTEXTCURRENTARBPROC) load(userptr, "wglMakeContextCurrentARB");
}
static void glad_wgl_load_WGL_ARB_pbuffer(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_ARB_pbuffer) return;
    glad_wglCreatePbufferARB = (PFNWGLCREATEPBUFFERARBPROC) load(userptr, "wglCreatePbufferARB");
    glad_wglDestroyPbufferARB = (PFNWGLDESTROYPBUFFERARBPROC) load(userptr, "wglDestroyPbufferARB");
    glad_wglGetPbufferDCARB = (PFNWGLGETPBUFFERDCARBPROC) load(userptr, "wglGetPbufferDCARB");
    glad_wglQueryPbufferARB = (PFNWGLQUERYPBUFFERARBPROC) load(userptr, "wglQueryPbufferARB");
    glad_wglReleasePbufferDCARB = (PFNWGLRELEASEPBUFFERDCARBPROC) load(userptr, "wglReleasePbufferDCARB");
}
static void glad_wgl_load_WGL_ARB_pixel_format(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_ARB_pixel_format) return;
    glad_wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC) load(userptr, "wglChoosePixelFormatARB");
    glad_wglGetPixelFormatAttribfvARB = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC) load(userptr, "wglGetPixelFormatAttribfvARB");
    glad_wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC) load(userptr, "wglGetPixelFormatAttribivARB");
}
static void glad_wgl_load_WGL_ARB_render_texture(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_ARB_render_texture) return;
    glad_wglBindTexImageARB = (PFNWGLBINDTEXIMAGEARBPROC) load(userptr, "wglBindTexImageARB");
    glad_wglReleaseTexImageARB = (PFNWGLRELEASETEXIMAGEARBPROC) load(userptr, "wglReleaseTexImageARB");
    glad_wglSetPbufferAttribARB = (PFNWGLSETPBUFFERATTRIBARBPROC) load(userptr, "wglSetPbufferAttribARB");
}
static void glad_wgl_load_WGL_EXT_display_color_table(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_EXT_display_color_table) return;
    glad_wglBindDisplayColorTableEXT = (PFNWGLBINDDISPLAYCOLORTABLEEXTPROC) load(userptr, "wglBindDisplayColorTableEXT");
    glad_wglCreateDisplayColorTableEXT = (PFNWGLCREATEDISPLAYCOLORTABLEEXTPROC) load(userptr, "wglCreateDisplayColorTableEXT");
    glad_wglDestroyDisplayColorTableEXT = (PFNWGLDESTROYDISPLAYCOLORTABLEEXTPROC) load(userptr, "wglDestroyDisplayColorTableEXT");
    glad_wglLoadDisplayColorTableEXT = (PFNWGLLOADDISPLAYCOLORTABLEEXTPROC) load(userptr, "wglLoadDisplayColorTableEXT");
}
static void glad_wgl_load_WGL_EXT_extensions_string(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_EXT_extensions_string) return;
    glad_wglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC) load(userptr, "wglGetExtensionsStringEXT");
}
static void glad_wgl_load_WGL_EXT_make_current_read(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_EXT_make_current_read) return;
    glad_wglGetCurrentReadDCEXT = (PFNWGLGETCURRENTREADDCEXTPROC) load(userptr, "wglGetCurrentReadDCEXT");
    glad_wglMakeContextCurrentEXT = (PFNWGLMAKECONTEXTCURRENTEXTPROC) load(userptr, "wglMakeContextCurrentEXT");
}
static void glad_wgl_load_WGL_EXT_pbuffer(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_EXT_pbuffer) return;
    glad_wglCreatePbufferEXT = (PFNWGLCREATEPBUFFEREXTPROC) load(userptr, "wglCreatePbufferEXT");
    glad_wglDestroyPbufferEXT = (PFNWGLDESTROYPBUFFEREXTPROC) load(userptr, "wglDestroyPbufferEXT");
    glad_wglGetPbufferDCEXT = (PFNWGLGETPBUFFERDCEXTPROC) load(userptr, "wglGetPbufferDCEXT");
    glad_wglQueryPbufferEXT = (PFNWGLQUERYPBUFFEREXTPROC) load(userptr, "wglQueryPbufferEXT");
    glad_wglReleasePbufferDCEXT = (PFNWGLRELEASEPBUFFERDCEXTPROC) load(userptr, "wglReleasePbufferDCEXT");
}
static void glad_wgl_load_WGL_EXT_pixel_format(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_EXT_pixel_format) return;
    glad_wglChoosePixelFormatEXT = (PFNWGLCHOOSEPIXELFORMATEXTPROC) load(userptr, "wglChoosePixelFormatEXT");
    glad_wglGetPixelFormatAttribfvEXT = (PFNWGLGETPIXELFORMATATTRIBFVEXTPROC) load(userptr, "wglGetPixelFormatAttribfvEXT");
    glad_wglGetPixelFormatAttribivEXT = (PFNWGLGETPIXELFORMATATTRIBIVEXTPROC) load(userptr, "wglGetPixelFormatAttribivEXT");
}
static void glad_wgl_load_WGL_EXT_swap_control(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_EXT_swap_control) return;
    glad_wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC) load(userptr, "wglGetSwapIntervalEXT");
    glad_wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) load(userptr, "wglSwapIntervalEXT");
}
static void glad_wgl_load_WGL_I3D_digital_video_control(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_I3D_digital_video_control) return;
    glad_wglGetDigitalVideoParametersI3D = (PFNWGLGETDIGITALVIDEOPARAMETERSI3DPROC) load(userptr, "wglGetDigitalVideoParametersI3D");
    glad_wglSetDigitalVideoParametersI3D = (PFNWGLSETDIGITALVIDEOPARAMETERSI3DPROC) load(userptr, "wglSetDigitalVideoParametersI3D");
}
static void glad_wgl_load_WGL_I3D_gamma(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_I3D_gamma) return;
    glad_wglGetGammaTableI3D = (PFNWGLGETGAMMATABLEI3DPROC) load(userptr, "wglGetGammaTableI3D");
    glad_wglGetGammaTableParametersI3D = (PFNWGLGETGAMMATABLEPARAMETERSI3DPROC) load(userptr, "wglGetGammaTableParametersI3D");
    glad_wglSetGammaTableI3D = (PFNWGLSETGAMMATABLEI3DPROC) load(userptr, "wglSetGammaTableI3D");
    glad_wglSetGammaTableParametersI3D = (PFNWGLSETGAMMATABLEPARAMETERSI3DPROC) load(userptr, "wglSetGammaTableParametersI3D");
}
static void glad_wgl_load_WGL_I3D_genlock(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_I3D_genlock) return;
    glad_wglDisableGenlockI3D = (PFNWGLDISABLEGENLOCKI3DPROC) load(userptr, "wglDisableGenlockI3D");
    glad_wglEnableGenlockI3D = (PFNWGLENABLEGENLOCKI3DPROC) load(userptr, "wglEnableGenlockI3D");
    glad_wglGenlockSampleRateI3D = (PFNWGLGENLOCKSAMPLERATEI3DPROC) load(userptr, "wglGenlockSampleRateI3D");
    glad_wglGenlockSourceDelayI3D = (PFNWGLGENLOCKSOURCEDELAYI3DPROC) load(userptr, "wglGenlockSourceDelayI3D");
    glad_wglGenlockSourceEdgeI3D = (PFNWGLGENLOCKSOURCEEDGEI3DPROC) load(userptr, "wglGenlockSourceEdgeI3D");
    glad_wglGenlockSourceI3D = (PFNWGLGENLOCKSOURCEI3DPROC) load(userptr, "wglGenlockSourceI3D");
    glad_wglGetGenlockSampleRateI3D = (PFNWGLGETGENLOCKSAMPLERATEI3DPROC) load(userptr, "wglGetGenlockSampleRateI3D");
    glad_wglGetGenlockSourceDelayI3D = (PFNWGLGETGENLOCKSOURCEDELAYI3DPROC) load(userptr, "wglGetGenlockSourceDelayI3D");
    glad_wglGetGenlockSourceEdgeI3D = (PFNWGLGETGENLOCKSOURCEEDGEI3DPROC) load(userptr, "wglGetGenlockSourceEdgeI3D");
    glad_wglGetGenlockSourceI3D = (PFNWGLGETGENLOCKSOURCEI3DPROC) load(userptr, "wglGetGenlockSourceI3D");
    glad_wglIsEnabledGenlockI3D = (PFNWGLISENABLEDGENLOCKI3DPROC) load(userptr, "wglIsEnabledGenlockI3D");
    glad_wglQueryGenlockMaxSourceDelayI3D = (PFNWGLQUERYGENLOCKMAXSOURCEDELAYI3DPROC) load(userptr, "wglQueryGenlockMaxSourceDelayI3D");
}
static void glad_wgl_load_WGL_I3D_image_buffer(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_I3D_image_buffer) return;
    glad_wglAssociateImageBufferEventsI3D = (PFNWGLASSOCIATEIMAGEBUFFEREVENTSI3DPROC) load(userptr, "wglAssociateImageBufferEventsI3D");
    glad_wglCreateImageBufferI3D = (PFNWGLCREATEIMAGEBUFFERI3DPROC) load(userptr, "wglCreateImageBufferI3D");
    glad_wglDestroyImageBufferI3D = (PFNWGLDESTROYIMAGEBUFFERI3DPROC) load(userptr, "wglDestroyImageBufferI3D");
    glad_wglReleaseImageBufferEventsI3D = (PFNWGLRELEASEIMAGEBUFFEREVENTSI3DPROC) load(userptr, "wglReleaseImageBufferEventsI3D");
}
static void glad_wgl_load_WGL_I3D_swap_frame_lock(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_I3D_swap_frame_lock) return;
    glad_wglDisableFrameLockI3D = (PFNWGLDISABLEFRAMELOCKI3DPROC) load(userptr, "wglDisableFrameLockI3D");
    glad_wglEnableFrameLockI3D = (PFNWGLENABLEFRAMELOCKI3DPROC) load(userptr, "wglEnableFrameLockI3D");
    glad_wglIsEnabledFrameLockI3D = (PFNWGLISENABLEDFRAMELOCKI3DPROC) load(userptr, "wglIsEnabledFrameLockI3D");
    glad_wglQueryFrameLockMasterI3D = (PFNWGLQUERYFRAMELOCKMASTERI3DPROC) load(userptr, "wglQueryFrameLockMasterI3D");
}
static void glad_wgl_load_WGL_I3D_swap_frame_usage(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_I3D_swap_frame_usage) return;
    glad_wglBeginFrameTrackingI3D = (PFNWGLBEGINFRAMETRACKINGI3DPROC) load(userptr, "wglBeginFrameTrackingI3D");
    glad_wglEndFrameTrackingI3D = (PFNWGLENDFRAMETRACKINGI3DPROC) load(userptr, "wglEndFrameTrackingI3D");
    glad_wglGetFrameUsageI3D = (PFNWGLGETFRAMEUSAGEI3DPROC) load(userptr, "wglGetFrameUsageI3D");
    glad_wglQueryFrameTrackingI3D = (PFNWGLQUERYFRAMETRACKINGI3DPROC) load(userptr, "wglQueryFrameTrackingI3D");
}
static void glad_wgl_load_WGL_NV_DX_interop(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_DX_interop) return;
    glad_wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC) load(userptr, "wglDXCloseDeviceNV");
    glad_wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC) load(userptr, "wglDXLockObjectsNV");
    glad_wglDXObjectAccessNV = (PFNWGLDXOBJECTACCESSNVPROC) load(userptr, "wglDXObjectAccessNV");
    glad_wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC) load(userptr, "wglDXOpenDeviceNV");
    glad_wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC) load(userptr, "wglDXRegisterObjectNV");
    glad_wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC) load(userptr, "wglDXSetResourceShareHandleNV");
    glad_wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC) load(userptr, "wglDXUnlockObjectsNV");
    glad_wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC) load(userptr, "wglDXUnregisterObjectNV");
}
static void glad_wgl_load_WGL_NV_copy_image(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_copy_image) return;
    glad_wglCopyImageSubDataNV = (PFNWGLCOPYIMAGESUBDATANVPROC) load(userptr, "wglCopyImageSubDataNV");
}
static void glad_wgl_load_WGL_NV_delay_before_swap(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_delay_before_swap) return;
    glad_wglDelayBeforeSwapNV = (PFNWGLDELAYBEFORESWAPNVPROC) load(userptr, "wglDelayBeforeSwapNV");
}
static void glad_wgl_load_WGL_NV_gpu_affinity(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_gpu_affinity) return;
    glad_wglCreateAffinityDCNV = (PFNWGLCREATEAFFINITYDCNVPROC) load(userptr, "wglCreateAffinityDCNV");
    glad_wglDeleteDCNV = (PFNWGLDELETEDCNVPROC) load(userptr, "wglDeleteDCNV");
    glad_wglEnumGpuDevicesNV = (PFNWGLENUMGPUDEVICESNVPROC) load(userptr, "wglEnumGpuDevicesNV");
    glad_wglEnumGpusFromAffinityDCNV = (PFNWGLENUMGPUSFROMAFFINITYDCNVPROC) load(userptr, "wglEnumGpusFromAffinityDCNV");
    glad_wglEnumGpusNV = (PFNWGLENUMGPUSNVPROC) load(userptr, "wglEnumGpusNV");
}
static void glad_wgl_load_WGL_NV_present_video(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_present_video) return;
    glad_wglBindVideoDeviceNV = (PFNWGLBINDVIDEODEVICENVPROC) load(userptr, "wglBindVideoDeviceNV");
    glad_wglEnumerateVideoDevicesNV = (PFNWGLENUMERATEVIDEODEVICESNVPROC) load(userptr, "wglEnumerateVideoDevicesNV");
    glad_wglQueryCurrentContextNV = (PFNWGLQUERYCURRENTCONTEXTNVPROC) load(userptr, "wglQueryCurrentContextNV");
}
static void glad_wgl_load_WGL_NV_swap_group(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_swap_group) return;
    glad_wglBindSwapBarrierNV = (PFNWGLBINDSWAPBARRIERNVPROC) load(userptr, "wglBindSwapBarrierNV");
    glad_wglJoinSwapGroupNV = (PFNWGLJOINSWAPGROUPNVPROC) load(userptr, "wglJoinSwapGroupNV");
    glad_wglQueryFrameCountNV = (PFNWGLQUERYFRAMECOUNTNVPROC) load(userptr, "wglQueryFrameCountNV");
    glad_wglQueryMaxSwapGroupsNV = (PFNWGLQUERYMAXSWAPGROUPSNVPROC) load(userptr, "wglQueryMaxSwapGroupsNV");
    glad_wglQuerySwapGroupNV = (PFNWGLQUERYSWAPGROUPNVPROC) load(userptr, "wglQuerySwapGroupNV");
    glad_wglResetFrameCountNV = (PFNWGLRESETFRAMECOUNTNVPROC) load(userptr, "wglResetFrameCountNV");
}
static void glad_wgl_load_WGL_NV_vertex_array_range(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_vertex_array_range) return;
    glad_wglAllocateMemoryNV = (PFNWGLALLOCATEMEMORYNVPROC) load(userptr, "wglAllocateMemoryNV");
    glad_wglFreeMemoryNV = (PFNWGLFREEMEMORYNVPROC) load(userptr, "wglFreeMemoryNV");
}
static void glad_wgl_load_WGL_NV_video_capture(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_video_capture) return;
    glad_wglBindVideoCaptureDeviceNV = (PFNWGLBINDVIDEOCAPTUREDEVICENVPROC) load(userptr, "wglBindVideoCaptureDeviceNV");
    glad_wglEnumerateVideoCaptureDevicesNV = (PFNWGLENUMERATEVIDEOCAPTUREDEVICESNVPROC) load(userptr, "wglEnumerateVideoCaptureDevicesNV");
    glad_wglLockVideoCaptureDeviceNV = (PFNWGLLOCKVIDEOCAPTUREDEVICENVPROC) load(userptr, "wglLockVideoCaptureDeviceNV");
    glad_wglQueryVideoCaptureDeviceNV = (PFNWGLQUERYVIDEOCAPTUREDEVICENVPROC) load(userptr, "wglQueryVideoCaptureDeviceNV");
    glad_wglReleaseVideoCaptureDeviceNV = (PFNWGLRELEASEVIDEOCAPTUREDEVICENVPROC) load(userptr, "wglReleaseVideoCaptureDeviceNV");
}
static void glad_wgl_load_WGL_NV_video_output(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_NV_video_output) return;
    glad_wglBindVideoImageNV = (PFNWGLBINDVIDEOIMAGENVPROC) load(userptr, "wglBindVideoImageNV");
    glad_wglGetVideoDeviceNV = (PFNWGLGETVIDEODEVICENVPROC) load(userptr, "wglGetVideoDeviceNV");
    glad_wglGetVideoInfoNV = (PFNWGLGETVIDEOINFONVPROC) load(userptr, "wglGetVideoInfoNV");
    glad_wglReleaseVideoDeviceNV = (PFNWGLRELEASEVIDEODEVICENVPROC) load(userptr, "wglReleaseVideoDeviceNV");
    glad_wglReleaseVideoImageNV = (PFNWGLRELEASEVIDEOIMAGENVPROC) load(userptr, "wglReleaseVideoImageNV");
    glad_wglSendPbufferToVideoNV = (PFNWGLSENDPBUFFERTOVIDEONVPROC) load(userptr, "wglSendPbufferToVideoNV");
}
static void glad_wgl_load_WGL_OML_sync_control(GLADuserptrloadfunc load, void *userptr) {
    if(!GLAD_WGL_OML_sync_control) return;
    glad_wglGetMscRateOML = (PFNWGLGETMSCRATEOMLPROC) load(userptr, "wglGetMscRateOML");
    glad_wglGetSyncValuesOML = (PFNWGLGETSYNCVALUESOMLPROC) load(userptr, "wglGetSyncValuesOML");
    glad_wglSwapBuffersMscOML = (PFNWGLSWAPBUFFERSMSCOMLPROC) load(userptr, "wglSwapBuffersMscOML");
    glad_wglSwapLayerBuffersMscOML = (PFNWGLSWAPLAYERBUFFERSMSCOMLPROC) load(userptr, "wglSwapLayerBuffersMscOML");
    glad_wglWaitForMscOML = (PFNWGLWAITFORMSCOMLPROC) load(userptr, "wglWaitForMscOML");
    glad_wglWaitForSbcOML = (PFNWGLWAITFORSBCOMLPROC) load(userptr, "wglWaitForSbcOML");
}



static int glad_wgl_has_extension(HDC hdc, const char *ext) {
    const char *terminator;
    const char *loc;
    const char *extensions;

    if(wglGetExtensionsStringEXT == NULL && wglGetExtensionsStringARB == NULL)
        return 0;

    if(wglGetExtensionsStringARB == NULL || hdc == INVALID_HANDLE_VALUE)
        extensions = wglGetExtensionsStringEXT();
    else
        extensions = wglGetExtensionsStringARB(hdc);

    if(extensions == NULL || ext == NULL)
        return 0;

    while(1) {
        loc = strstr(extensions, ext);
        if(loc == NULL)
            break;

        terminator = loc + strlen(ext);
        if((loc == extensions || *(loc - 1) == ' ') &&
            (*terminator == ' ' || *terminator == '\0'))
        {
            return 1;
        }
        extensions = terminator;
    }

    return 0;
}

static GLADapiproc glad_wgl_get_proc_from_userptr(void *userptr, const char* name) {
    return (GLAD_GNUC_EXTENSION (GLADapiproc (*)(const char *name)) userptr)(name);
}

static int glad_wgl_find_extensions_wgl(HDC hdc) {
    GLAD_WGL_3DFX_multisample = glad_wgl_has_extension(hdc, "WGL_3DFX_multisample");
    GLAD_WGL_3DL_stereo_control = glad_wgl_has_extension(hdc, "WGL_3DL_stereo_control");
    GLAD_WGL_AMD_gpu_association = glad_wgl_has_extension(hdc, "WGL_AMD_gpu_association");
    GLAD_WGL_ARB_buffer_region = glad_wgl_has_extension(hdc, "WGL_ARB_buffer_region");
    GLAD_WGL_ARB_context_flush_control = glad_wgl_has_extension(hdc, "WGL_ARB_context_flush_control");
    GLAD_WGL_ARB_create_context = glad_wgl_has_extension(hdc, "WGL_ARB_create_context");
    GLAD_WGL_ARB_create_context_no_error = glad_wgl_has_extension(hdc, "WGL_ARB_create_context_no_error");
    GLAD_WGL_ARB_create_context_profile = glad_wgl_has_extension(hdc, "WGL_ARB_create_context_profile");
    GLAD_WGL_ARB_create_context_robustness = glad_wgl_has_extension(hdc, "WGL_ARB_create_context_robustness");
    GLAD_WGL_ARB_extensions_string = glad_wgl_has_extension(hdc, "WGL_ARB_extensions_string");
    GLAD_WGL_ARB_framebuffer_sRGB = glad_wgl_has_extension(hdc, "WGL_ARB_framebuffer_sRGB");
    GLAD_WGL_ARB_make_current_read = glad_wgl_has_extension(hdc, "WGL_ARB_make_current_read");
    GLAD_WGL_ARB_multisample = glad_wgl_has_extension(hdc, "WGL_ARB_multisample");
    GLAD_WGL_ARB_pbuffer = glad_wgl_has_extension(hdc, "WGL_ARB_pbuffer");
    GLAD_WGL_ARB_pixel_format = glad_wgl_has_extension(hdc, "WGL_ARB_pixel_format");
    GLAD_WGL_ARB_pixel_format_float = glad_wgl_has_extension(hdc, "WGL_ARB_pixel_format_float");
    GLAD_WGL_ARB_render_texture = glad_wgl_has_extension(hdc, "WGL_ARB_render_texture");
    GLAD_WGL_ARB_robustness_application_isolation = glad_wgl_has_extension(hdc, "WGL_ARB_robustness_application_isolation");
    GLAD_WGL_ARB_robustness_share_group_isolation = glad_wgl_has_extension(hdc, "WGL_ARB_robustness_share_group_isolation");
    GLAD_WGL_ATI_pixel_format_float = glad_wgl_has_extension(hdc, "WGL_ATI_pixel_format_float");
    GLAD_WGL_ATI_render_texture_rectangle = glad_wgl_has_extension(hdc, "WGL_ATI_render_texture_rectangle");
    GLAD_WGL_EXT_colorspace = glad_wgl_has_extension(hdc, "WGL_EXT_colorspace");
    GLAD_WGL_EXT_create_context_es2_profile = glad_wgl_has_extension(hdc, "WGL_EXT_create_context_es2_profile");
    GLAD_WGL_EXT_create_context_es_profile = glad_wgl_has_extension(hdc, "WGL_EXT_create_context_es_profile");
    GLAD_WGL_EXT_depth_float = glad_wgl_has_extension(hdc, "WGL_EXT_depth_float");
    GLAD_WGL_EXT_display_color_table = glad_wgl_has_extension(hdc, "WGL_EXT_display_color_table");
    GLAD_WGL_EXT_extensions_string = glad_wgl_has_extension(hdc, "WGL_EXT_extensions_string");
    GLAD_WGL_EXT_framebuffer_sRGB = glad_wgl_has_extension(hdc, "WGL_EXT_framebuffer_sRGB");
    GLAD_WGL_EXT_make_current_read = glad_wgl_has_extension(hdc, "WGL_EXT_make_current_read");
    GLAD_WGL_EXT_multisample = glad_wgl_has_extension(hdc, "WGL_EXT_multisample");
    GLAD_WGL_EXT_pbuffer = glad_wgl_has_extension(hdc, "WGL_EXT_pbuffer");
    GLAD_WGL_EXT_pixel_format = glad_wgl_has_extension(hdc, "WGL_EXT_pixel_format");
    GLAD_WGL_EXT_pixel_format_packed_float = glad_wgl_has_extension(hdc, "WGL_EXT_pixel_format_packed_float");
    GLAD_WGL_EXT_swap_control = glad_wgl_has_extension(hdc, "WGL_EXT_swap_control");
    GLAD_WGL_EXT_swap_control_tear = glad_wgl_has_extension(hdc, "WGL_EXT_swap_control_tear");
    GLAD_WGL_I3D_digital_video_control = glad_wgl_has_extension(hdc, "WGL_I3D_digital_video_control");
    GLAD_WGL_I3D_gamma = glad_wgl_has_extension(hdc, "WGL_I3D_gamma");
    GLAD_WGL_I3D_genlock = glad_wgl_has_extension(hdc, "WGL_I3D_genlock");
    GLAD_WGL_I3D_image_buffer = glad_wgl_has_extension(hdc, "WGL_I3D_image_buffer");
    GLAD_WGL_I3D_swap_frame_lock = glad_wgl_has_extension(hdc, "WGL_I3D_swap_frame_lock");
    GLAD_WGL_I3D_swap_frame_usage = glad_wgl_has_extension(hdc, "WGL_I3D_swap_frame_usage");
    GLAD_WGL_NV_DX_interop = glad_wgl_has_extension(hdc, "WGL_NV_DX_interop");
    GLAD_WGL_NV_DX_interop2 = glad_wgl_has_extension(hdc, "WGL_NV_DX_interop2");
    GLAD_WGL_NV_copy_image = glad_wgl_has_extension(hdc, "WGL_NV_copy_image");
    GLAD_WGL_NV_delay_before_swap = glad_wgl_has_extension(hdc, "WGL_NV_delay_before_swap");
    GLAD_WGL_NV_float_buffer = glad_wgl_has_extension(hdc, "WGL_NV_float_buffer");
    GLAD_WGL_NV_gpu_affinity = glad_wgl_has_extension(hdc, "WGL_NV_gpu_affinity");
    GLAD_WGL_NV_multigpu_context = glad_wgl_has_extension(hdc, "WGL_NV_multigpu_context");
    GLAD_WGL_NV_multisample_coverage = glad_wgl_has_extension(hdc, "WGL_NV_multisample_coverage");
    GLAD_WGL_NV_present_video = glad_wgl_has_extension(hdc, "WGL_NV_present_video");
    GLAD_WGL_NV_render_depth_texture = glad_wgl_has_extension(hdc, "WGL_NV_render_depth_texture");
    GLAD_WGL_NV_render_texture_rectangle = glad_wgl_has_extension(hdc, "WGL_NV_render_texture_rectangle");
    GLAD_WGL_NV_swap_group = glad_wgl_has_extension(hdc, "WGL_NV_swap_group");
    GLAD_WGL_NV_vertex_array_range = glad_wgl_has_extension(hdc, "WGL_NV_vertex_array_range");
    GLAD_WGL_NV_video_capture = glad_wgl_has_extension(hdc, "WGL_NV_video_capture");
    GLAD_WGL_NV_video_output = glad_wgl_has_extension(hdc, "WGL_NV_video_output");
    GLAD_WGL_OML_sync_control = glad_wgl_has_extension(hdc, "WGL_OML_sync_control");
    return 1;
}

static int glad_wgl_find_core_wgl(void) {
    int major = 1, minor = 0;
    GLAD_WGL_VERSION_1_0 = (major == 1 && minor >= 0) || major > 1;
    return GLAD_MAKE_VERSION(major, minor);
}

int gladLoadWGLUserPtr(HDC hdc, GLADuserptrloadfunc load, void *userptr) {
    int version;
    wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) load(userptr, "wglGetExtensionsStringARB");
    wglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC) load(userptr, "wglGetExtensionsStringEXT");
    if(wglGetExtensionsStringARB == NULL && wglGetExtensionsStringEXT == NULL) return 0;
    version = glad_wgl_find_core_wgl();


    if (!glad_wgl_find_extensions_wgl(hdc)) return 0;
    glad_wgl_load_WGL_3DL_stereo_control(load, userptr);
    glad_wgl_load_WGL_AMD_gpu_association(load, userptr);
    glad_wgl_load_WGL_ARB_buffer_region(load, userptr);
    glad_wgl_load_WGL_ARB_create_context(load, userptr);
    glad_wgl_load_WGL_ARB_extensions_string(load, userptr);
    glad_wgl_load_WGL_ARB_make_current_read(load, userptr);
    glad_wgl_load_WGL_ARB_pbuffer(load, userptr);
    glad_wgl_load_WGL_ARB_pixel_format(load, userptr);
    glad_wgl_load_WGL_ARB_render_texture(load, userptr);
    glad_wgl_load_WGL_EXT_display_color_table(load, userptr);
    glad_wgl_load_WGL_EXT_extensions_string(load, userptr);
    glad_wgl_load_WGL_EXT_make_current_read(load, userptr);
    glad_wgl_load_WGL_EXT_pbuffer(load, userptr);
    glad_wgl_load_WGL_EXT_pixel_format(load, userptr);
    glad_wgl_load_WGL_EXT_swap_control(load, userptr);
    glad_wgl_load_WGL_I3D_digital_video_control(load, userptr);
    glad_wgl_load_WGL_I3D_gamma(load, userptr);
    glad_wgl_load_WGL_I3D_genlock(load, userptr);
    glad_wgl_load_WGL_I3D_image_buffer(load, userptr);
    glad_wgl_load_WGL_I3D_swap_frame_lock(load, userptr);
    glad_wgl_load_WGL_I3D_swap_frame_usage(load, userptr);
    glad_wgl_load_WGL_NV_DX_interop(load, userptr);
    glad_wgl_load_WGL_NV_copy_image(load, userptr);
    glad_wgl_load_WGL_NV_delay_before_swap(load, userptr);
    glad_wgl_load_WGL_NV_gpu_affinity(load, userptr);
    glad_wgl_load_WGL_NV_present_video(load, userptr);
    glad_wgl_load_WGL_NV_swap_group(load, userptr);
    glad_wgl_load_WGL_NV_vertex_array_range(load, userptr);
    glad_wgl_load_WGL_NV_video_capture(load, userptr);
    glad_wgl_load_WGL_NV_video_output(load, userptr);
    glad_wgl_load_WGL_OML_sync_control(load, userptr);


    return version;
}

int gladLoadWGL(HDC hdc, GLADloadfunc load) {
    return gladLoadWGLUserPtr(hdc, glad_wgl_get_proc_from_userptr, GLAD_GNUC_EXTENSION (void*) load);
}
 


#ifdef __cplusplus
}
#endif
