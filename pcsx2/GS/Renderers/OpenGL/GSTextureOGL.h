// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSTexture.h"

#include "glad.h"

class GSTextureOGL final : public GSTexture
{
private:
	GLuint m_texture_id = 0; // the texture id

	// Avoid alignment constrain
	//GSVector4i m_r;
	int m_r_x = 0;
	int m_r_y = 0;
	int m_r_w = 0;
	int m_r_h = 0;
	int m_layer = 0;
	u32 m_map_offset = 0;

	// internal opengl format/type/alignment
	GLenum m_int_format = 0;
	GLenum m_int_type = 0;
	u32 m_int_shift = 0;

public:
	explicit GSTextureOGL(Type type, int width, int height, int levels, Format format);
	~GSTextureOGL() override;

	__fi GLenum GetIntFormat() const { return m_int_format; }
	__fi GLenum GetIntType() const { return m_int_type; }
	__fi u32 GetIntShift() const { return m_int_shift; }

	void* GetNativeHandle() const override;

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void Unmap() override;
	void GenerateMipmap() override;

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif

	bool IsIntegerFormat() const
	{
		return (m_int_format == GL_RED_INTEGER || m_int_format == GL_RGBA_INTEGER);
	}
	bool IsUnsignedFormat() const
	{
		return (m_int_type == GL_UNSIGNED_BYTE || m_int_type == GL_UNSIGNED_SHORT || m_int_type == GL_UNSIGNED_INT);
	}

	__fi u32 GetID() { return m_texture_id; }
};

class GSDownloadTextureOGL final : public GSDownloadTexture
{
public:
	~GSDownloadTextureOGL() override;

	static std::unique_ptr<GSDownloadTextureOGL> Create(u32 width, u32 height, GSTexture::Format format);

	void CopyFromTexture(const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch) override;

	bool Map(const GSVector4i& read_rc) override;
	void Unmap() override;

	void Flush() override;

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif

private:
	GSDownloadTextureOGL(u32 width, u32 height, GSTexture::Format format);

	GLuint m_buffer_id = 0;
	u32 m_buffer_size = 0;

	GLsync m_sync = {};

	// used when buffer storage is not available
	u8* m_cpu_buffer = nullptr;
};
