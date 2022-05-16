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

#include <memory>
#include <string>
#include <vector>

#include <lzma.h>
#include <zstd.h>

#define GEN_REG_ENUM_CLASS_CONTENT(ClassName, EntryName, Value) \
	EntryName = Value,

#define GEN_REG_GETNAME_CONTENT(ClassName, EntryName, Value) \
	case ClassName::EntryName: \
		return #EntryName;

#define GEN_REG_ENUM_CLASS_AND_GETNAME(Macro, ClassName, Type, DefaultString) \
	enum class ClassName : Type \
	{ \
		Macro(GEN_REG_ENUM_CLASS_CONTENT) \
	}; \
	static constexpr const char* GetName(ClassName reg) \
	{ \
		switch (reg) \
		{ \
			Macro(GEN_REG_GETNAME_CONTENT) default : return DefaultString; \
		} \
	}

namespace GSDumpTypes
{
	// clang-format off
#define DEF_GSType(X) \
	X(GSType, Transfer,  0) \
	X(GSType, VSync,     1) \
	X(GSType, ReadFIFO2, 2) \
	X(GSType, Registers, 3)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GSType, GSType, u8, "UnknownType")
#undef DEF_GSType

#define DEF_GSTransferPath(X) \
	X(GSTransferPath, Path1Old, 0) \
	X(GSTransferPath, Path2,    1) \
	X(GSTransferPath, Path3,    2) \
	X(GSTransferPath, Path1New, 3) \
	X(GSTransferPath, Dummy,    4)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GSTransferPath, GSTransferPath, u8, "UnknownPath")
#undef DEF_GSTransferPath

#define DEF_GIFFlag(X) \
	X(GIFFlag, PACKED,  0) \
	X(GIFFlag, REGLIST, 1) \
	X(GIFFlag, IMAGE,   2) \
	X(GIFFlag, IMAGE2,  3)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GIFFlag, GIFFlag, u8, "UnknownFlag")
#undef DEF_GifFlag

#define DEF_GIFReg(X) \
	X(GIFReg, PRIM,       0x00) \
	X(GIFReg, RGBAQ,      0x01) \
	X(GIFReg, ST,         0x02) \
	X(GIFReg, UV,         0x03) \
	X(GIFReg, XYZF2,      0x04) \
	X(GIFReg, XYZ2,       0x05) \
	X(GIFReg, TEX0_1,     0x06) \
	X(GIFReg, TEX0_2,     0x07) \
	X(GIFReg, CLAMP_1,    0x08) \
	X(GIFReg, CLAMP_2,    0x09) \
	X(GIFReg, FOG,        0x0a) \
	X(GIFReg, XYZF3,      0x0c) \
	X(GIFReg, XYZ3,       0x0d) \
	X(GIFReg, AD,         0x0e) \
	X(GIFReg, NOP,        0x0f) \
	X(GIFReg, TEX1_1,     0x14) \
	X(GIFReg, TEX1_2,     0x15) \
	X(GIFReg, TEX2_1,     0x16) \
	X(GIFReg, TEX2_2,     0x17) \
	X(GIFReg, XYOFFSET_1, 0x18) \
	X(GIFReg, XYOFFSET_2, 0x19) \
	X(GIFReg, PRMODECONT, 0x1a) \
	X(GIFReg, PRMODE,     0x1b) \
	X(GIFReg, TEXCLUT,    0x1c) \
	X(GIFReg, SCANMSK,    0x22) \
	X(GIFReg, MIPTBP1_1,  0x34) \
	X(GIFReg, MIPTBP1_2,  0x35) \
	X(GIFReg, MIPTBP2_1,  0x36) \
	X(GIFReg, MIPTBP2_2,  0x37) \
	X(GIFReg, TEXA,       0x3b) \
	X(GIFReg, FOGCOL,     0x3d) \
	X(GIFReg, TEXFLUSH,   0x3f) \
	X(GIFReg, SCISSOR_1,  0x40) \
	X(GIFReg, SCISSOR_2,  0x41) \
	X(GIFReg, ALPHA_1,    0x42) \
	X(GIFReg, ALPHA_2,    0x43) \
	X(GIFReg, DIMX,       0x44) \
	X(GIFReg, DTHE,       0x45) \
	X(GIFReg, COLCLAMP,   0x46) \
	X(GIFReg, TEST_1,     0x47) \
	X(GIFReg, TEST_2,     0x48) \
	X(GIFReg, PABE,       0x49) \
	X(GIFReg, FBA_1,      0x4a) \
	X(GIFReg, FBA_2,      0x4b) \
	X(GIFReg, FRAME_1,    0x4c) \
	X(GIFReg, FRAME_2,    0x4d) \
	X(GIFReg, ZBUF_1,     0x4e) \
	X(GIFReg, ZBUF_2,     0x4f) \
	X(GIFReg, BITBLTBUF,  0x50) \
	X(GIFReg, TRXPOS,     0x51) \
	X(GIFReg, TRXREG,     0x52) \
	X(GIFReg, TRXDIR,     0x53) \
	X(GIFReg, HWREG,      0x54) \
	X(GIFReg, SIGNAL,     0x60) \
	X(GIFReg, FINISH,     0x61) \
	X(GIFReg, LABEL,      0x62)
		GEN_REG_ENUM_CLASS_AND_GETNAME(DEF_GIFReg, GIFReg, u8, "UnknownReg")
