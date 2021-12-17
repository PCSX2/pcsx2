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

#include "common/PrecompiledHeader.h"

#include "common/GL/Program.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include <array>
#include <fstream>

namespace GL
{
	GLuint Program::s_last_program_id = 0;
	static GLuint s_next_bad_shader_id = 1;

	Program::Program() = default;

	Program::Program(Program&& prog)
	{
		m_program_id = prog.m_program_id;
		prog.m_program_id = 0;
		m_vertex_shader_id = prog.m_vertex_shader_id;
		prog.m_vertex_shader_id = 0;
		m_fragment_shader_id = prog.m_fragment_shader_id;
		prog.m_fragment_shader_id = 0;
		m_uniform_locations = std::move(prog.m_uniform_locations);
	}

	Program::~Program()
	{
		Destroy();
	}

	GLuint Program::CompileShader(GLenum type, const std::string_view source)
	{
		GLuint id = glCreateShader(type);

		std::array<const GLchar*, 1> sources = {{source.data()}};
		std::array<GLint, 1> source_lengths = {{static_cast<GLint>(source.size())}};
		glShaderSource(id, static_cast<GLsizei>(sources.size()), sources.data(), source_lengths.data());
		glCompileShader(id);

		GLint status = GL_FALSE;
		glGetShaderiv(id, GL_COMPILE_STATUS, &status);

		GLint info_log_length = 0;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &info_log_length);

		if (status == GL_FALSE || info_log_length > 0)
		{
			std::string info_log;
			info_log.resize(info_log_length + 1);
			glGetShaderInfoLog(id, info_log_length, &info_log_length, &info_log[0]);

			if (status == GL_TRUE)
			{
				Console.Warning("Shader compiled with warnings:\n%s", info_log.c_str());
			}
			else
			{
				Console.Error("Shader failed to compile:\n%s", info_log.c_str());

				std::ofstream ofs(StringUtil::StdStringFromFormat("bad_shader_%u.txt", s_next_bad_shader_id++).c_str(),
					std::ofstream::out | std::ofstream::binary);
				if (ofs.is_open())
				{
					ofs.write(sources[0], source_lengths[0]);
					ofs << "\n\nCompile failed, info log:\n";
					ofs << info_log;
					ofs.close();
				}

				glDeleteShader(id);
				return 0;
			}
		}

