// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/FileSystem.h"
#include "common/Error.h"

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <condition_variable>
#include <type_traits>

class Error;

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
	static constexpr s64 PACKET_OUT_OF_DATA = -1;
	static constexpr s64 PACKET_FAILURE     = -2;
	static constexpr s64 PACKET_EOF         = -3;
	static constexpr s64 PACKET_STOP        = -4;

	friend class GSDumpLazy;

	struct GSData
	{
		GSDumpTypes::GSType id;
		const u8* data;
		u32 length;
		GSDumpTypes::GSTransferPath path;
	};

	using ByteArray = std::vector<u8>;
	using GSDataArray = std::vector<GSData>;

	virtual ~GSDumpFile();

	static size_t GetPacketSize(const GSData& data);

	static std::unique_ptr<GSDumpFile> OpenGSDump(const char* filename, Error* error = nullptr);

	// We modify the GSDumpFile in place to avoid having to reallocate if frequently.
	// Warning: The GSDumpFile does not copy the data--it must be fully read before the
	// caller deallocates it. The dump should have been opened with this function to be reused with it.
	static void OpenGSDumpMemory(std::unique_ptr<GSDumpFile>& dump, const void* ptr, const size_t size);

	static bool OpenGSDumpLazy(std::unique_ptr<GSDumpLazy>& dump, size_t buffer_size, const char* filename, Error* error = nullptr);

	static bool GetPreviewImageFromDump(const char* filename, u32* width, u32* height, std::vector<u32>* pixels);

	__fi const std::string& GetSerial() const { return m_serial; }
	__fi u32 GetCRC() const { return m_crc; }

	__fi virtual const ByteArray& GetRegsData() const { return m_regs_data; }
	__fi virtual const ByteArray& GetStateData() const { return m_state_data; }
	__fi virtual const GSDataArray& GetPackets() const { return m_dump_packets; }
	__fi virtual s64 GetPacket(size_t i, GSData& data, Error* error = nullptr);
	__fi virtual size_t GetPacketsSize() const { return m_dump_packets.size(); }
	__fi virtual bool DonePackets(size_t i) const { return i >= m_dump_packets.size(); }

	bool ReadFile(Error* error);
	bool ReadFile(std::vector<u8>& dst, size_t max_size, Error* error);

	virtual s64 GetFileSize() = 0;
protected:
	GSDumpFile();

	bool ReadHeaderStateRegs(Error* error);
	bool ReadPackets(Error* error);
	bool ReadPacketData(Error* error);
	static s64 ReadOnePacket(u8* data_start, u8* data_end, GSData& packet, Error* error);

	virtual bool Open(FileSystem::ManagedCFilePtr fp, Error* error) = 0;
	virtual bool IsEof() = 0;
	virtual size_t Read(void* ptr, size_t size) = 0;
protected:
	FileSystem::ManagedCFilePtr m_fp;
	s64 m_size = -1;
	s64 m_size_compressed = -1;

private:
	std::string m_serial;
	u32 m_crc = 0;

	std::vector<u8> m_regs_data;
	std::vector<u8> m_state_data;
	std::vector<u8> m_packet_data;

	GSDataArray m_dump_packets;

	void Clear();
};

class GSStream
{
public:
	static constexpr s64 READ_EOF   = -1;
	static constexpr s64 READ_ERROR = -2;

	enum Type
	{
		RAW,
		LZMA,
		ZSTD
	};

protected:
	FileSystem::ManagedCFilePtr m_fp;
	size_t m_size_compressed = 0;

public:
	virtual ~GSStream() = default;
	virtual bool Open(FileSystem::ManagedCFilePtr fp, Error* error) = 0;
	virtual s64 Read(void* ptr, size_t size, Error* error) = 0;
	virtual bool IsEof() = 0;
	virtual Type GetType() = 0;
	size_t GetSizeCompressed() { return m_size_compressed; }

	static bool OpenStream(std::unique_ptr<GSStream>& stream, const char* filename, Error* error = nullptr);
};

// Initializes CRC tables used by LZMA SDK.
void GSInit7ZCRCTables();

// Must define outside class declaration for MSVC.
template <typename T>
concept GSDumpFileLoader_IsDstType = std::same_as<T, std::unique_ptr<GSDumpFile>> || std::same_as<T, std::vector<u8>>;

struct GSDumpFileLoader
{
	enum DumpState
	{
		WRITEABLE = 0,
		READABLE,
		DONE
	};

	enum ReturnValue
	{
		EMPTY,
		ERROR_,
		SUCCESS,
		FINISHED
	};

	struct DumpInfo
	{
		std::string filename;
		DumpState state = WRITEABLE;
		double load_time = 0.0;
		double block_time_read = 0.0;
		double block_time_write = 0.0;
		std::string error;
		std::vector<u8>* data = nullptr;
	};

	// Stays constant after construction.
	size_t num_threads = 0;
	size_t num_dumps_buffered = 0;
	size_t max_file_size = 0;
	std::vector<DumpInfo> dump_list; // Slots for consumers to acquire and work on.
	std::vector<std::vector<u8>> dumps_avail_list; // One vector for each dump being buffered.

	// Threads.
	std::vector<std::thread> threads;

	bool started = false; // Started flag. Only used by consumer.

