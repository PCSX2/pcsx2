// dear imgui: Renderer Backend for DirectX11
// This needs to be used along with a Platform Backend (e.g. Win32)

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

struct ID3D11Device;
struct ID3D11DeviceContext;

bool ImGui_ImplDX11_Init(ID3D11Device* device, ID3D11DeviceContext* device_context);
void ImGui_ImplDX11_Shutdown();
void ImGui_ImplDX11_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
void ImGui_ImplDX11_InvalidateDeviceObjects();
bool ImGui_ImplDX11_CreateDeviceObjects();
void ImGui_ImplDX11_CreateFontsTexture();
