/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GS.h"

class GSShaderOGL
{
	GLuint m_pipeline;
	std::unordered_map<uint32, GLuint> m_program;
	const bool m_debug_shader;

	std::vector<GLuint> m_shad_to_delete;
	std::vector<GLuint> m_prog_to_delete;
	std::vector<GLuint> m_pipe_to_delete;

	bool ValidateShader(GLuint s);
	bool ValidateProgram(GLuint p);
	bool ValidatePipeline(GLuint p);

	std::string GenGlslHeader(const std::string& entry, GLenum type, const std::string& macro);
	std::vector<char> m_common_header;

public:
	GSShaderOGL(bool debug);
	~GSShaderOGL();

	void BindPipeline(GLuint vs, GLuint gs, GLuint ps);
	void BindPipeline(GLuint pipe);

	GLuint Compile(const std::string& glsl_file, const std::string& entry, GLenum type, const char* glsl_h_code, const std::string& macro_sel = "");
	GLuint LinkPipeline(const std::string& pretty_print, GLuint vs, GLuint gs, GLuint ps);

	// Same as above but for not separated build
	void BindProgram(GLuint vs, GLuint gs, GLuint ps);
	void BindProgram(GLuint p);

	GLuint CompileShader(const std::string& glsl_file, const std::string& entry, GLenum type, const char* glsl_h_code, const std::string& macro_sel = "");
	GLuint LinkProgram(GLuint vs, GLuint gs, GLuint ps);

	int DumpAsm(const std::string& file, GLuint p);
};
