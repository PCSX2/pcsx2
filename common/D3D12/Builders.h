/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/RedtapeWindows.h"

#include <array>
#include <d3d12.h>
#include <wil/com.h>

namespace D3D12
{
	class ShaderCache;

	class RootSignatureBuilder
	{
	public:
		enum : u32
		{
			MAX_PARAMETERS = 16,
			MAX_DESCRIPTOR_RANGES = 16
		};

		RootSignatureBuilder();

		void Clear();

		wil::com_ptr_nothrow<ID3D12RootSignature> Create(bool clear = true);

		void SetInputAssemblerFlag();

		u32 Add32BitConstants(u32 shader_reg, u32 num_values, D3D12_SHADER_VISIBILITY visibility);
		u32 AddCBVParameter(u32 shader_reg, D3D12_SHADER_VISIBILITY visibility);
		u32 AddSRVParameter(u32 shader_reg, D3D12_SHADER_VISIBILITY visibility);
		u32 AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE rt, u32 start_shader_reg, u32 num_shader_regs,
			D3D12_SHADER_VISIBILITY visibility);

	private:
		D3D12_ROOT_SIGNATURE_DESC m_desc{};
		std::array<D3D12_ROOT_PARAMETER, MAX_PARAMETERS> m_params{};
		std::array<D3D12_DESCRIPTOR_RANGE, MAX_DESCRIPTOR_RANGES> m_descriptor_ranges{};
		u32 m_num_descriptor_ranges = 0;
	};

	class GraphicsPipelineBuilder
	{
	public:
		enum : u32
		{
			MAX_VERTEX_ATTRIBUTES = 16,
		};

		GraphicsPipelineBuilder();

		~GraphicsPipelineBuilder() = default;

		void Clear();

		wil::com_ptr_nothrow<ID3D12PipelineState> Create(ID3D12Device* device, bool clear = true);
		wil::com_ptr_nothrow<ID3D12PipelineState> Create(ID3D12Device* device, ShaderCache& cache, bool clear = true);

		void SetRootSignature(ID3D12RootSignature* rs);

		void SetVertexShader(const void* data, u32 data_size);
		void SetGeometryShader(const void* data, u32 data_size);
		void SetPixelShader(const void* data, u32 data_size);

		void SetVertexShader(const ID3DBlob* blob);
		void SetGeometryShader(const ID3DBlob* blob);
		void SetPixelShader(const ID3DBlob* blob);

		void AddVertexAttribute(const char* semantic_name, u32 semantic_index, DXGI_FORMAT format, u32 buffer, u32 offset);

		void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);

		void SetRasterizationState(D3D12_FILL_MODE polygon_mode, D3D12_CULL_MODE cull_mode, bool front_face_ccw);

		void SetMultisamples(u32 multisamples);

		void SetNoCullRasterizationState();

		void SetDepthState(bool depth_test, bool depth_write, D3D12_COMPARISON_FUNC compare_op);
		void SetStencilState(bool stencil_test, u8 read_mask, u8 write_mask, const D3D12_DEPTH_STENCILOP_DESC& front, const D3D12_DEPTH_STENCILOP_DESC& back);

		void SetNoDepthTestState();
		void SetNoStencilState();

		void SetBlendState(u32 rt, bool blend_enable, D3D12_BLEND src_factor, D3D12_BLEND dst_factor, D3D12_BLEND_OP op,
			D3D12_BLEND alpha_src_factor, D3D12_BLEND alpha_dst_factor, D3D12_BLEND_OP alpha_op,
			u8 write_mask = D3D12_COLOR_WRITE_ENABLE_ALL);

		void SetNoBlendingState();

		void ClearRenderTargets();

		void SetRenderTarget(u32 rt, DXGI_FORMAT format);

		void ClearDepthStencilFormat();

		void SetDepthStencilFormat(DXGI_FORMAT format);

	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC m_desc{};
		std::array<D3D12_INPUT_ELEMENT_DESC, MAX_VERTEX_ATTRIBUTES> m_input_elements{};
	};

} // namespace D3D12