		return id;
	}

	void Program::ResetLastProgram()
	{
		s_last_program_id = 0;
	}

	bool Program::Compile(const std::string_view vertex_shader, const std::string_view geometry_shader,
		const std::string_view fragment_shader)
	{
		GLuint vertex_shader_id = 0;
		if (!vertex_shader.empty())
		{
			vertex_shader_id = CompileShader(GL_VERTEX_SHADER, vertex_shader);
			if (vertex_shader_id == 0)
				return false;
		}

		GLuint geometry_shader_id = 0;
		if (!geometry_shader.empty())
		{
			geometry_shader_id = CompileShader(GL_GEOMETRY_SHADER, geometry_shader);
			if (geometry_shader_id == 0)
				return false;
		}

		GLuint fragment_shader_id = 0;
		if (!fragment_shader.empty())
		{
			fragment_shader_id = CompileShader(GL_FRAGMENT_SHADER, fragment_shader);
			if (fragment_shader_id == 0)
			{
				glDeleteShader(vertex_shader_id);
				return false;
			}
		}

		m_program_id = glCreateProgram();
		if (vertex_shader_id != 0)
			glAttachShader(m_program_id, vertex_shader_id);
		if (geometry_shader_id != 0)
			glAttachShader(m_program_id, geometry_shader_id);
		if (fragment_shader_id != 0)
			glAttachShader(m_program_id, fragment_shader_id);
		return true;
	}

	bool Program::CreateFromBinary(const void* data, u32 data_length, u32 data_format)
	{
		GLuint prog = glCreateProgram();
		glProgramBinary(prog, static_cast<GLenum>(data_format), data, data_length);

		GLint link_status;
		glGetProgramiv(prog, GL_LINK_STATUS, &link_status);
		if (link_status != GL_TRUE)
		{
			Console.Error("Failed to create GL program from binary: status %d", link_status);
			glDeleteProgram(prog);
			return false;
		}

		m_program_id = prog;
		return true;
	}

	bool Program::GetBinary(std::vector<u8>* out_data, u32* out_data_format)
	{
		GLint binary_size = 0;
		glGetProgramiv(m_program_id, GL_PROGRAM_BINARY_LENGTH, &binary_size);
		if (binary_size == 0)
		{
			Console.Warning("glGetProgramiv(GL_PROGRAM_BINARY_LENGTH) returned 0");
			return false;
		}

		GLenum format = 0;
		out_data->resize(static_cast<size_t>(binary_size));
		glGetProgramBinary(m_program_id, binary_size, &binary_size, &format, out_data->data());
		if (binary_size == 0)
		{
			Console.Warning("glGetProgramBinary() failed");
			return false;
		}
		else if (static_cast<size_t>(binary_size) != out_data->size())
		{
			Console.Warning("Size changed from %zu to %d after glGetProgramBinary()", out_data->size(), binary_size);
			out_data->resize(static_cast<size_t>(binary_size));
		}

		*out_data_format = static_cast<u32>(format);
		DevCon.WriteLn("Program binary retrieved, %zu bytes, format %u", out_data->size(), *out_data_format);
		return true;
	}

	void Program::SetBinaryRetrievableHint()
	{
		glProgramParameteri(m_program_id, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	}

	void Program::BindAttribute(GLuint index, const char* name)
	{
		glBindAttribLocation(m_program_id, index, name);
	}

	void Program::BindDefaultAttributes()
	{
		BindAttribute(0, "a_position");
		BindAttribute(1, "a_texcoord");
		BindAttribute(2, "a_color");
	}

	void Program::BindFragData(GLuint index /*= 0*/, const char* name /*= "o_col0"*/)
	{
		glBindFragDataLocation(m_program_id, index, name);
	}

	void Program::BindFragDataIndexed(GLuint color_number /*= 0*/, const char* name /*= "o_col0"*/)
	{
		if (GLAD_GL_VERSION_3_3 || GLAD_GL_ARB_blend_func_extended)
		{
			glBindFragDataLocationIndexed(m_program_id, color_number, 0, name);
			return;
		}
		else if (GLAD_GL_EXT_blend_func_extended)
		{
			glBindFragDataLocationIndexedEXT(m_program_id, color_number, 0, name);
			return;
		}

		Console.Error("BindFragDataIndexed() called without ARB or EXT extension, we'll probably crash.");
		glBindFragDataLocationIndexed(m_program_id, color_number, 0, name);
	}

	bool Program::Link()
	{
		glLinkProgram(m_program_id);

		if (m_vertex_shader_id != 0)
			glDeleteShader(m_vertex_shader_id);
		m_vertex_shader_id = 0;
		if (m_fragment_shader_id != 0)
			glDeleteShader(m_fragment_shader_id);
		m_fragment_shader_id = 0;

		GLint status = GL_FALSE;
		glGetProgramiv(m_program_id, GL_LINK_STATUS, &status);

		GLint info_log_length = 0;
		glGetProgramiv(m_program_id, GL_INFO_LOG_LENGTH, &info_log_length);

		if (status == GL_FALSE || info_log_length > 0)
		{
			std::string info_log;
			info_log.resize(info_log_length + 1);
			glGetProgramInfoLog(m_program_id, info_log_length, &info_log_length, &info_log[0]);

			if (status == GL_TRUE)
			{
				Console.Error("Program linked with warnings:\n%s", info_log.c_str());
			}
			else
			{
				Console.Error("Program failed to link:\n%s", info_log.c_str());
				glDeleteProgram(m_program_id);
				m_program_id = 0;
				return false;
			}
		}

		return true;
	}

	void Program::Bind() const
	{
		if (s_last_program_id == m_program_id)
			return;

		glUseProgram(m_program_id);
		s_last_program_id = m_program_id;
	}

	void Program::Destroy()
	{
		if (m_vertex_shader_id != 0)
		{
			glDeleteShader(m_vertex_shader_id);
			m_vertex_shader_id = 0;
		}
		if (m_fragment_shader_id != 0)
		{
			glDeleteShader(m_fragment_shader_id);
			m_fragment_shader_id = 0;
		}
		if (m_program_id != 0)
		{
			glDeleteProgram(m_program_id);
			m_program_id = 0;
		}

		m_uniform_locations.clear();
	}

	int Program::RegisterUniform(const char* name)
	{
		int id = static_cast<int>(m_uniform_locations.size());
		m_uniform_locations.push_back(glGetUniformLocation(m_program_id, name));
		return id;
	}

	void Program::Uniform1ui(int index, u32 x) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform1ui(location, x);
	}

	void Program::Uniform2ui(int index, u32 x, u32 y) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform2ui(location, x, y);
	}

	void Program::Uniform3ui(int index, u32 x, u32 y, u32 z) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform3ui(location, x, y, z);
	}

	void Program::Uniform4ui(int index, u32 x, u32 y, u32 z, u32 w) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform4ui(location, x, y, z, w);
	}

	void Program::Uniform1i(int index, s32 x) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform1i(location, x);
	}

	void Program::Uniform2i(int index, s32 x, s32 y) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform2i(location, x, y);
	}

	void Program::Uniform3i(int index, s32 x, s32 y, s32 z) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform3i(location, x, y, z);
	}

	void Program::Uniform4i(int index, s32 x, s32 y, s32 z, s32 w) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform4i(location, x, y, z, w);
	}

	void Program::Uniform1f(int index, float x) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform1f(location, x);
	}

	void Program::Uniform2f(int index, float x, float y) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform2f(location, x, y);
	}

	void Program::Uniform3f(int index, float x, float y, float z) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform3f(location, x, y, z);
	}

	void Program::Uniform4f(int index, float x, float y, float z, float w) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform4f(location, x, y, z, w);
	}

	void Program::Uniform2uiv(int index, const u32* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform2uiv(location, 1, v);
	}

	void Program::Uniform3uiv(int index, const u32* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform3uiv(location, 1, v);
	}

	void Program::Uniform4uiv(int index, const u32* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform4uiv(location, 1, v);
	}

	void Program::Uniform2iv(int index, const s32* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform2iv(location, 1, v);
	}

	void Program::Uniform3iv(int index, const s32* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform3iv(location, 1, v);
	}

	void Program::Uniform4iv(int index, const s32* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform4iv(location, 1, v);
	}

	void Program::Uniform2fv(int index, const float* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform2fv(location, 1, v);
	}

	void Program::Uniform3fv(int index, const float* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform3fv(location, 1, v);
	}

	void Program::Uniform4fv(int index, const float* v) const
	{
		pxAssert(static_cast<size_t>(index) < m_uniform_locations.size());
		const GLint location = m_uniform_locations[index];
		if (location >= 0)
			glUniform4fv(location, 1, v);
	}

	void Program::Uniform1ui(const char* name, u32 x) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform1ui(location, x);
	}

	void Program::Uniform2ui(const char* name, u32 x, u32 y) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform2ui(location, x, y);
	}

	void Program::Uniform3ui(const char* name, u32 x, u32 y, u32 z) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform3ui(location, x, y, z);
	}

	void Program::Uniform4ui(const char* name, u32 x, u32 y, u32 z, u32 w) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform4ui(location, x, y, z, w);
	}

	void Program::Uniform1i(const char* name, s32 x) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform1i(location, x);
	}

	void Program::Uniform2i(const char* name, s32 x, s32 y) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform2i(location, x, y);
	}

	void Program::Uniform3i(const char* name, s32 x, s32 y, s32 z) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform3i(location, x, y, z);
	}

	void Program::Uniform4i(const char* name, s32 x, s32 y, s32 z, s32 w) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform4i(location, x, y, z, w);
	}

	void Program::Uniform1f(const char* name, float x) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform1f(location, x);
	}

	void Program::Uniform2f(const char* name, float x, float y) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform2f(location, x, y);
	}

	void Program::Uniform3f(const char* name, float x, float y, float z) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform3f(location, x, y, z);
	}

	void Program::Uniform4f(const char* name, float x, float y, float z, float w) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform4f(location, x, y, z, w);
	}

	void Program::Uniform2uiv(const char* name, const u32* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform2uiv(location, 1, v);
	}

	void Program::Uniform3uiv(const char* name, const u32* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform3uiv(location, 1, v);
	}

	void Program::Uniform4uiv(const char* name, const u32* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform4uiv(location, 1, v);
	}

	void Program::Uniform2iv(const char* name, const s32* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform2iv(location, 1, v);
	}

	void Program::Uniform3iv(const char* name, const s32* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform3iv(location, 1, v);
	}

	void Program::Uniform4iv(const char* name, const s32* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform4iv(location, 1, v);
	}

	void Program::Uniform2fv(const char* name, const float* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform2fv(location, 1, v);
	}

	void Program::Uniform3fv(const char* name, const float* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform3fv(location, 1, v);
	}

	void Program::Uniform4fv(const char* name, const float* v) const
	{
		const GLint location = glGetUniformLocation(m_program_id, name);
		if (location >= 0)
			glUniform4fv(location, 1, v);
	}

	void Program::BindUniformBlock(const char* name, u32 index)
	{
		const GLint location = glGetUniformBlockIndex(m_program_id, name);
		if (location >= 0)
			glUniformBlockBinding(m_program_id, location, index);
	}

	void Program::SetName(const std::string_view& name)
	{
		if (name.empty())
			return;

#ifdef _DEBUG
		glObjectLabel(GL_PROGRAM, m_program_id, name.length(), name.data());
#endif
	}

	void Program::SetFormattedName(const char* format, ...)
	{
		va_list ap;
		va_start(ap, format);
		std::string n = StringUtil::StdStringFromFormatV(format, ap);
		va_end(ap);
		SetName(n);
	}

	Program& Program::operator=(Program&& prog)
	{
		Destroy();
		m_program_id = prog.m_program_id;
		prog.m_program_id = 0;
		m_vertex_shader_id = prog.m_vertex_shader_id;
		prog.m_vertex_shader_id = 0;
		m_fragment_shader_id = prog.m_fragment_shader_id;
		prog.m_fragment_shader_id = 0;
		m_uniform_locations = std::move(prog.m_uniform_locations);
		return *this;
	}
} // namespace GL
