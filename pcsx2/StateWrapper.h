// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "common/HeapArray.h"
#include "common/Pcsx2Defs.h"
#include <array>
#include <cstring>
#include <deque>
#include <string>
#include <type_traits>
#include <vector>

class String;

class StateWrapper
{
public:
	enum class Mode
	{
		Read,
		Write
	};

	// Only supports up to 4GB. More than enough.
	class IStream
	{
	public:
		virtual ~IStream();
		virtual u32 Read(void* buf, u32 count) = 0;
		virtual u32 Write(const void* buf, u32 count) = 0;
		virtual u32 GetPosition() = 0;
		virtual bool SeekAbsolute(u32 pos) = 0;
		virtual bool SeekRelative(s32 count) = 0;
	};

	class ReadOnlyMemoryStream : public IStream
	{
	public:
		ReadOnlyMemoryStream(const void* buf, u32 buf_length);

		u32 Read(void* buf, u32 count) override;
		u32 Write(const void* buf, u32 count) override;
		u32 GetPosition() override;
		bool SeekAbsolute(u32 pos) override;
		bool SeekRelative(s32 count) override;

	private:
		const u8* m_buf;
		u32 m_buf_length;
		u32 m_buf_position = 0;
	};

	class MemoryStream : public IStream
	{
	public:
		MemoryStream(void* buf, u32 buf_length);

		u32 Read(void* buf, u32 count) override;
		u32 Write(const void* buf, u32 count) override;
		u32 GetPosition() override;
		bool SeekAbsolute(u32 pos) override;
		bool SeekRelative(s32 count) override;

	private:
		u8* m_buf;
		u32 m_buf_length;
		u32 m_buf_position = 0;
	};

	class VectorMemoryStream : public IStream
	{
	public:
		VectorMemoryStream();
		VectorMemoryStream(u32 reserve);

		const std::vector<u8>& GetBuffer() const { return m_buf; }

		u32 Read(void* buf, u32 count) override;
		u32 Write(const void* buf, u32 count) override;
		u32 GetPosition() override;
		bool SeekAbsolute(u32 pos) override;
		bool SeekRelative(s32 count) override;

	private:
		void Expand(u32 new_size);

		std::vector<u8> m_buf;
		u32 m_buf_position = 0;
	};

public:
	StateWrapper(IStream* stream, Mode mode, u32 version);
	StateWrapper(const StateWrapper&) = delete;
	~StateWrapper();

	IStream* GetStream() const { return m_stream; }
	bool HasError() const { return m_error; }
	bool IsGood() const { return !m_error; }
	bool IsReading() const { return (m_mode == Mode::Read); }
	bool IsWriting() const { return (m_mode == Mode::Write); }
	Mode GetMode() const { return m_mode; }
	void SetMode(Mode mode) { m_mode = mode; }
	u32 GetVersion() const { return m_version; }

	/// Overload for integral or floating-point types. Writes bytes as-is.
	template <typename T, std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>, int> = 0>
	void Do(T* value_ptr)
	{
		if (m_mode == Mode::Read)
		{
			if (m_error || (m_error |= (m_stream->Read(value_ptr, sizeof(T)) != sizeof(T))) == true)
				*value_ptr = static_cast<T>(0);
		}
		else
		{
			if (!m_error)
				m_error |= (m_stream->Write(value_ptr, sizeof(T)) != sizeof(T));
		}
	}

	/// Overload for enum types. Uses the underlying type.
	template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
	void Do(T* value_ptr)
	{
		using TType = std::underlying_type_t<T>;
		if (m_mode == Mode::Read)
		{
			TType temp;
			if (m_error || (m_error |= (m_stream->Read(&temp, sizeof(TType)) != sizeof(T))) == true)
				temp = static_cast<TType>(0);

			*value_ptr = static_cast<T>(temp);
		}
		else
		{
			TType temp;
			std::memcpy(&temp, value_ptr, sizeof(TType));
			if (!m_error)
				m_error |= (m_stream->Write(&temp, sizeof(TType)) != sizeof(TType));
		}
	}

	/// Overload for POD types, such as structs.
	template <typename T, std::enable_if_t<std::is_standard_layout_v<T> && std::is_trivial_v<T>, int> = 0>
	void DoPOD(T* value_ptr)
	{
		if (m_mode == Mode::Read)
		{
			if (m_error || (m_error |= (m_stream->Read(value_ptr, sizeof(T)) != sizeof(T))) == true)
				std::memset(value_ptr, 0, sizeof(*value_ptr));
		}
		else
		{
			if (!m_error)
				m_error |= (m_stream->Write(value_ptr, sizeof(T)) != sizeof(T));
		}
	}

	template <typename T>
	void DoArray(T* values, size_t count)
	{
		for (size_t i = 0; i < count; i++)
			Do(&values[i]);
	}

	template <typename T>
	void DoPODArray(T* values, size_t count)
	{
		for (size_t i = 0; i < count; i++)
			DoPOD(&values[i]);
	}

	void DoBytes(void* data, size_t length);

	void Do(bool* value_ptr);
	void Do(std::string* value_ptr);
	void Do(String* value_ptr);

	template <typename T, size_t N>
	void Do(std::array<T, N>* data)
	{
		DoArray(data->data(), data->size());
	}

	template <typename T, size_t N, size_t A>
	void Do(FixedHeapArray<T, N, A>* data)
	{
		DoArray(data->data(), data->size());
	}

	template <typename T, size_t A>
	void Do(DynamicHeapArray<T, A>* data)
	{
		DoArray(data->data(), data->size());
	}

	template <typename T>
	void Do(std::vector<T>* data)
	{
		u32 length = static_cast<u32>(data->size());
		Do(&length);
		if (m_mode == Mode::Read)
			data->resize(length);
		DoArray(data->data(), data->size());
	}

	template <typename T>
	void Do(std::deque<T>* data)
	{
		u32 length = static_cast<u32>(data->size());
		Do(&length);
		if (m_mode == Mode::Read)
		{
			data->clear();
			for (u32 i = 0; i < length; i++)
			{
				T value;
				Do(&value);
				data->push_back(value);
			}
		}
		else
		{
			for (T& ch : *data)
				Do(&ch);
		}
	}

	template <typename T>
	T DoBitfield(T data)
	{
		Do(&data);
		return data;
	}

	bool DoMarker(const char* marker);

	template <typename T>
	void DoEx(T* data, u32 version_introduced, T default_value)
	{
		if (m_version < version_introduced)
		{
			*data = std::move(default_value);
			return;
		}

		Do(data);
	}

	void SkipBytes(u32 count)
	{
		if (m_mode != Mode::Read)
		{
			m_error = true;
			return;
		}

		if (!m_error)
			m_error = !m_stream->SeekRelative(static_cast<s32>(count));
	}

private:
	IStream* m_stream;
	Mode m_mode;
	u32 m_version;
	bool m_error = false;
};
