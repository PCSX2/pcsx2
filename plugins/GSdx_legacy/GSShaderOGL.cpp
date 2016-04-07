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

#include "stdafx.h"
#include "GSShaderOGL.h"
#include "GLState.h"

GSShaderOGL::GSShaderOGL(bool debug) :
	m_pipeline(0),
	m_debug_shader(debug)
{
	m_single_prog.clear();
	if (GLLoader::found_GL_ARB_separate_shader_objects) {
		glGenProgramPipelines(1, &m_pipeline);
		glBindProgramPipeline(m_pipeline);
	}
}

GSShaderOGL::~GSShaderOGL()
{
	if (GLLoader::found_GL_ARB_separate_shader_objects)
		glDeleteProgramPipelines(1, &m_pipeline);

	for (auto it = m_single_prog.begin(); it != m_single_prog.end() ; it++) glDeleteProgram(it->second);
	m_single_prog.clear();
}

void GSShaderOGL::VS(GLuint s)
{
	if (GLState::vs != s)
	{
		GLState::vs = s;
		GLState::dirty_prog = true;
		if (GLLoader::found_GL_ARB_separate_shader_objects)
			glUseProgramStages(m_pipeline, GL_VERTEX_SHADER_BIT, s);
	}
}

void GSShaderOGL::PS(GLuint s)
{
#ifdef _DEBUG
	if (true)
#else
	if (GLState::ps != s)
#endif
	{
		// In debug always sets the program. It allow to replace the program in apitrace easily.
		GLState::ps = s;
		GLState::dirty_prog = true;
		if (GLLoader::found_GL_ARB_separate_shader_objects) {
			glUseProgramStages(m_pipeline, GL_FRAGMENT_SHADER_BIT, s);
		}
	}
}

void GSShaderOGL::GS(GLuint s)
{
	if (GLState::gs != s)
	{
		GLState::gs = s;
		GLState::dirty_prog = true;
		if (GLLoader::found_GL_ARB_separate_shader_objects)
			glUseProgramStages(m_pipeline, GL_GEOMETRY_SHADER_BIT, s);
	}
}

bool GSShaderOGL::ValidateShader(GLuint s)
{
	if (!m_debug_shader) return true;

	GLint status = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (status) return true;

	GLint log_length = 0;
	glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0) {
		char* log = new char[log_length];
		glGetShaderInfoLog(s, log_length, NULL, log);
		fprintf(stderr, "%s", log);
		delete[] log;
	}
	fprintf(stderr, "\n");

	return false;
}

bool GSShaderOGL::ValidateProgram(GLuint p)
{
	if (!m_debug_shader) return true;

	GLint status = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &status);
	if (status) return true;

	GLint log_length = 0;
	glGetProgramiv(p, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0) {
		char* log = new char[log_length];
		glGetProgramInfoLog(p, log_length, NULL, log);
		fprintf(stderr, "%s", log);
		delete[] log;
	}
	fprintf(stderr, "\n");

	return false;
}

bool GSShaderOGL::ValidatePipeline(GLuint p)
{
	if (!m_debug_shader) return true;

	// FIXME: might be mandatory to validate the pipeline
	glValidateProgramPipeline(p);

	GLint status = 0;
	glGetProgramPipelineiv(p, GL_VALIDATE_STATUS, &status);
	if (status) return true;

	GLint log_length = 0;
	glGetProgramPipelineiv(p, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0) {
		char* log = new char[log_length];
		glGetProgramPipelineInfoLog(p, log_length, NULL, log);
		fprintf(stderr, "%s", log);
		delete[] log;
	}
	fprintf(stderr, "\n");

	return false;
}

GLuint GSShaderOGL::LinkNewProgram()
{
	GLuint p = glCreateProgram();
	if (GLState::vs) glAttachShader(p, GLState::vs);
	if (GLState::ps) glAttachShader(p, GLState::ps);
	if (GLState::gs) glAttachShader(p, GLState::gs);

	glLinkProgram(p);

	ValidateProgram(p);

	return p;
}

void GSShaderOGL::UseProgram()
{
	if (GLState::dirty_prog) {
		if (!GLLoader::found_GL_ARB_separate_shader_objects) {
			hash_map<uint64, GLuint >::iterator it;
			// Note: shader are integer lookup pointer. They start from 1 and incr
			// every time you create a new shader OR a new program.
			// Note2: vs & gs are precompiled at startup. FGLRX and radeon got value < 128. GS has only 2 programs
			// We migth be able to pack the value in a 32bits int
			// I would need to check the behavior on Nvidia (pause/resume).
			uint64 sel = (uint64)GLState::vs << 40 | (uint64)GLState::gs << 20 | GLState::ps;
			it = m_single_prog.find(sel);
			if (it == m_single_prog.end()) {
				GLState::program = LinkNewProgram();
				m_single_prog[sel] = GLState::program;

				ValidateProgram(GLState::program);

				glUseProgram(GLState::program);
			} else {
				GLuint prog = it->second;
				if (prog != GLState::program) {
					GLState::program = prog;
					glUseProgram(GLState::program);
				}
			}
		}
	}

	GLState::dirty_prog = false;
}

