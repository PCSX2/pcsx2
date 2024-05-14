// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include "glad/gl.h"

#include <string_view>
#include <vector>

class GLProgram
{
public:
	GLProgram();
	GLProgram(const GLProgram&) = delete;
	GLProgram(GLProgram&& prog);
	~GLProgram();

	static GLuint CompileShader(GLenum type, const std::string_view source);
	static void ResetLastProgram();

	bool IsValid() const { return m_program_id != 0; }

	bool Compile(const std::string_view vertex_shader, const std::string_view fragment_shader);

	bool CompileCompute(const std::string_view glsl);

	bool CreateFromBinary(const void* data, u32 data_length, u32 data_format);

	bool GetBinary(std::vector<u8>* out_data, u32* out_data_format);
	void SetBinaryRetrievableHint();

	void BindAttribute(GLuint index, const char* name);
	void BindDefaultAttributes();

	void BindFragData(GLuint index = 0, const char* name = "o_col0");
	void BindFragDataIndexed(GLuint color_number = 0, const char* name = "o_col0");

	bool Link();

	void Bind() const;

	void Destroy();

	int RegisterUniform(const char* name);
	void Uniform1ui(int index, u32 x) const;
	void Uniform2ui(int index, u32 x, u32 y) const;
	void Uniform3ui(int index, u32 x, u32 y, u32 z) const;
	void Uniform4ui(int index, u32 x, u32 y, u32 z, u32 w) const;
	void Uniform1i(int index, s32 x) const;
	void Uniform2i(int index, s32 x, s32 y) const;
	void Uniform3i(int index, s32 x, s32 y, s32 z) const;
	void Uniform4i(int index, s32 x, s32 y, s32 z, s32 w) const;
	void Uniform1f(int index, float x) const;
	void Uniform2f(int index, float x, float y) const;
	void Uniform3f(int index, float x, float y, float z) const;
	void Uniform4f(int index, float x, float y, float z, float w) const;
	void Uniform2uiv(int index, const u32* v) const;
	void Uniform3uiv(int index, const u32* v) const;
	void Uniform4uiv(int index, const u32* v) const;
	void Uniform2iv(int index, const s32* v) const;
	void Uniform3iv(int index, const s32* v) const;
	void Uniform4iv(int index, const s32* v) const;
	void Uniform2fv(int index, const float* v) const;
	void Uniform3fv(int index, const float* v) const;
	void Uniform4fv(int index, const float* v) const;

	void UniformMatrix2fv(int index, const float* v);
	void UniformMatrix3fv(int index, const float* v);
	void UniformMatrix4fv(int index, const float* v);

	void BindUniformBlock(const char* name, u32 index);

	void SetName(const std::string_view name);
	void SetFormattedName(const char* format, ...);

	GLProgram& operator=(const GLProgram&) = delete;
	GLProgram& operator=(GLProgram&& prog);

	__fi bool operator==(const GLProgram& rhs) const { return m_program_id == rhs.m_program_id; }
	__fi bool operator!=(const GLProgram& rhs) const { return m_program_id != rhs.m_program_id; }

private:
	GLuint m_program_id = 0;
	GLuint m_vertex_shader_id = 0;
	GLuint m_fragment_shader_id = 0;

	std::vector<GLint> m_uniform_locations;
};
