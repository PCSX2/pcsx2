// dear imgui: Renderer Backend for modern OpenGL with shaders / programmatic pipeline
// This needs to be used along with a Platform Backend (e.g. GLFW, SDL, Win32, custom..)

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

// Backend API
bool ImGui_ImplOpenGL3_Init(const char* glsl_version = NULL);
void ImGui_ImplOpenGL3_Shutdown();
void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data);

// (Optional) Called by Init/NewFrame/Shutdown
bool ImGui_ImplOpenGL3_CreateFontsTexture();
void ImGui_ImplOpenGL3_DestroyFontsTexture();
bool ImGui_ImplOpenGL3_CreateDeviceObjects();
void ImGui_ImplOpenGL3_DestroyDeviceObjects();
