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

#include "PrecompiledHeader.h"
#include "GSShaderOGL.h"
#include "GLState.h"
#include "GS/GS.h"

#ifdef _WIN32
#include "GS/resource.h"
#else
#include "GS_res.h"
#endif

GSShaderOGL::GSShaderOGL(bool debug)
	: m_pipeline(0)
	, m_debug_shader(debug)
{
	theApp.LoadResource(IDR_COMMON_GLSL, m_common_header);

	// Create a default pipeline
	m_pipeline = LinkPipeline("HW pipe", 0, 0, 0);
	BindPipeline(m_pipeline);
}

GSShaderOGL::~GSShaderOGL()
{
	printf("Delete %zu Shaders, %zu Programs, %zu Pipelines\n",
		m_shad_to_delete.size(), m_prog_to_delete.size(), m_pipe_to_delete.size());

	for (auto s : m_shad_to_delete)
		glDeleteShader(s);
	for (auto p : m_prog_to_delete)
		glDeleteProgram(p);
	glDeleteProgramPipelines(m_pipe_to_delete.size(), &m_pipe_to_delete[0]);
}

GLuint GSShaderOGL::LinkPipeline(const std::string& pretty_print, GLuint vs, GLuint gs, GLuint ps)
{
	GLuint p;
	glCreateProgramPipelines(1, &p);
	glUseProgramStages(p, GL_VERTEX_SHADER_BIT, vs);
	glUseProgramStages(p, GL_GEOMETRY_SHADER_BIT, gs);
	glUseProgramStages(p, GL_FRAGMENT_SHADER_BIT, ps);

	glObjectLabel(GL_PROGRAM_PIPELINE, p, pretty_print.size(), pretty_print.c_str());

	m_pipe_to_delete.push_back(p);

	return p;
}

GLuint GSShaderOGL::LinkProgram(GLuint vs, GLuint gs, GLuint ps)
{
	u32 hash = ((vs ^ gs) << 24) ^ ps;
	auto it = m_program.find(hash);
	if (it != m_program.end())
		return it->second;

	GLuint p = glCreateProgram();
	if (vs) glAttachShader(p, vs);
	if (ps) glAttachShader(p, ps);
	if (gs) glAttachShader(p, gs);

	glLinkProgram(p);

	ValidateProgram(p);

	m_prog_to_delete.push_back(p);
	m_program[hash] = p;

	return p;
}

void GSShaderOGL::BindProgram(GLuint vs, GLuint gs, GLuint ps)
{
	GLuint p = LinkProgram(vs, gs, ps);

	if (GLState::program != p)
	{
		GLState::program = p;
		glUseProgram(p);
	}
}

void GSShaderOGL::BindProgram(GLuint p)
{
	if (GLState::program != p)
	{
		GLState::program = p;
		glUseProgram(p);
	}
}

void GSShaderOGL::BindPipeline(GLuint vs, GLuint gs, GLuint ps)
{
	BindPipeline(m_pipeline);

	if (GLState::vs != vs)
	{
		GLState::vs = vs;
		glUseProgramStages(m_pipeline, GL_VERTEX_SHADER_BIT, vs);
	}

	if (GLState::gs != gs)
	{
		GLState::gs = gs;
		glUseProgramStages(m_pipeline, GL_GEOMETRY_SHADER_BIT, gs);
	}

#ifdef _DEBUG
	// In debug always sets the program. It allow to replace the program in apitrace easily.
	if (true)
#else
	if (GLState::ps != ps)
#endif
	{
		GLState::ps = ps;
		glUseProgramStages(m_pipeline, GL_FRAGMENT_SHADER_BIT, ps);
	}
}

void GSShaderOGL::BindPipeline(GLuint pipe)
{
	if (GLState::pipeline != pipe)
	{
		GLState::pipeline = pipe;
		glBindProgramPipeline(pipe);
	}

	if (GLState::program)
	{
		GLState::program = 0;
		glUseProgram(0);
	}
}

bool GSShaderOGL::ValidateShader(GLuint s)
{
	if (!m_debug_shader)
		return true;

	GLint status = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (status)
		return true;

	GLint log_length = 0;
	glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0)
	{
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
	if (!m_debug_shader)
		return true;

	GLint status = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &status);
	if (status)
		return true;

	GLint log_length = 0;
	glGetProgramiv(p, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0)
	{
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
	if (!m_debug_shader)
		return true;

	// FIXME: might be mandatory to validate the pipeline
	glValidateProgramPipeline(p);

	GLint status = 0;
	glGetProgramPipelineiv(p, GL_VALIDATE_STATUS, &status);
	if (status)
		return true;

	GLint log_length = 0;
	glGetProgramPipelineiv(p, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length > 0)
	{
		char* log = new char[log_length];
		glGetProgramPipelineInfoLog(p, log_length, NULL, log);
		fprintf(stderr, "%s", log);
		delete[] log;
	}
	fprintf(stderr, "\n");

	return false;
}

