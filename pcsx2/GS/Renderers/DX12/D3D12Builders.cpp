// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/DX12/D3D12Builders.h"
#include "GS/Renderers/DX12/D3D12ShaderCache.h"
#include "GS/Renderers/DX12/GSDevice12.h"
#include "common/Console.h"

#include <cstdarg>
#include <limits>

D3D12::GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
	Clear();
}

void D3D12::GraphicsPipelineBuilder::Clear()
{
	std::memset(&m_desc, 0, sizeof(m_desc));
	std::memset(m_input_elements.data(), 0, sizeof(D3D12_INPUT_ELEMENT_DESC) * m_input_elements.size());
	m_desc.NodeMask = 1;
	m_desc.SampleMask = 0xFFFFFFFF;
	m_desc.SampleDesc.Count = 1;
}

wil::com_ptr_nothrow<ID3D12PipelineState> D3D12::GraphicsPipelineBuilder::Create(
	ID3D12Device* device, bool clear /*= true*/)
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

wil::com_ptr_nothrow<ID3D12PipelineState> D3D12::GraphicsPipelineBuilder::Create(
	ID3D12Device* device, D3D12ShaderCache& cache, bool clear /*= true*/)
{
	wil::com_ptr_nothrow<ID3D12PipelineState> pso = cache.GetPipelineState(device, m_desc);
	if (!pso)
		return {};

	if (clear)
		Clear();

	return pso;
}

void D3D12::GraphicsPipelineBuilder::SetRootSignature(ID3D12RootSignature* rs)
{
	m_desc.pRootSignature = rs;
}

void D3D12::GraphicsPipelineBuilder::SetVertexShader(const ID3DBlob* blob)
{
	SetVertexShader(const_cast<ID3DBlob*>(blob)->GetBufferPointer(),
		static_cast<u32>(const_cast<ID3DBlob*>(blob)->GetBufferSize()));
}

void D3D12::GraphicsPipelineBuilder::SetVertexShader(const void* data, u32 data_size)
{
	m_desc.VS.pShaderBytecode = data;
	m_desc.VS.BytecodeLength = data_size;
}

void D3D12::GraphicsPipelineBuilder::SetGeometryShader(const ID3DBlob* blob)
{
	SetGeometryShader(const_cast<ID3DBlob*>(blob)->GetBufferPointer(),
		static_cast<u32>(const_cast<ID3DBlob*>(blob)->GetBufferSize()));
}

void D3D12::GraphicsPipelineBuilder::SetGeometryShader(const void* data, u32 data_size)
{
	m_desc.GS.pShaderBytecode = data;
	m_desc.GS.BytecodeLength = data_size;
}

void D3D12::GraphicsPipelineBuilder::SetPixelShader(const ID3DBlob* blob)
{
	SetPixelShader(const_cast<ID3DBlob*>(blob)->GetBufferPointer(),
		static_cast<u32>(const_cast<ID3DBlob*>(blob)->GetBufferSize()));
}

void D3D12::GraphicsPipelineBuilder::SetPixelShader(const void* data, u32 data_size)
{
	m_desc.PS.pShaderBytecode = data;
	m_desc.PS.BytecodeLength = data_size;
}

void D3D12::GraphicsPipelineBuilder::AddVertexAttribute(
	const char* semantic_name, u32 semantic_index, DXGI_FORMAT format, u32 buffer, u32 offset)
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

void D3D12::GraphicsPipelineBuilder::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
{
	m_desc.PrimitiveTopologyType = type;
}

void D3D12::GraphicsPipelineBuilder::SetRasterizationState(
	D3D12_FILL_MODE polygon_mode, D3D12_CULL_MODE cull_mode, bool front_face_ccw)
{
	m_desc.RasterizerState.FillMode = polygon_mode;
	m_desc.RasterizerState.CullMode = cull_mode;
	m_desc.RasterizerState.FrontCounterClockwise = front_face_ccw;
}

