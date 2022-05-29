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

#include "common/PrecompiledHeader.h"

#include "common/D3D12/Builders.h"
#include "common/D3D12/Context.h"
#include "common/D3D12/ShaderCache.h"
#include "common/Console.h"

#include <cstdarg>
#include <limits>

using namespace D3D12;

GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
	Clear();
}

void GraphicsPipelineBuilder::Clear()
{
	std::memset(&m_desc, 0, sizeof(m_desc));
	std::memset(m_input_elements.data(), 0, sizeof(D3D12_INPUT_ELEMENT_DESC) * m_input_elements.size());
	m_desc.NodeMask = 1;
	m_desc.SampleMask = 0xFFFFFFFF;
	m_desc.SampleDesc.Count = 1;
}

wil::com_ptr_nothrow<ID3D12PipelineState> GraphicsPipelineBuilder::Create(ID3D12Device* device, bool clear /*= true*/)
{
	wil::com_ptr_nothrow<ID3D12PipelineState> ps;
	HRESULT hr = device->CreateGraphicsPipelineState(&m_desc, IID_PPV_ARGS(ps.put()));
	if (FAILED(hr))
	{
		Console.Error("CreateGraphicsPipelineState() failed: %08X", hr);
		return {};
	}

	if (clear)
		Clear();

	return ps;
}

wil::com_ptr_nothrow<ID3D12PipelineState> GraphicsPipelineBuilder::Create(ID3D12Device* device, ShaderCache& cache,
	bool clear /*= true*/)
{
	wil::com_ptr_nothrow<ID3D12PipelineState> pso = cache.GetPipelineState(device, m_desc);
	if (!pso)
		return {};

	if (clear)
		Clear();

	return pso;
}

void GraphicsPipelineBuilder::SetRootSignature(ID3D12RootSignature* rs)
{
	m_desc.pRootSignature = rs;
}

void GraphicsPipelineBuilder::SetVertexShader(const ID3DBlob* blob)
{
	SetVertexShader(const_cast<ID3DBlob*>(blob)->GetBufferPointer(), static_cast<u32>(const_cast<ID3DBlob*>(blob)->GetBufferSize()));
}

void GraphicsPipelineBuilder::SetVertexShader(const void* data, u32 data_size)
{
	m_desc.VS.pShaderBytecode = data;
	m_desc.VS.BytecodeLength = data_size;
}

void GraphicsPipelineBuilder::SetGeometryShader(const ID3DBlob* blob)
{
	SetGeometryShader(const_cast<ID3DBlob*>(blob)->GetBufferPointer(), static_cast<u32>(const_cast<ID3DBlob*>(blob)->GetBufferSize()));
}

void GraphicsPipelineBuilder::SetGeometryShader(const void* data, u32 data_size)
{
	m_desc.GS.pShaderBytecode = data;
	m_desc.GS.BytecodeLength = data_size;
}

void GraphicsPipelineBuilder::SetPixelShader(const ID3DBlob* blob)
{
	SetPixelShader(const_cast<ID3DBlob*>(blob)->GetBufferPointer(), static_cast<u32>(const_cast<ID3DBlob*>(blob)->GetBufferSize()));
}

void GraphicsPipelineBuilder::SetPixelShader(const void* data, u32 data_size)
{
	m_desc.PS.pShaderBytecode = data;
	m_desc.PS.BytecodeLength = data_size;
}

void GraphicsPipelineBuilder::AddVertexAttribute(const char* semantic_name, u32 semantic_index, DXGI_FORMAT format,
	u32 buffer, u32 offset)
{
	const u32 index = m_desc.InputLayout.NumElements;
	m_input_elements[index].SemanticIndex = semantic_index;
	m_input_elements[index].SemanticName = semantic_name;
	m_input_elements[index].Format = format;
	m_input_elements[index].AlignedByteOffset = offset;
	m_input_elements[index].InputSlot = buffer;
	m_input_elements[index].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	m_input_elements[index].InstanceDataStepRate = 0;

	m_desc.InputLayout.pInputElementDescs = m_input_elements.data();
	m_desc.InputLayout.NumElements++;
}

void GraphicsPipelineBuilder::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
{
	m_desc.PrimitiveTopologyType = type;
}

void GraphicsPipelineBuilder::SetRasterizationState(D3D12_FILL_MODE polygon_mode, D3D12_CULL_MODE cull_mode,
	bool front_face_ccw)
{
	m_desc.RasterizerState.FillMode = polygon_mode;
	m_desc.RasterizerState.CullMode = cull_mode;
	m_desc.RasterizerState.FrontCounterClockwise = front_face_ccw;
}

void GraphicsPipelineBuilder::SetMultisamples(u32 multisamples)
{
	m_desc.RasterizerState.MultisampleEnable = multisamples > 1;
	m_desc.SampleDesc.Count = multisamples;
}

void GraphicsPipelineBuilder::SetNoCullRasterizationState()
{
	SetRasterizationState(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false);
}

void GraphicsPipelineBuilder::SetDepthState(bool depth_test, bool depth_write, D3D12_COMPARISON_FUNC compare_op)
{
	m_desc.DepthStencilState.DepthEnable = depth_test;
	m_desc.DepthStencilState.DepthWriteMask = depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	m_desc.DepthStencilState.DepthFunc = compare_op;
}

void GraphicsPipelineBuilder::SetStencilState(bool stencil_test, u8 read_mask, u8 write_mask,
	const D3D12_DEPTH_STENCILOP_DESC& front, const D3D12_DEPTH_STENCILOP_DESC& back)
{
	m_desc.DepthStencilState.StencilEnable = stencil_test;
	m_desc.DepthStencilState.StencilReadMask = read_mask;
	m_desc.DepthStencilState.StencilWriteMask = write_mask;
	m_desc.DepthStencilState.FrontFace = front;
	m_desc.DepthStencilState.BackFace = back;
}

