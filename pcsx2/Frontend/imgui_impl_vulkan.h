// dear imgui: Renderer Backend for Vulkan
// This needs to be used along with a Platform Backend (e.g. GLFW, SDL, Win32, custom..)

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#include "common/Vulkan/Loader.h"

// Called by user code
bool ImGui_ImplVulkan_Init(VkRenderPass render_pass);
void ImGui_ImplVulkan_Shutdown();
void ImGui_ImplVulkan_RenderDrawData(ImDrawData* draw_data);
bool ImGui_ImplVulkan_CreateFontsTexture();
void ImGui_ImplVulkan_DestroyFontUploadObjects();
