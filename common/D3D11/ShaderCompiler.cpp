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
#include "common/D3D11/ShaderCompiler.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include <array>
#include <d3dcompiler.h>
#include <fstream>

static unsigned s_next_bad_shader_id = 1;

wil::com_ptr_nothrow<ID3DBlob> D3D11::ShaderCompiler::CompileShader(Type type, D3D_FEATURE_LEVEL feature_level, bool debug,
	const std::string_view& code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	const char* target;
	switch (feature_level)
	{
		case D3D_FEATURE_LEVEL_10_0:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_4_0", "gs_4_0", "ps_4_0", "cs_4_0"}};
			target = targets[static_cast<int>(type)];
		}
		break;

		case D3D_FEATURE_LEVEL_10_1:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_4_1", "gs_4_1", "ps_4_1", "cs_4_1"}};
			target = targets[static_cast<int>(type)];
		}
		break;

		case D3D_FEATURE_LEVEL_11_0:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_5_0", "gs_5_0", "ps_5_0", "cs_5_0"}};
			target = targets[static_cast<int>(type)];
		}
		break;

		case D3D_FEATURE_LEVEL_11_1:
		default:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_5_1", "gs_5_1", "ps_5_1", "cs_5_1"}};
			target = targets[static_cast<int>(type)];
		}
		break;
	}

	static constexpr UINT flags_non_debug = D3DCOMPILE_OPTIMIZATION_LEVEL3;
	static constexpr UINT flags_debug = D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;

	wil::com_ptr_nothrow<ID3DBlob> blob;
	wil::com_ptr_nothrow<ID3DBlob> error_blob;
	const HRESULT hr =
		D3DCompile(code.data(), code.size(), "0", macros, nullptr, entry_point, target, debug ? flags_debug : flags_non_debug,
			0, blob.put(), error_blob.put());

	std::string error_string;
	if (error_blob)
	{
		error_string.append(static_cast<const char*>(error_blob->GetBufferPointer()), error_blob->GetBufferSize());
		error_blob.reset();
	}

	if (FAILED(hr))
	{
		Console.WriteLn("Failed to compile '%s':\n%s", target, error_string.c_str());

		std::ofstream ofs(StringUtil::StdStringFromFormat("bad_shader_%u.txt", s_next_bad_shader_id++).c_str(),
			std::ofstream::out | std::ofstream::binary);
		if (ofs.is_open())
		{
			ofs << code;
			ofs << "\n\nCompile as " << target << " failed: " << hr << "\n";
			ofs.write(error_string.c_str(), error_string.size());
			ofs.close();
		}

		return {};
	}

	if (!error_string.empty())
		Console.Warning("'%s' compiled with warnings:\n%s", target, error_string.c_str());

	return blob;
}

wil::com_ptr_nothrow<ID3D11VertexShader> D3D11::ShaderCompiler::CompileAndCreateVertexShader(ID3D11Device* device, bool debug,
	const std::string_view& code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = CompileShader(Type::Vertex, device->GetFeatureLevel(), debug, code, macros, entry_point);
	if (!blob)
		return {};

	return CreateVertexShader(device, blob.get());
}

wil::com_ptr_nothrow<ID3D11GeometryShader> D3D11::ShaderCompiler::CompileAndCreateGeometryShader(ID3D11Device* device, bool debug,
	const std::string_view& code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = CompileShader(Type::Geometry, device->GetFeatureLevel(), debug, code, macros, entry_point);
	if (!blob)
		return {};

	return CreateGeometryShader(device, blob.get());
}

wil::com_ptr_nothrow<ID3D11PixelShader> D3D11::ShaderCompiler::CompileAndCreatePixelShader(ID3D11Device* device, bool debug,
	const std::string_view& code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = CompileShader(Type::Pixel, device->GetFeatureLevel(), debug, code, macros, entry_point);
	if (!blob)
		return {};

	return CreatePixelShader(device, blob.get());
}

wil::com_ptr_nothrow<ID3D11ComputeShader> D3D11::ShaderCompiler::CompileAndCreateComputeShader(ID3D11Device* device, bool debug,
	const std::string_view& code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = CompileShader(Type::Compute, device->GetFeatureLevel(), debug, code, macros, entry_point);
	if (!blob)
		return {};

	return CreateComputeShader(device, blob.get());
}

wil::com_ptr_nothrow<ID3D11VertexShader> D3D11::ShaderCompiler::CreateVertexShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length)
{
	wil::com_ptr_nothrow<ID3D11VertexShader> shader;
	const HRESULT hr = device->CreateVertexShader(bytecode, bytecode_length, nullptr, shader.put());
	if (FAILED(hr))
	{
		Console.Error("Failed to create vertex shader: 0x%08X", hr);
		return {};
	}

	return shader;
}

wil::com_ptr_nothrow<ID3D11VertexShader> D3D11::ShaderCompiler::CreateVertexShader(ID3D11Device* device, const ID3DBlob* blob)
{
	return CreateVertexShader(device, const_cast<ID3DBlob*>(blob)->GetBufferPointer(),
		const_cast<ID3DBlob*>(blob)->GetBufferSize());
}

wil::com_ptr_nothrow<ID3D11GeometryShader> D3D11::ShaderCompiler::CreateGeometryShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length)
{
	wil::com_ptr_nothrow<ID3D11GeometryShader> shader;
	const HRESULT hr = device->CreateGeometryShader(bytecode, bytecode_length, nullptr, shader.put());
	if (FAILED(hr))
	{
		Console.Error("Failed to create geometry shader: 0x%08X", hr);
		return {};
	}

	return shader;
}

wil::com_ptr_nothrow<ID3D11GeometryShader> D3D11::ShaderCompiler::CreateGeometryShader(ID3D11Device* device, const ID3DBlob* blob)
{
	return CreateGeometryShader(device, const_cast<ID3DBlob*>(blob)->GetBufferPointer(),
		const_cast<ID3DBlob*>(blob)->GetBufferSize());
}

wil::com_ptr_nothrow<ID3D11PixelShader> D3D11::ShaderCompiler::CreatePixelShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length)
{
	wil::com_ptr_nothrow<ID3D11PixelShader> shader;
	const HRESULT hr = device->CreatePixelShader(bytecode, bytecode_length, nullptr, shader.put());
	if (FAILED(hr))
	{
		Console.Error("Failed to create pixel shader: 0x%08X", hr);
		return {};
	}

	return shader;
}

wil::com_ptr_nothrow<ID3D11PixelShader> D3D11::ShaderCompiler::CreatePixelShader(ID3D11Device* device, const ID3DBlob* blob)
{
	return CreatePixelShader(device, const_cast<ID3DBlob*>(blob)->GetBufferPointer(),
		const_cast<ID3DBlob*>(blob)->GetBufferSize());
}

wil::com_ptr_nothrow<ID3D11ComputeShader> D3D11::ShaderCompiler::CreateComputeShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length)
{
	wil::com_ptr_nothrow<ID3D11ComputeShader> shader;
	const HRESULT hr = device->CreateComputeShader(bytecode, bytecode_length, nullptr, shader.put());
	if (FAILED(hr))
	{
		Console.Error("Failed to create compute shader: 0x%08X", hr);
		return {};
	}

	return shader;
}

wil::com_ptr_nothrow<ID3D11ComputeShader> D3D11::ShaderCompiler::CreateComputeShader(ID3D11Device* device, const ID3DBlob* blob)
{
	return CreateComputeShader(device, const_cast<ID3DBlob*>(blob)->GetBufferPointer(),
		const_cast<ID3DBlob*>(blob)->GetBufferSize());
}
