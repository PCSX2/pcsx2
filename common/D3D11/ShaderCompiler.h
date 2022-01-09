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
#include "common/RedtapeWindows.h"
#include <d3d11.h>
#include <string_view>
#include <type_traits>
#include <wil/com.h>

namespace D3D11::ShaderCompiler
{
	enum class Type
	{
		Vertex,
		Geometry,
		Pixel,
		Compute
	};

	wil::com_ptr_nothrow<ID3DBlob> CompileShader(Type type, D3D_FEATURE_LEVEL feature_level, bool debug, const std::string_view& code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

	wil::com_ptr_nothrow<ID3D11VertexShader> CompileAndCreateVertexShader(ID3D11Device* device, bool debug, const std::string_view& code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");
	wil::com_ptr_nothrow<ID3D11GeometryShader> CompileAndCreateGeometryShader(ID3D11Device* device, bool debug, const std::string_view& code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");
	wil::com_ptr_nothrow<ID3D11PixelShader> CompileAndCreatePixelShader(ID3D11Device* device, bool debug, const std::string_view& code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");
	wil::com_ptr_nothrow<ID3D11ComputeShader> CompileAndCreateComputeShader(ID3D11Device* device, bool debug, const std::string_view& code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

	wil::com_ptr_nothrow<ID3D11VertexShader> CreateVertexShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length);
	wil::com_ptr_nothrow<ID3D11VertexShader> CreateVertexShader(ID3D11Device* device, const ID3DBlob* blob);
	wil::com_ptr_nothrow<ID3D11GeometryShader> CreateGeometryShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length);
	wil::com_ptr_nothrow<ID3D11GeometryShader> CreateGeometryShader(ID3D11Device* device, const ID3DBlob* blob);
	wil::com_ptr_nothrow<ID3D11PixelShader> CreatePixelShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length);
	wil::com_ptr_nothrow<ID3D11PixelShader> CreatePixelShader(ID3D11Device* device, const ID3DBlob* blob);
	wil::com_ptr_nothrow<ID3D11ComputeShader> CreateComputeShader(ID3D11Device* device, const void* bytecode, size_t bytecode_length);
	wil::com_ptr_nothrow<ID3D11ComputeShader> CreateComputeShader(ID3D11Device* device, const ID3DBlob* blob);
}; // namespace D3D11::ShaderCompiler
