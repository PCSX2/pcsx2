// dear imgui: Renderer Backend for DirectX12
// This needs to be used along with a Platform Backend (e.g. Win32)

#pragma once
#include "imgui.h" // IMGUI_IMPL_API

bool ImGui_ImplDX12_Init(DXGI_FORMAT rtv_format);
void ImGui_ImplDX12_Shutdown();
void ImGui_ImplDX12_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
void ImGui_ImplDX12_DestroyDeviceObjects();
bool ImGui_ImplDX12_CreateDeviceObjects();
bool ImGui_ImplDX12_CreateFontsTexture();
