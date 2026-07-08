#pragma once
#include "GLContextEGL.h"

class GLContextEGLAndroid final : public GLContextEGL
{
public:
    GLContextEGLAndroid(const WindowInfo& wi);
    ~GLContextEGLAndroid() override;

    static std::unique_ptr<GLContext> Create(const WindowInfo& wi, const Version* versions_to_try,
                                             size_t num_versions_to_try);

    std::unique_ptr<GLContext> CreateSharedContext(const WindowInfo& wi, Error* error) override;
    void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) override;

protected:
    EGLNativeWindowType GetNativeWindow(EGLConfig config) override;
};