	// Synchronization. 
	std::mutex mut;
	std::condition_variable cond_read; // For consumer to wait on.
	std::condition_variable cond_write; // For producer to wait on.

	// Following member should only be modified with the mutex held.
	size_t read = 0; // Read index.
	size_t write = 0; // Write index.
	bool stopped = false; // Stopped flag.
	
	// Stats. Modified by consumer with or without mutex.
	std::atomic<size_t> num_loaded = 0;
	std::atomic<size_t> num_errored = 0;

	GSDumpFileLoader(size_t nthreads = 1, size_t num_dumps_buffered = 1, size_t max_file_size = SIZE_MAX);
	~GSDumpFileLoader();

	void Start(const std::vector<std::string>& files, const std::string& from = "");

	template<typename T>
		requires GSDumpFileLoader_IsDstType<T>
	ReturnValue Get(T& dst, DumpInfo* info_out, bool block = true);
	void Stop();
	static void LoaderFunc(GSDumpFileLoader* parent);

	// Call any time by consumer.
	bool Started();
	bool Finished();
	size_t DumpsRemaining();
	void AddFile(const std::string& file);
	void SetMaxFileSize(std::size_t size);

	// Private - call only with mut locked.
	bool _Full();
	bool _Empty();
	bool _DoneWrite();
	bool _DoneRead();
	bool _Stopped();
	void _DebugCheck();

	// Unsafe
	void DebugPrint();
};

class GSDumpLazy final : public GSDumpFile
{
	friend class GSDumpFile;
	friend struct GSDumpFileLoaderLazy;

private:
	struct PacketInfo
	{
		GSData data;
		size_t packet_num;
		size_t buffer_start;
		size_t buffer_end;
	};

	static constexpr size_t pad_size = 4 * _1mb;

	const size_t buffer_size;

	std::thread thread;

	// State protected by mutex.
	mutable std::mutex mut;
	std::unique_ptr<GSStream> stream;
	std::string filename;
	std::string filename_next;
	std::vector<u8> buffer;
	size_t read_buffer = 0;
	size_t parse_buffer = 0;
	size_t write_buffer = 0;
	bool eof_stream = false;
	bool parse_error = false;
	Error error;
	bool reading_packet = false;
	std::vector<PacketInfo> packets;
	size_t read_packet = 0;
	size_t write_packet = 0;
	bool stop = false;

	// Condition variables.
	mutable std::condition_variable cond_read;
	mutable std::condition_variable cond_write;

	// Call only once before starting thread.
	void Init();

	bool OpenNext(const std::string& filename, Error* error);
	void Stop();
	bool DonePackets(size_t i) const override;

	__fi const GSDumpFile::ByteArray& GetRegsData() const override;
	__fi const GSDumpFile::ByteArray& GetStateData() const override;
	__fi const GSDumpFile::GSDataArray& GetPackets() const override;

	static void _LoaderFunc(GSDumpLazy* dump); // Private.

	// Helpers - call only with mutex locked.
	bool _OpenNext(const std::string& filename, Error* error, std::unique_lock<std::mutex>& lock);
	
	static bool _FillBytesRaw(std::unique_ptr<GSStream>& stream, std::vector<u8>& buffer,
		size_t buffer_size, size_t& _write_buffer, size_t& _read_buffer, Error* error); // Producer
	
	static size_t _CopyPadBytes(std::vector<u8>& buffer, size_t buffer_size, size_t _write_buffer, size_t _read_buffer);
	
	static bool _ParsePackets(std::vector<u8>& buffer, size_t buffer_size, std::vector<PacketInfo>& packets,
		size_t& _write_buffer, size_t& _read_buffer, size_t& _parse_buffer, size_t& _write_packet,
		size_t& _read_packet, size_t use_pad_size, bool eof_stream, Error* _error);
	
	void _ResetState();
	bool _Eof() const;
	bool _EofStream() const;
	bool _LoadCond() const;
	bool _FullBuffer() const;
	bool _FullPackets() const;
	bool _EmptyBuffer() const;
	bool _EmptyPackets() const;
	bool _NotLoading() const;
	bool _Stopped() const;
	bool _HasNext() const;
	void _DebugCheck() const;

public:
	GSDumpLazy(size_t buffer_size);
	~GSDumpLazy() override;

	bool Open(FileSystem::ManagedCFilePtr fp, Error* error) override;
	bool IsEof() override;
	size_t Read(void* ptr, size_t size) override;
	s64 GetFileSize() override;

	s64 GetPacket(size_t i, GSData& packet, Error* error = nullptr) override;
};

struct GSDumpFileLoaderLazy
{
	enum RetVal
	{
		SUCCESS,
		FAILURE,
		FULL,
		EMPTY
	};

	std::vector<std::unique_ptr<GSDumpLazy>> dumps;
	std::vector<std::string> filenames;
	size_t buffer_size;
	size_t read = 0;
	size_t write = 0;
	GSDumpLazy* reading_dump = nullptr;

	bool Started();
	void Start(size_t num_dumps, size_t buffer_size);
	bool Full();
	bool Empty();
	RetVal AddFile(const std::string& filename, Error* error);
	RetVal Get(std::unique_ptr<GSDumpFile>& dump, std::string& filename);
};