#include "GLContextEGLAndroid.h"
#include "common/Console.h"
#include <android/native_window.h>

GLContextEGLAndroid::GLContextEGLAndroid(const WindowInfo& wi) : GLContextEGL(wi) {}
GLContextEGLAndroid::~GLContextEGLAndroid() = default;

std::unique_ptr<GLContext> GLContextEGLAndroid::Create(const WindowInfo& wi, const Version* versions_to_try,
                                                       size_t num_versions_to_try)
{
  std::unique_ptr<GLContextEGLAndroid> context = std::make_unique<GLContextEGLAndroid>(wi);
  if (!context->Initialize(std::span<const Version>(versions_to_try, num_versions_to_try), nullptr))
    return nullptr;

  return context;
}

std::unique_ptr<GLContext> GLContextEGLAndroid::CreateSharedContext(const WindowInfo& wi, Error* error)
{
  std::unique_ptr<GLContextEGLAndroid> context = std::make_unique<GLContextEGLAndroid>(wi);
  context->m_display = m_display;

  if (!context->CreateContextAndSurface(m_version, m_context, false))
    return nullptr;

  return context;
}

void GLContextEGLAndroid::ResizeSurface(u32 new_surface_width, u32 new_surface_height)
{
    GLContextEGL::ResizeSurface(new_surface_width, new_surface_height);
}

EGLNativeWindowType GLContextEGLAndroid::GetNativeWindow(EGLConfig config)
{
  EGLint native_visual_id = 0;
  if (!eglGetConfigAttrib(m_display, m_config, EGL_NATIVE_VISUAL_ID, &native_visual_id))
  {
    Console.Error("Failed to get native visual ID");
    return 0;
  }

  ANativeWindow_setBuffersGeometry(static_cast<ANativeWindow*>(m_wi.window_handle), 0, 0, static_cast<int32_t>(native_visual_id));
  m_wi.surface_width = ANativeWindow_getWidth(static_cast<ANativeWindow*>(m_wi.window_handle));
  m_wi.surface_height = ANativeWindow_getHeight(static_cast<ANativeWindow*>(m_wi.window_handle));
  return static_cast<EGLNativeWindowType>(m_wi.window_handle);
}