void D3D12::GraphicsPipelineBuilder::SetMultisamples(u32 multisamples)
{
	m_desc.RasterizerState.MultisampleEnable = multisamples > 1;
	m_desc.SampleDesc.Count = multisamples;
}

void D3D12::GraphicsPipelineBuilder::SetNoCullRasterizationState()
{
	SetRasterizationState(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false);
}

void D3D12::GraphicsPipelineBuilder::SetDepthState(bool depth_test, bool depth_write, D3D12_COMPARISON_FUNC compare_op)
{
	m_desc.DepthStencilState.DepthEnable = depth_test;
	m_desc.DepthStencilState.DepthWriteMask = depth_write ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	m_desc.DepthStencilState.DepthFunc = compare_op;
}

void D3D12::GraphicsPipelineBuilder::SetStencilState(bool stencil_test, u8 read_mask, u8 write_mask,
	const D3D12_DEPTH_STENCILOP_DESC& front, const D3D12_DEPTH_STENCILOP_DESC& back)
{
	m_desc.DepthStencilState.StencilEnable = stencil_test;
	m_desc.DepthStencilState.StencilReadMask = read_mask;
	m_desc.DepthStencilState.StencilWriteMask = write_mask;
	m_desc.DepthStencilState.FrontFace = front;
	m_desc.DepthStencilState.BackFace = back;
}

void D3D12::GraphicsPipelineBuilder::SetNoDepthTestState()
{
	SetDepthState(false, false, D3D12_COMPARISON_FUNC_ALWAYS);
}

void D3D12::GraphicsPipelineBuilder::SetNoStencilState()
{
	D3D12_DEPTH_STENCILOP_DESC empty = {};
	SetStencilState(false, 0, 0, empty, empty);
}

void D3D12::GraphicsPipelineBuilder::SetBlendState(u32 rt, bool blend_enable, D3D12_BLEND src_factor,
	D3D12_BLEND dst_factor, D3D12_BLEND_OP op, D3D12_BLEND alpha_src_factor, D3D12_BLEND alpha_dst_factor,
	D3D12_BLEND_OP alpha_op, u8 write_mask /*= 0xFF*/)
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

void D3D12::GraphicsPipelineBuilder::SetColorWriteMask(u32 rt, u8 write_mask /* = D3D12_COLOR_WRITE_ENABLE_ALL */)
{
	m_desc.BlendState.RenderTarget[rt].RenderTargetWriteMask = write_mask;
}

void D3D12::GraphicsPipelineBuilder::SetNoBlendingState()
{
	SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO,
		D3D12_BLEND_OP_ADD, D3D12_COLOR_WRITE_ENABLE_ALL);
	m_desc.BlendState.IndependentBlendEnable = FALSE;
}

void D3D12::GraphicsPipelineBuilder::ClearRenderTargets()
{
	m_desc.NumRenderTargets = 0;
	for (u32 i = 0; i < sizeof(m_desc.RTVFormats) / sizeof(m_desc.RTVFormats[0]); i++)
		m_desc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
}

void D3D12::GraphicsPipelineBuilder::SetRenderTarget(u32 rt, DXGI_FORMAT format)
{
	m_desc.RTVFormats[rt] = format;
	if (rt >= m_desc.NumRenderTargets)
		m_desc.NumRenderTargets = rt + 1;
}

void D3D12::GraphicsPipelineBuilder::ClearDepthStencilFormat()
{
	m_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
}

void D3D12::GraphicsPipelineBuilder::SetDepthStencilFormat(DXGI_FORMAT format)
{
	m_desc.DSVFormat = format;
}

D3D12::ComputePipelineBuilder::ComputePipelineBuilder()
{
	Clear();
}

void D3D12::ComputePipelineBuilder::Clear()
{
	std::memset(&m_desc, 0, sizeof(m_desc));
}

