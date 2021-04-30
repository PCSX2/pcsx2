/*
 *	Copyright (C) 2011-2013 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

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