#undef DEF_GIFReg
	// clang-format on

	template <typename Output, typename Input, typename std::enable_if<sizeof(Input) == sizeof(Output), bool>::type = true>
	static constexpr Output BitCast(Input input)
	{
		Output output;
		memcpy(&output, &input, sizeof(input));
		return output;
	}
	template <typename Output = u32>
	static constexpr Output GetBits(u64 value, u32 shift, u32 numbits)
	{
		return static_cast<Output>((value >> shift) & ((1ull << numbits) - 1));
	}

	template <typename Output = u32>
	static constexpr Output GetBits(u128 value, u32 shift, u32 numbits)
	{
		u64 outval = 0;
		if (shift == 0)
			outval = value.lo;
		else if (shift < 64)
			outval = (value.lo >> shift) | (value.hi << (64 - shift));
		else
			outval = value.hi >> (shift - 64);
		return static_cast<Output>(outval & ((1ull << numbits) - 1));
	}
	static constexpr const char* GetNameOneBit(u8 value, const char* zero, const char* one)
	{
		switch (value)
		{
			case 0:
				return zero;
			case 1:
				return one;
			default:
				return "UNKNOWN";
		}
	}
	static constexpr const char* GetNameBool(bool value)
	{
		return value ? "True" : "False";
	}

	static constexpr const char* GetNamePRIMPRIM(u8 prim)
	{
		switch (prim)
		{
			case 0:
				return "Point";
			case 1:
				return "Line";
			case 2:
				return "Line Strip";
			case 3:
				return "Triangle";
			case 4:
				return "Triangle Strip";
			case 5:
				return "Triangle Fan";
			case 6:
				return "Sprite";
			case 7:
				return "Invalid";
			default:
				return "UNKNOWN";
		}
	}
	static constexpr const char* GetNamePRIMIIP(u8 iip)
	{
		return GetNameOneBit(iip, "Flat Shading", "Gouraud Shading");
	}
	static constexpr const char* GetNamePRIMFST(u8 fst)
	{
		return GetNameOneBit(fst, "STQ Value", "UV Value");
	}
	static constexpr const char* GetNamePRIMCTXT(u8 ctxt)
	{
		return GetNameOneBit(ctxt, "Context 1", "Context 2");
	}
	static constexpr const char* GetNamePRIMFIX(u8 fix)
	{
		return GetNameOneBit(fix, "Unfixed", "Fixed");
	}
	static constexpr const char* GetNameTEXTCC(u8 tcc)
	{
		return GetNameOneBit(tcc, "RGB", "RGBA");
	}
	static constexpr const char* GetNameTEXTFX(u8 tfx)
	{
		switch (tfx)
		{
			case 0:
				return "Modulate";
			case 1:
				return "Decal";
			case 2:
				return "Highlight";
			case 3:
				return "Highlight2";
			default:
				return "UNKNOWN";
		}
	}
	static constexpr const char* GetNameTEXCSM(u8 csm)
	{
		return GetNameOneBit(csm, "CSM1", "CSM2");
	}
	static constexpr const char* GetNameTEXPSM(u8 psm)
	{
		switch (psm)
		{
			case 000:
				return "PSMCT32";
			case 001:
				return "PSMCT24";
			case 002:
				return "PSMCT16";
			case 012:
				return "PSMCT16S";
			case 023:
				return "PSMT8";
			case 024:
				return "PSMT4";
			case 033:
				return "PSMT8H";
			case 044:
				return "PSMT4HL";
			case 054:
				return "PSMT4HH";
			case 060:
				return "PSMZ32";
			case 061:
				return "PSMZ24";
			case 062:
				return "PSMZ16";
			case 072:
				return "PSMZ16S";
			default:
				return "UNKNOWN";
		}
	}
	static constexpr const char* GetNameTEXCPSM(u8 psm)
	{
		switch (psm)
		{
			case 000:
				return "PSMCT32";
			case 002:
				return "PSMCT16";
			case 012:
				return "PSMCT16S";
			default:
				return "UNKNOWN";
		}
	}
} // namespace GSDumpTypes