std::string GSShaderOGL::GenGlslHeader(const std::string& entry, GLenum type, const std::string& macro)
{
	std::string header;
	header = "#version 330 core\n";
	// Need GL version 420
	header += "#extension GL_ARB_shading_language_420pack: require\n";
	// Need GL version 410
	header += "#extension GL_ARB_separate_shader_objects: require\n";
	if (GLLoader::found_GL_ARB_shader_image_load_store)
	{
		// Need GL version 420
		header += "#extension GL_ARB_shader_image_load_store: require\n";
	}
	else
	{
		header += "#define DISABLE_GL42_image\n";
	}
	if (GLLoader::vendor_id_amd || GLLoader::vendor_id_intel)
		header += "#define BROKEN_DRIVER as_usual\n";

	// Stupid GL implementation (can't use GL_ES)
	// AMD/nvidia define it to 0
	// intel window don't define it
	// intel linux refuse to define it
	header += "#define pGL_ES 0\n";

	// Allow to puts several shader in 1 files
	switch (type)
	{
		case GL_VERTEX_SHADER:
			header += "#define VERTEX_SHADER 1\n";
			break;
		case GL_GEOMETRY_SHADER:
			header += "#define GEOMETRY_SHADER 1\n";
			break;
		case GL_FRAGMENT_SHADER:
			header += "#define FRAGMENT_SHADER 1\n";
			break;
		default:
			ASSERT(0);
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

	// Note it is better to separate header and source file to have the good line number
	// in the glsl compiler report
	const int shader_nb = 3;
	const char* sources[shader_nb];

	std::string header = GenGlslHeader(entry, type, macro_sel);

	sources[0] = header.c_str();
	sources[1] = m_common_header.data();
	sources[2] = glsl_h_code;

	program = glCreateShaderProgramv(type, shader_nb, sources);

	bool status = ValidateProgram(program);

	if (!status)
	{
		// print extra info
		fprintf(stderr, "%s (entry %s, prog %d) :", glsl_file.c_str(), entry.c_str(), program);
		fprintf(stderr, "\n%s", macro_sel.c_str());
		fprintf(stderr, "\n");
	}

	m_prog_to_delete.push_back(program);

	return program;
}

// Same as above but for not-separated build
GLuint GSShaderOGL::CompileShader(const std::string& glsl_file, const std::string& entry, GLenum type, const char* glsl_h_code, const std::string& macro_sel)
{
	ASSERT(glsl_h_code != NULL);

	GLuint shader = 0;

	// Note it is better to separate header and source file to have the good line number
	// in the glsl compiler report
	const int shader_nb = 3;
	const char* sources[shader_nb];

	std::string header = GenGlslHeader(entry, type, macro_sel);

	sources[0] = header.c_str();
	sources[1] = m_common_header.data();
	sources[2] = glsl_h_code;

	shader = glCreateShader(type);
	glShaderSource(shader, shader_nb, sources, NULL);
	glCompileShader(shader);

	bool status = ValidateShader(shader);

	if (!status)
	{
		// print extra info
		fprintf(stderr, "%s (entry %s, prog %d) :", glsl_file.c_str(), entry.c_str(), shader);
		fprintf(stderr, "\n%s", macro_sel.c_str());
		fprintf(stderr, "\n");
	}

	m_shad_to_delete.push_back(shader);

	return shader;
}

// This function will get the binary program. Normally it must be used a caching
// solution but Nvidia also incorporates the ASM dump. Asm is nice because it allow
// to have an overview of the program performance based on the instruction number
// Note: initially I was using cg offline compiler but it doesn't support latest
// GLSL improvement (unfortunately).
int GSShaderOGL::DumpAsm(const std::string& file, GLuint p)
{
	if (!GLLoader::vendor_id_nvidia)
		return 0;

	GLint binaryLength;
	glGetProgramiv(p, GL_PROGRAM_BINARY_LENGTH, &binaryLength);

	char* binary = new char[binaryLength + 4];
	GLenum binaryFormat;
	glGetProgramBinary(p, binaryLength, NULL, &binaryFormat, binary);

	FILE* outfile = fopen(file.c_str(), "w");
	ASSERT(outfile);

	// Search the magic number "!!"
	int asm_ = 0;
	while (asm_ < binaryLength && (binary[asm_] != '!' || binary[asm_ + 1] != '!'))
	{
		asm_ += 1;
	}

	int instructions = -1;
	if (asm_ < binaryLength)
	{
		// Now print asm as text
		char* asm_txt = strtok(&binary[asm_], "\n");
		while (asm_txt != NULL && (strncmp(asm_txt, "END", 3) || !strncmp(asm_txt, "ENDIF", 5)))
		{
			if (!strncmp(asm_txt, "OUT", 3) || !strncmp(asm_txt, "TEMP", 4) || !strncmp(asm_txt, "LONG", 4))
			{
				instructions = 0;
			}
			else if (instructions >= 0)
			{
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

	if (instructions < 0)
	{
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