void GraphicsPipelineBuilder::SetNoDepthTestState()
{
	SetDepthState(false, false, D3D12_COMPARISON_FUNC_ALWAYS);
}

void GraphicsPipelineBuilder::SetNoStencilState()
{
	D3D12_DEPTH_STENCILOP_DESC empty = {};
	SetStencilState(false, 0, 0, empty, empty);
}

void GraphicsPipelineBuilder::SetBlendState(u32 rt, bool blend_enable, D3D12_BLEND src_factor, D3D12_BLEND dst_factor,
	D3D12_BLEND_OP op, D3D12_BLEND alpha_src_factor,
	D3D12_BLEND alpha_dst_factor, D3D12_BLEND_OP alpha_op,
	u8 write_mask /*= 0xFF*/)
{
	m_desc.BlendState.RenderTarget[rt].BlendEnable = blend_enable;
	m_desc.BlendState.RenderTarget[rt].SrcBlend = src_factor;
	m_desc.BlendState.RenderTarget[rt].DestBlend = dst_factor;
	m_desc.BlendState.RenderTarget[rt].BlendOp = op;
	m_desc.BlendState.RenderTarget[rt].SrcBlendAlpha = alpha_src_factor;
	m_desc.BlendState.RenderTarget[rt].DestBlendAlpha = alpha_dst_factor;
	m_desc.BlendState.RenderTarget[rt].BlendOpAlpha = alpha_op;
	m_desc.BlendState.RenderTarget[rt].RenderTargetWriteMask = write_mask;

	if (rt > 0)
		m_desc.BlendState.IndependentBlendEnable = TRUE;
}

void GraphicsPipelineBuilder::SetNoBlendingState()
{
	SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD, D3D12_COLOR_WRITE_ENABLE_ALL);
	m_desc.BlendState.IndependentBlendEnable = FALSE;
}

void GraphicsPipelineBuilder::ClearRenderTargets()
{
	m_desc.NumRenderTargets = 0;
	for (u32 i = 0; i < sizeof(m_desc.RTVFormats) / sizeof(m_desc.RTVFormats[0]); i++)
		m_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
}

void GraphicsPipelineBuilder::SetRenderTarget(u32 rt, DXGI_FORMAT format)
{
	m_desc.RTVFormats[rt] = format;
	if (rt >= m_desc.NumRenderTargets)
		m_desc.NumRenderTargets = rt + 1;
}

void GraphicsPipelineBuilder::ClearDepthStencilFormat()
{
	m_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
}

void GraphicsPipelineBuilder::SetDepthStencilFormat(DXGI_FORMAT format)
{
	m_desc.DSVFormat = format;
}

RootSignatureBuilder::RootSignatureBuilder()
{
	Clear();
}

void RootSignatureBuilder::Clear()
{
	m_desc = {};
	m_desc.pParameters = m_params.data();
	m_params = {};
	m_descriptor_ranges = {};
	m_num_descriptor_ranges = 0;
}

wil::com_ptr_nothrow<ID3D12RootSignature> RootSignatureBuilder::Create(bool clear /*= true*/)
{
	wil::com_ptr_nothrow<ID3D12RootSignature> rs = g_d3d12_context->CreateRootSignature(&m_desc);
	if (!rs)
		return {};

	if (clear)
		Clear();

	return rs;
}

void RootSignatureBuilder::SetInputAssemblerFlag()
{
	m_desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

u32 RootSignatureBuilder::Add32BitConstants(u32 shader_reg, u32 num_values, D3D12_SHADER_VISIBILITY visibility)
{
	const u32 index = m_desc.NumParameters++;

	m_params[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	m_params[index].ShaderVisibility = visibility;
	m_params[index].Constants.ShaderRegister = shader_reg;
	m_params[index].Constants.RegisterSpace = 0;
	m_params[index].Constants.Num32BitValues = num_values;

	return index;
}

u32 RootSignatureBuilder::AddCBVParameter(u32 shader_reg, D3D12_SHADER_VISIBILITY visibility)
{
	const u32 index = m_desc.NumParameters++;

	m_params[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	m_params[index].ShaderVisibility = visibility;
	m_params[index].Descriptor.ShaderRegister = shader_reg;
	m_params[index].Descriptor.RegisterSpace = 0;

	return index;
}

u32 RootSignatureBuilder::AddSRVParameter(u32 shader_reg, D3D12_SHADER_VISIBILITY visibility)
{
	const u32 index = m_desc.NumParameters++;

	m_params[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	m_params[index].ShaderVisibility = visibility;
	m_params[index].Descriptor.ShaderRegister = shader_reg;
	m_params[index].Descriptor.RegisterSpace = 0;

	return index;
}

u32 RootSignatureBuilder::AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE rt, u32 start_shader_reg, u32 num_shader_regs,
	D3D12_SHADER_VISIBILITY visibility)
{
	const u32 index = m_desc.NumParameters++;
	const u32 dr_index = m_num_descriptor_ranges++;

	m_descriptor_ranges[dr_index].RangeType = rt;
	m_descriptor_ranges[dr_index].NumDescriptors = num_shader_regs;
	m_descriptor_ranges[dr_index].BaseShaderRegister = start_shader_reg;
	m_descriptor_ranges[dr_index].RegisterSpace = 0;
	m_descriptor_ranges[dr_index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	m_params[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	m_params[index].DescriptorTable.pDescriptorRanges = &m_descriptor_ranges[dr_index];
	m_params[index].DescriptorTable.NumDescriptorRanges = 1;
	m_params[index].ShaderVisibility = visibility;

	return index;
}