class GSDumpFile
{
public:
	struct GSData
	{
		GSDumpTypes::GSType id;
		const u8* data;
		size_t length;
		GSDumpTypes::GSTransferPath path;
	};

	using ByteArray = std::vector<u8>;
	using GSDataArray = std::vector<GSData>;

	virtual ~GSDumpFile();

	static std::unique_ptr<GSDumpFile> OpenGSDump(const char* filename, const char* repack_filename = nullptr);
	static bool GetPreviewImageFromDump(const char* filename, u32* width, u32* height, std::vector<u32>* pixels);

	__fi const std::string& GetSerial() const { return m_serial; }
	__fi u32 GetCRC() const { return m_crc; }

	__fi const ByteArray& GetRegsData() const { return m_regs_data; }
	__fi const ByteArray& GetStateData() const { return m_state_data; }
	__fi const GSDataArray& GetPackets() const { return m_dump_packets; }

	bool ReadFile();

protected:
	GSDumpFile(FILE* file, FILE* repack_file);

	virtual bool IsEof() = 0;
	virtual size_t Read(void* ptr, size_t size) = 0;

	void Repack(void* ptr, size_t size);

	FILE* m_fp = nullptr;

private:
	FILE* m_repack_fp = nullptr;

	std::string m_serial;
	u32 m_crc = 0;

	std::vector<u8> m_regs_data;
	std::vector<u8> m_state_data;
	std::vector<u8> m_packet_data;

	GSDataArray m_dump_packets;
};

class GSDumpLzma : public GSDumpFile
{
	lzma_stream m_strm;

	size_t m_buff_size;
	uint8_t* m_area;
	uint8_t* m_inbuf;

	size_t m_avail;
	size_t m_start;

	void Decompress();
	void Initialize();

public:
	GSDumpLzma(FILE* file, FILE* repack_file);
	virtual ~GSDumpLzma();

	bool IsEof() final;
	size_t Read(void* ptr, size_t size) final;
};

class GSDumpDecompressZst : public GSDumpFile
{
	static constexpr u32 INPUT_BUFFER_SIZE = 512 * _1kb;
	static constexpr u32 OUTPUT_BUFFER_SIZE = 2 * _1mb;

	ZSTD_DStream* m_strm;
	ZSTD_inBuffer m_inbuf;

	uint8_t* m_area;

	size_t m_avail;
	size_t m_start;

	void Decompress();
	void Initialize();

public:
	GSDumpDecompressZst(FILE* file, FILE* repack_file);
	virtual ~GSDumpDecompressZst();

	bool IsEof() final;
	size_t Read(void* ptr, size_t size) final;
};

class GSDumpRaw : public GSDumpFile
{
public:
	GSDumpRaw(FILE* file, FILE* repack_file);
	virtual ~GSDumpRaw() = default;

	bool IsEof() final;
	size_t Read(void* ptr, size_t size) final;
};