wil::com_ptr_nothrow<ID3D12PipelineState> D3D12::ComputePipelineBuilder::Create(
	ID3D12Device* device, bool clear /*= true*/)
{
	wil::com_ptr_nothrow<ID3D12PipelineState> ps;
	HRESULT hr = device->CreateComputePipelineState(&m_desc, IID_PPV_ARGS(ps.put()));
	if (FAILED(hr))
	{
		Console.Error("CreateComputePipelineState() failed: %08X", hr);
		return {};
	}

	if (clear)
		Clear();

	return ps;
}

wil::com_ptr_nothrow<ID3D12PipelineState> D3D12::ComputePipelineBuilder::Create(
	ID3D12Device* device, D3D12ShaderCache& cache, bool clear /*= true*/)
{
	wil::com_ptr_nothrow<ID3D12PipelineState> pso = cache.GetPipelineState(device, m_desc);
	if (!pso)
		return {};

	if (clear)
		Clear();

	return pso;
}

void D3D12::ComputePipelineBuilder::SetRootSignature(ID3D12RootSignature* rs)
{
	m_desc.pRootSignature = rs;
}

void D3D12::ComputePipelineBuilder::SetShader(const void* data, u32 data_size)
{
	m_desc.CS.pShaderBytecode = data;
	m_desc.CS.BytecodeLength = data_size;
}

D3D12::RootSignatureBuilder::RootSignatureBuilder()
{
	Clear();
}

void D3D12::RootSignatureBuilder::Clear()
{
	m_desc = {};
	m_desc.pParameters = m_params.data();
	m_params = {};
	m_descriptor_ranges = {};
	m_num_descriptor_ranges = 0;
}

wil::com_ptr_nothrow<ID3D12RootSignature> D3D12::RootSignatureBuilder::Create(bool clear /*= true*/)
{
	wil::com_ptr_nothrow<ID3D12RootSignature> rs = GSDevice12::GetInstance()->CreateRootSignature(&m_desc);
	if (!rs)
		return {};

	if (clear)
		Clear();

	return rs;
}

void D3D12::RootSignatureBuilder::SetInputAssemblerFlag()
{
	m_desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

u32 D3D12::RootSignatureBuilder::Add32BitConstants(u32 shader_reg, u32 num_values, D3D12_SHADER_VISIBILITY visibility)
{
	const u32 index = m_desc.NumParameters++;

	m_params[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	m_params[index].ShaderVisibility = visibility;
	m_params[index].Constants.ShaderRegister = shader_reg;
	m_params[index].Constants.RegisterSpace = 0;
	m_params[index].Constants.Num32BitValues = num_values;

	return index;
}

u32 D3D12::RootSignatureBuilder::AddCBVParameter(u32 shader_reg, D3D12_SHADER_VISIBILITY visibility)
{
	const u32 index = m_desc.NumParameters++;

	m_params[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	m_params[index].ShaderVisibility = visibility;
	m_params[index].Descriptor.ShaderRegister = shader_reg;
	m_params[index].Descriptor.RegisterSpace = 0;

	return index;
}

u32 D3D12::RootSignatureBuilder::AddSRVParameter(u32 shader_reg, D3D12_SHADER_VISIBILITY visibility)
{
	const u32 index = m_desc.NumParameters++;

	m_params[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	m_params[index].ShaderVisibility = visibility;
	m_params[index].Descriptor.ShaderRegister = shader_reg;
	m_params[index].Descriptor.RegisterSpace = 0;

	return index;
}

u32 D3D12::RootSignatureBuilder::AddDescriptorTable(
	D3D12_DESCRIPTOR_RANGE_TYPE rt, u32 start_shader_reg, u32 num_shader_regs, D3D12_SHADER_VISIBILITY visibility)
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

#ifdef PCSX2_DEVBUILD
#include "common/StringUtil.h"

void D3D12::SetObjectName(ID3D12Object* object, std::string_view name)
{
	object->SetName(StringUtil::UTF8StringToWideString(name).c_str());
}

#endif