std::string GSShaderOGL::GenGlslHeader(const std::string& entry, GLenum type, const std::string& macro)
{
	std::string header;
	header = "#version 330 core\n";
	// Need GL version 420
	header += "#extension GL_ARB_shading_language_420pack: require\n";
	if (GLLoader::found_GL_ARB_separate_shader_objects) {
		// Need GL version 410
		header += "#extension GL_ARB_separate_shader_objects: require\n";
	}
	if (GLLoader::found_GL_ARB_shader_image_load_store) {
		// Need GL version 420
		header += "#extension GL_ARB_shader_image_load_store: require\n";
	} else {
		header += "#define DISABLE_GL42_image\n";
	}
	if (GLLoader::found_GL_ARB_clip_control) {
		header += "#define ZERO_TO_ONE_DEPTH\n";
	}

	// Stupid GL implementation (can't use GL_ES)
	// AMD/nvidia define it to 0
	// intel window don't define it
	// intel linux refuse to define it
	header += "#define pGL_ES 0\n";

	// Allow to puts several shader in 1 files
	switch (type) {
		case GL_VERTEX_SHADER:
			header += "#define VERTEX_SHADER 1\n";
			break;
		case GL_GEOMETRY_SHADER:
			header += "#define GEOMETRY_SHADER 1\n";
			break;
		case GL_FRAGMENT_SHADER:
			header += "#define FRAGMENT_SHADER 1\n";
			break;
		default: ASSERT(0);
	}

	// Select the entry point ie the main function
	header += format("#define %s main\n", entry.c_str());

	header += macro;

	return header;
}

GLuint GSShaderOGL::Compile(const std::string& glsl_file, const std::string& entry, GLenum type, const char* glsl_h_code, const std::string& macro_sel)
{
	ASSERT(glsl_h_code != NULL);

	GLuint program = 0;

	if (type == GL_GEOMETRY_SHADER && !GLLoader::found_geometry_shader) {
		return program;
	}

	// Note it is better to separate header and source file to have the good line number
	// in the glsl compiler report
	const char* sources[2];

	std::string header = GenGlslHeader(entry, type, macro_sel);
	int shader_nb = 1;
#if 1
	sources[0] = header.c_str();
	sources[1] = glsl_h_code;
	shader_nb++;
#else
	sources[0] = header.append(glsl_h_code).c_str();
#endif

	if (GLLoader::found_GL_ARB_separate_shader_objects) {
		program = glCreateShaderProgramv(type, shader_nb, sources);
	} else {
		program = glCreateShader(type);
		glShaderSource(program, shader_nb, sources, NULL);
		glCompileShader(program);
	}

	bool status;
	if (GLLoader::found_GL_ARB_separate_shader_objects)
		status = ValidateProgram(program);
	else
		status = ValidateShader(program);

	if (!status) {
		// print extra info
		fprintf(stderr, "%s (entry %s, prog %d) :", glsl_file.c_str(), entry.c_str(), program);
		fprintf(stderr, "\n%s", macro_sel.c_str());
		fprintf(stderr, "\n");
	}
	return program;
}

// This function will get the binary program. Normally it must be used a caching
// solution but Nvidia also incorporates the ASM dump. Asm is nice because it allow
// to have an overview of the program performance based on the instruction number
// Note: initially I was using cg offline compiler but it doesn't support latest
// GLSL improvement (unfortunately).
int GSShaderOGL::DumpAsm(const std::string& file, GLuint p)
{
	if (!GLLoader::nvidia_buggy_driver) return 0;

	GLint   binaryLength;
	glGetProgramiv(p, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

	char* binary = new char[binaryLength+4];
	GLenum binaryFormat;
	glGetProgramBinary(p, binaryLength, NULL, &binaryFormat, binary);

	FILE* outfile = fopen(file.c_str(), "w");
	ASSERT(outfile);

	// Search the magic number "!!"
	int asm_ = 0;
	while (asm_ < binaryLength && (binary[asm_] != '!' || binary[asm_+1] != '!')) {
		asm_ += 1;
	}

	int instructions = -1;
	if (asm_ < binaryLength) {
		// Now print asm as text
		char* asm_txt = strtok(&binary[asm_], "\n");
		while (asm_txt != NULL && (strncmp(asm_txt, "END", 3) || !strncmp(asm_txt, "ENDIF", 5))) {
			if (!strncmp(asm_txt, "OUT", 3) || !strncmp(asm_txt, "TEMP", 4) || !strncmp(asm_txt, "LONG", 4)) {
				instructions = 0;
			} else if (instructions >= 0) {
				if (instructions == 0)
					fprintf(outfile, "\n");
				instructions++;
			}

			fprintf(outfile, "%s\n", asm_txt);
			asm_txt = strtok(NULL, "\n");
		}
		fprintf(outfile, "\nFound %d instructions\n", instructions);
	}
	fclose(outfile);

	if (instructions < 0) {
		// RAW dump in case of error
		fprintf(stderr, "Error: failed to find the number of instructions!\n");
		outfile = fopen(file.c_str(), "wb");
		fwrite(binary, binaryLength, 1, outfile);
		fclose(outfile);
		ASSERT(0);
	}

	delete[] binary;

	return instructions;
}

void GSShaderOGL::Delete(GLuint s)
{
	if (GLLoader::found_GL_ARB_separate_shader_objects) {
		glDeleteProgram(s);
	} else {
		glDeleteShader(s);
	}
}
