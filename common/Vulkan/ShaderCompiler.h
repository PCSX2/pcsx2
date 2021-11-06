/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include <optional>
#include <string_view>
#include <vector>

namespace Vulkan::ShaderCompiler
{
	// Shader types
	enum class Type
	{
		Vertex,
		Geometry,
		Fragment,
		Compute
	};

	void DeinitializeGlslang();

	// SPIR-V compiled code type
	using SPIRVCodeType = u32;
	using SPIRVCodeVector = std::vector<SPIRVCodeType>;

	// Compile a vertex shader to SPIR-V.
	std::optional<SPIRVCodeVector> CompileVertexShader(std::string_view source_code);

	// Compile a geometry shader to SPIR-V.
	std::optional<SPIRVCodeVector> CompileGeometryShader(std::string_view source_code);

	// Compile a fragment shader to SPIR-V.
	std::optional<SPIRVCodeVector> CompileFragmentShader(std::string_view source_code);

	// Compile a compute shader to SPIR-V.
	std::optional<SPIRVCodeVector> CompileComputeShader(std::string_view source_code);

	std::optional<SPIRVCodeVector> CompileShader(Type type, std::string_view source_code, bool debug);
} // namespace Vulkan::ShaderCompiler
